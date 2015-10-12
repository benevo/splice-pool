#include "splice-pool.hpp"
#include "gtest/gtest.h"

namespace
{
    const std::size_t blockSize(20);
}

TEST(UniqueSemantics, AutoReleaseNode)
{
    splicer::ObjectPool<int> pool(blockSize);
    EXPECT_EQ(pool.allocated(), pool.available());

    {
        splicer::ObjectPool<int>::UniqueNodeType node(pool.acquireOne(42));
        ASSERT_TRUE(node.get());
        EXPECT_EQ(node->val(), 42);

        EXPECT_GE(pool.allocated(), blockSize);
        EXPECT_EQ(pool.available(), pool.allocated() - 1);

        node.reset();
        EXPECT_EQ(pool.available(), pool.allocated());
        ASSERT_FALSE(node.get());

        node = pool.acquireOne(314);

        ASSERT_TRUE(node.get());
        EXPECT_EQ(node->val(), 314);

        EXPECT_GE(pool.allocated(), blockSize);
        EXPECT_EQ(pool.available(), pool.allocated() - 1);
    }

    EXPECT_EQ(pool.available(), pool.allocated());
}

TEST(UniqueSemantics, AutoReleaseStack)
{
    splicer::ObjectPool<int> pool(blockSize);

    std::size_t count(blockSize * 2 + 1);

    {
        splicer::ObjectPool<int>::UniqueStackType stack(pool.acquire(count));

        ASSERT_EQ(stack.size(), count);
        EXPECT_EQ(pool.available(), pool.allocated() - count);

        // Pop a Node from the UniqueStack and release it.
        splicer::ObjectPool<int>::UniqueNodeType node(stack.pop());
        EXPECT_EQ(stack.size(), --count);
        EXPECT_TRUE(node.get());

        node.reset();

        EXPECT_FALSE(node.get());
        EXPECT_EQ(pool.available(), pool.allocated() - count);

        // Pop a UniqueStack from the first UniqueStack and release it.
        ASSERT_GT(count, 4);
        splicer::ObjectPool<int>::UniqueStackType other(stack.popStack(4));

        count -= 4;

        EXPECT_EQ(stack.size(), count);
        EXPECT_EQ(other.size(), 4);

        other.reset();

        EXPECT_EQ(pool.available(), pool.allocated() - count);
    }

    EXPECT_EQ(pool.available(), pool.allocated());
}

TEST(UniqueSemantics, Combinations)
{
    splicer::ObjectPool<int> pool(blockSize);

    // Merge separately acquired UniqueStack instances and release.
    {
        splicer::ObjectPool<int>::UniqueStackType one(pool.acquire(blockSize));
        splicer::ObjectPool<int>::UniqueStackType two(pool.acquire(blockSize));

        EXPECT_EQ(pool.available(), pool.allocated() - blockSize * 2);
        EXPECT_EQ(one.size(), blockSize);
        EXPECT_EQ(two.size(), blockSize);

        one.push(std::move(two));

        EXPECT_EQ(pool.available(), pool.allocated() - blockSize * 2);
        EXPECT_EQ(one.size(), blockSize * 2);
        EXPECT_EQ(two.size(), 0);
    }

    EXPECT_EQ(pool.available(), pool.allocated());

    // Merge separately acquired UniqueNode instances into a UniqueStack and
    // release.
    {
        splicer::ObjectPool<int>::UniqueNodeType one(pool.acquireOne(1));
        splicer::ObjectPool<int>::UniqueNodeType two(pool.acquireOne(2));

        EXPECT_EQ(pool.available(), pool.allocated() - 2);
        ASSERT_TRUE(one.get());
        ASSERT_TRUE(two.get());
        EXPECT_EQ(one->val(), 1);
        EXPECT_EQ(two->val(), 2);

        splicer::ObjectPool<int>::UniqueStackType stack(pool);
        stack.push(std::move(one));
        stack.push(std::move(two));

        EXPECT_FALSE(one.get());
        EXPECT_FALSE(two.get());
        EXPECT_EQ(stack.size(), 2);
    }

    EXPECT_EQ(pool.available(), pool.allocated());
}

TEST(UniqueSemantics, ManualRelease)
{
    splicer::ObjectPool<int> pool(blockSize);

    // Manual Stack release.
    {
        splicer::ObjectPool<int>::UniqueStackType one(pool.acquire(blockSize));
        splicer::ObjectPool<int>::UniqueStackType two(pool.acquire(blockSize));

        EXPECT_EQ(pool.available(), pool.allocated() - blockSize * 2);
        EXPECT_EQ(one.size(), blockSize);
        EXPECT_EQ(two.size(), blockSize);

        pool.release(std::move(one));

        EXPECT_EQ(pool.available(), pool.allocated() - blockSize);
        EXPECT_TRUE(one.empty());

        splicer::Stack<int> manual(two.release());

        EXPECT_TRUE(two.empty());
        EXPECT_EQ(manual.size(), blockSize);
        EXPECT_EQ(pool.available(), pool.allocated() - blockSize);

        pool.release(std::move(manual));
        EXPECT_EQ(pool.available(), pool.allocated());
    }

    // Manual Node release.
    {
        splicer::ObjectPool<int>::UniqueNodeType one(pool.acquireOne(1));
        splicer::ObjectPool<int>::UniqueNodeType two(pool.acquireOne(2));

        EXPECT_EQ(pool.available(), pool.allocated() - 2);
        ASSERT_TRUE(one.get());
        ASSERT_TRUE(two.get());
        EXPECT_EQ(one->val(), 1);
        EXPECT_EQ(two->val(), 2);

        pool.release(std::move(one));

        EXPECT_EQ(pool.available(), pool.allocated() - 1);
        EXPECT_FALSE(one.get());

        splicer::Node<int>* manual(two.release());

        EXPECT_FALSE(two.get());
        ASSERT_TRUE(manual);
        EXPECT_EQ(manual->val(), 2);

        pool.release(manual);

        EXPECT_EQ(pool.available(), pool.allocated());
    }
}
