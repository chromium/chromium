// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/state_bitmap.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

using TestBitmap = StateBitmap<kSuperPageSize, kSuperPageSize, kAlignment>;

class PageWithBitmap final {
 public:
  PageWithBitmap()
      : base_(base::AllocPages(nullptr,
                               kSuperPageSize,
                               kSuperPageAlignment,
                               PageReadWrite,
                               PageTag::kPartitionAlloc)),
        bitmap_(new (base_) TestBitmap) {}

  PageWithBitmap(const PageWithBitmap&) = delete;
  PageWithBitmap& operator=(const PageWithBitmap&) = delete;

  ~PageWithBitmap() { base::FreePages(base_, kSuperPageSize); }

  TestBitmap& bitmap() const { return *bitmap_; }

  void* base() const { return base_; }
  size_t size() const { return kSuperPageSize; }

  void* base_;
  TestBitmap* bitmap_;
};

class PartitionAllocStateBitmapTest : public ::testing::Test {
 protected:
  TestBitmap& bitmap() const { return page.bitmap(); }

  void AllocateObject(size_t object_position) {
    page.bitmap().Allocate(ObjectAddress(object_position));
  }

  void FreeObject(size_t object_position) {
    page.bitmap().Free(ObjectAddress(object_position));
  }

  bool QuarantineObject(size_t object_position, size_t epoch) {
    return page.bitmap().Quarantine(ObjectAddress(object_position), epoch);
  }

  void MarkQuarantinedObject(size_t object_position) {
    page.bitmap().MarkQuarantined(ObjectAddress(object_position));
  }

  bool IsAllocated(size_t object_position) const {
    return page.bitmap().IsAllocated(ObjectAddress(object_position));
  }

  bool IsQuarantined(size_t object_position) const {
    return page.bitmap().IsQuarantined(ObjectAddress(object_position));
  }

  bool IsFreed(size_t object_position) const {
    return page.bitmap().IsFreed(ObjectAddress(object_position));
  }

  void AssertAllocated(size_t object_position) const {
    EXPECT_TRUE(IsAllocated(object_position));
    EXPECT_FALSE(IsQuarantined(object_position));
    EXPECT_FALSE(IsFreed(object_position));
  }

  void AssertFreed(size_t object_position) const {
    EXPECT_FALSE(IsAllocated(object_position));
    EXPECT_FALSE(IsQuarantined(object_position));
    EXPECT_TRUE(IsFreed(object_position));
  }

  void AssertQuarantined(size_t object_position) const {
    EXPECT_FALSE(IsAllocated(object_position));
    EXPECT_TRUE(IsQuarantined(object_position));
    EXPECT_FALSE(IsFreed(object_position));
  }

  size_t CountAllocated() const {
    size_t count = 0;
    bitmap().IterateAllocated([&count](uintptr_t) { count++; });
    return count;
  }

  size_t CountQuarantined() const {
    size_t count = 0;
    bitmap().IterateQuarantined([&count](uintptr_t) { count++; });
    return count;
  }

  bool IsQuarantineEmpty() const { return !CountQuarantined(); }

  uintptr_t ObjectAddress(size_t pos) const {
    return reinterpret_cast<uintptr_t>(page.base()) + sizeof(TestBitmap) +
           pos * kAlignment;
  }

  static constexpr uintptr_t LastIndex() {
    return TestBitmap::kMaxEntries - (sizeof(TestBitmap) / kAlignment) - 1;
  }

  static constexpr uintptr_t MiddleIndex() { return LastIndex() / 2; }

 private:
  PageWithBitmap page;
};

constexpr size_t kTestEpoch = 0;

}  // namespace

TEST_F(PartitionAllocStateBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = TestBitmap::kMaxEntries;
  EXPECT_LT(0u, max_entries);
}

TEST_F(PartitionAllocStateBitmapTest, InitialQuarantineEmpty) {
  EXPECT_TRUE(IsQuarantineEmpty());
}

TEST_F(PartitionAllocStateBitmapTest, QuarantineImpliesNonEmpty) {
  AllocateObject(0);
  EXPECT_TRUE(IsQuarantineEmpty());
  QuarantineObject(0, kTestEpoch);
  EXPECT_FALSE(IsQuarantineEmpty());
}

TEST_F(PartitionAllocStateBitmapTest, RepetitiveQuarantine) {
  AllocateObject(MiddleIndex());
  EXPECT_TRUE(QuarantineObject(MiddleIndex(), kTestEpoch));
  EXPECT_FALSE(QuarantineObject(MiddleIndex(), kTestEpoch));
}

TEST_F(PartitionAllocStateBitmapTest, CountAllocated) {
  AllocateObject(0);
  EXPECT_EQ(1u, CountAllocated());
  QuarantineObject(0, kTestEpoch);
  EXPECT_EQ(0u, CountAllocated());
}

TEST_F(PartitionAllocStateBitmapTest, StateTransititions) {
  for (auto i : {static_cast<uintptr_t>(0), static_cast<uintptr_t>(1),
                 LastIndex() - 1, LastIndex()}) {
    AssertFreed(i);

    AllocateObject(i);
    AssertAllocated(i);

    QuarantineObject(i, kTestEpoch);
    AssertQuarantined(i);

    MarkQuarantinedObject(i);
    AssertQuarantined(i);

    FreeObject(i);
    AssertFreed(i);
  }
}

TEST_F(PartitionAllocStateBitmapTest, QuarantineFreeMultipleObjects) {
  static constexpr size_t kCount = 256;
  for (size_t i = 0; i < kCount; ++i) {
    AllocateObject(i);
  }
  EXPECT_EQ(kCount, CountAllocated());
  EXPECT_EQ(0u, CountQuarantined());

  for (size_t i = 0; i < kCount; ++i) {
    QuarantineObject(i, kTestEpoch);
  }
  EXPECT_EQ(0u, CountAllocated());
  EXPECT_EQ(kCount, CountQuarantined());

  for (size_t i = 0; i < kCount; ++i) {
    FreeObject(i);
    EXPECT_EQ(kCount - i - 1, CountQuarantined());
  }
  EXPECT_TRUE(IsQuarantineEmpty());
}

TEST_F(PartitionAllocStateBitmapTest, AdjacentQuarantinedObjectsAtBegin) {
  AllocateObject(0);
  QuarantineObject(0, kTestEpoch);
  AllocateObject(1);
  QuarantineObject(1, kTestEpoch);

  EXPECT_FALSE(IsQuarantined(2));
  {
    size_t count = 0;
    this->bitmap().IterateQuarantined([&count, this](uintptr_t current) {
      if (count == 0) {
        EXPECT_EQ(ObjectAddress(0), current);
      } else if (count == 1) {
        EXPECT_EQ(ObjectAddress(1), current);
      }
      count++;
    });

    EXPECT_EQ(2u, count);
  }
  // Now mark only the first object.
  {
    MarkQuarantinedObject(0);

    size_t count = 0;
    this->bitmap().IterateUnmarkedQuarantined(
        kTestEpoch, [&count, this](uintptr_t current) {
          if (count == 0)
            EXPECT_EQ(ObjectAddress(1), current);
          count++;
        });

    EXPECT_EQ(1u, count);
  }
}

TEST_F(PartitionAllocStateBitmapTest, AdjacentQuarantinedObjectsAtMiddle) {
  AllocateObject(MiddleIndex());
  QuarantineObject(MiddleIndex(), kTestEpoch);
  AllocateObject(MiddleIndex() + 1);
  QuarantineObject(MiddleIndex() + 1, kTestEpoch);
  {
    size_t count = 0;
    this->bitmap().IterateQuarantined([&count, this](uintptr_t current) {
      if (count == 0) {
        EXPECT_EQ(ObjectAddress(MiddleIndex()), current);
      } else if (count == 1) {
        EXPECT_EQ(ObjectAddress(MiddleIndex() + 1), current);
      }
      count++;
    });

    EXPECT_EQ(2u, count);
  }
  // Now mark only the first object.
  {
    MarkQuarantinedObject(MiddleIndex());

    size_t count = 0;
    this->bitmap().IterateUnmarkedQuarantined(
        kTestEpoch, [&count, this](uintptr_t current) {
          if (count == 0)
            EXPECT_EQ(ObjectAddress(MiddleIndex() + 1), current);
          count++;
        });

    EXPECT_EQ(1u, count);
  }
}

TEST_F(PartitionAllocStateBitmapTest, AdjacentQuarantinedObjectsAtEnd) {
  AllocateObject(LastIndex());
  QuarantineObject(LastIndex(), kTestEpoch);
  AllocateObject(LastIndex() - 1);
  QuarantineObject(LastIndex() - 1, kTestEpoch);

  EXPECT_FALSE(IsQuarantined(LastIndex() - 2));
  {
    size_t count = 0;
    this->bitmap().IterateQuarantined([&count, this](uintptr_t current) {
      if (count == 0) {
        EXPECT_EQ(ObjectAddress(LastIndex() - 1), current);
      } else if (count == 1) {
        EXPECT_EQ(ObjectAddress(LastIndex()), current);
      }
      count++;
    });

    EXPECT_EQ(2u, count);
  }
  // Now mark only the first object.
  {
    MarkQuarantinedObject(LastIndex());

    size_t count = 0;
    this->bitmap().IterateUnmarkedQuarantined(
        kTestEpoch, [&count, this](uintptr_t current) {
          if (count == 0)
            EXPECT_EQ(ObjectAddress(LastIndex() - 1), current);
          count++;
        });

    EXPECT_EQ(1u, count);
  }
}

}  // namespace internal
}  // namespace base
