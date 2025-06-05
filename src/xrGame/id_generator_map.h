#pragma once

template <
	typename TIME_ID,                      //u32
	typename TYPE_ID,                      //u8 - Block Member ID type
	typename VALUE_ID,                     //_OBJECT_ID
	typename BLOCK_ID,                     //u8
	typename CHUNK_ID,                     //u16 - Block Size&Count type
	VALUE_ID tMinValue,                    //_OBJECT_ID(0)
	VALUE_ID tMaxValue,                    //_OBJECT_ID(-2)
	CHUNK_ID tBlockSize,                   //u16(256)
	VALUE_ID tInvalidValueID = tMaxValue,  //_OBJECT_ID(-1)
	TIME_ID tStartTime = 0                 //u32(0)
>
class CID_Generator
{
private:
	struct SID_Block
	{
		CHUNK_ID m_tCount;
		TIME_ID m_tTimeID;
		TYPE_ID m_tpIDs[tBlockSize];

		IC SID_Block() : m_tCount(0)
		{
		}

		IC SID_Block(TIME_ID tTime, BOOL reverse)
		{
			for (m_tCount = 0; m_tCount < tBlockSize; m_tCount++)
				m_tpIDs[m_tCount] = TYPE_ID(m_tCount);
			if (reverse)
				std::reverse(m_tpIDs, m_tpIDs + m_tCount);
			m_tTimeID = tTime;
		}

		IC bool operator<(const SID_Block& b) const
		{
			return (m_tCount && ((m_tTimeID < b.m_tTimeID) || !b.m_tCount));
		}
	};

	enum
	{
		m_tBlockCount = u32(tMaxValue - tMinValue) / tBlockSize + 1,
	};

	u32 m_available_count;
	typedef xr_hash_map<u32, SID_Block> blocks_hash_map;
	blocks_hash_map m_tppBlocks;

	IC BLOCK_ID tfGetBlockByValue(VALUE_ID tValueID)
	{
		BLOCK_ID l_tBlockID = BLOCK_ID((tValueID - tMinValue) / tBlockSize);
		R_ASSERT2(l_tBlockID < m_tBlockCount, "Requesting ID is invalid!");
		return (l_tBlockID);
	}

	IC VALUE_ID tfGetFromBlock(SID_Block& l_tID_Block, BLOCK_ID l_tBlockID, VALUE_ID tValueID)
	{
		VERIFY(l_tID_Block.m_tCount);
		//BLOCK_ID l_tBlockID = BLOCK_ID(&l_tID_Block - m_tppBlocks);

		if (l_tID_Block.m_tCount == 1)
		{
			--m_available_count;
			VERIFY(m_available_count >= 0);
		}

		if (tInvalidValueID == tValueID)
			return (VALUE_ID(l_tID_Block.m_tpIDs[--l_tID_Block.m_tCount]) + l_tBlockID * tBlockSize + tMinValue);

		TYPE_ID* l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount,
			TYPE_ID((tValueID - tMinValue) % tBlockSize));
		R_ASSERT2(l_tID_Block.m_tpIDs + l_tID_Block.m_tCount != l_tpBlockID, "Requesting ID has already been used!");
		*l_tpBlockID = *(l_tID_Block.m_tpIDs + --l_tID_Block.m_tCount);
		return (tValueID);
	}

	IC BOOL fExist(BLOCK_ID l_tBlockID)
	{
		if (m_tppBlocks.find(l_tBlockID) == m_tppBlocks.end())
			return FALSE;
		return TRUE;
	}

	IC typename blocks_hash_map::iterator fGetMinBlockIt()
	{
		if (m_tppBlocks.empty())
		{
			m_tppBlocks[0] = SID_Block(tStartTime, TRUE);
			return m_tppBlocks.begin();
		}

		u32 minBlockID = 0;
		for (; minBlockID < m_tBlockCount; minBlockID++)
		{
			if (!fExist(minBlockID))
				break;
		}

		blocks_hash_map::iterator minIt = m_tppBlocks.begin();
		blocks_hash_map::iterator E = m_tppBlocks.end();
		blocks_hash_map::iterator It = m_tppBlocks.begin();
		for (++It; It != E; It++)
		{
			if ((It->first < minBlockID) && (It->second < minIt->second))
			{
				minBlockID = It->first;
				minIt = It;
			}
		}

		if (!fExist(minBlockID))
		{
			m_tppBlocks[minBlockID] = SID_Block(tStartTime, TRUE);
			minIt = m_tppBlocks.find(minBlockID);
		}
		return minIt;
	}

public:
	IC CID_Generator()
	{
		m_available_count = m_tBlockCount - m_tppBlocks.size();
		blocks_hash_map::iterator It = m_tppBlocks.begin();
		blocks_hash_map::iterator E = m_tppBlocks.end();
		u32 block_firstID;
		SID_Block* b;
		for (; It != E; It++)
		{
			block_firstID = It->first * tBlockSize + tMinValue;
			for (u32 id = block_firstID; id < block_firstID + tBlockSize; id++)
				vfFreeID(id, tStartTime);

			b = &(It->second);
			std::reverse(b, b + b->m_tCount);
		}
		VERIFY(m_available_count == m_tBlockCount);
	}

	IC VALUE_ID tfGetID(VALUE_ID tValueID = tInvalidValueID)
	{
		if (tInvalidValueID != tValueID)
		{
			BLOCK_ID l_tBlockID = tfGetBlockByValue(tValueID);
			if (!fExist(l_tBlockID))
				m_tppBlocks[l_tBlockID] = SID_Block(tStartTime, TRUE);
			return tfGetFromBlock(m_tppBlocks.at(l_tBlockID), l_tBlockID, tValueID);
		}

		R_ASSERT2(m_available_count, "Not enough IDs");
		blocks_hash_map::iterator minBlockIt = fGetMinBlockIt();

		return (tfGetFromBlock(minBlockIt->second, minBlockIt->first, tValueID));
	}

	IC void vfFreeID(VALUE_ID tValueID, TIME_ID tTimeID)
	{
		BLOCK_ID l_tBlockID = tfGetBlockByValue(tValueID);
		SID_Block& l_tID_Block = m_tppBlocks[l_tBlockID];

		VERIFY(l_tID_Block.m_tCount < tBlockSize);

		if (!l_tID_Block.m_tCount)
		{
			++m_available_count;
			VERIFY(m_available_count <= m_tBlockCount);
		}

#ifdef DEBUG
		TYPE_ID* l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount, TYPE_ID((tValueID - tMinValue) % tBlockSize));
		VERIFY(l_tpBlockID == l_tID_Block.m_tpIDs + l_tID_Block.m_tCount);
#endif
		l_tID_Block.m_tpIDs[l_tID_Block.m_tCount++] = TYPE_ID((tValueID - tMinValue) % tBlockSize);
		l_tID_Block.m_tTimeID = tTimeID;
	}
};
