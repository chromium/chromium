// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/object_bitmap.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

using TestBitmap = ObjectBitmap<kSuperPageSize, kSuperPageSize, kAlignment>;

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

class ObjectBitmapTest : public ::testing::Test {
 protected:
  TestBitmap& bitmap() const { return page.bitmap(); }

  void SetBitForObject(size_t object_position) {
    page.bitmap().SetBit(ObjectAddress(object_position));
  }

  void ClearBitForObject(size_t object_position) {
    page.bitmap().ClearBit(ObjectAddress(object_position));
  }

  bool CheckBitForObject(size_t object_position) const {
    return page.bitmap().CheckBit(ObjectAddress(object_position));
  }

  bool IsEmpty() const {
    size_t count = 0;
    bitmap().Iterate([&count](uintptr_t) { count++; });
    return count == 0;
  }

  uintptr_t ObjectAddress(size_t pos) const {
    return reinterpret_cast<uintptr_t>(page.base()) + sizeof(TestBitmap) +
           pos * kAlignment;
  }

  uintptr_t LastIndex() const {
    return TestBitmap::kMaxEntries - (sizeof(TestBitmap) / kAlignment) - 1;
  }

 private:
  PageWithBitmap page;
};

}  // namespace

TEST_F(ObjectBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = TestBitmap::kMaxEntries;
  EXPECT_LT(0u, max_entries);
}

TEST_F(ObjectBitmapTest, InitialEmpty) {
  EXPECT_TRUE(IsEmpty());
}

TEST_F(ObjectBitmapTest, SetBitImpliesNonEmpty) {
  SetBitForObject(0);
  EXPECT_FALSE(IsEmpty());
}

TEST_F(ObjectBitmapTest, SetBitCheckBit) {
  SetBitForObject(0);
  EXPECT_TRUE(CheckBitForObject(0));
}

TEST_F(ObjectBitmapTest, SetBitClearbitCheckBit) {
  SetBitForObject(0);
  ClearBitForObject(0);
  EXPECT_FALSE(CheckBitForObject(0));
}

TEST_F(ObjectBitmapTest, SetBitClearBitImpliesEmpty) {
  SetBitForObject(LastIndex());
  ClearBitForObject(LastIndex());
  EXPECT_TRUE(IsEmpty());
}

TEST_F(ObjectBitmapTest, AdjacentObjectsAtBegin) {
  SetBitForObject(0);
  SetBitForObject(1);
  EXPECT_FALSE(CheckBitForObject(3));
  size_t count = 0;
  bitmap().Iterate([&count, this](uintptr_t current) {
    if (count == 0) {
      EXPECT_EQ(ObjectAddress(0), current);
    } else if (count == 1) {
      EXPECT_EQ(ObjectAddress(1), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST_F(ObjectBitmapTest, AdjacentObjectsAtEnd) {
  static const size_t last_entry_index = LastIndex();
  SetBitForObject(last_entry_index - 1);
  SetBitForObject(last_entry_index);
  EXPECT_FALSE(CheckBitForObject(last_entry_index - 2));
  size_t count = 0;
  bitmap().Iterate([&count, this](uintptr_t current) {
    if (count == 0) {
      EXPECT_EQ(ObjectAddress(last_entry_index - 1), current);
    } else if (count == 1) {
      EXPECT_EQ(ObjectAddress(last_entry_index), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST_F(ObjectBitmapTest, FindElementSentinel) {
  EXPECT_EQ(TestBitmap::kSentinel,
            bitmap().FindPotentialObjectBeginning(ObjectAddress(654)));
}

TEST_F(ObjectBitmapTest, FindElementExact) {
  SetBitForObject(654);
  EXPECT_EQ(ObjectAddress(654),
            bitmap().FindPotentialObjectBeginning(ObjectAddress(654)));
}

TEST_F(ObjectBitmapTest, FindElementApproximate) {
  static const size_t kInternalDelta = 37;
  SetBitForObject(654);
  EXPECT_EQ(ObjectAddress(654), bitmap().FindPotentialObjectBeginning(
                                    ObjectAddress(654 + kInternalDelta)));
}

TEST_F(ObjectBitmapTest, FindElementIteratingWholeBitmap) {
  SetBitForObject(0);
  const uintptr_t hint_index = LastIndex();
  EXPECT_EQ(ObjectAddress(0),
            bitmap().FindPotentialObjectBeginning(ObjectAddress(hint_index)));
}

}  // namespace internal
}  // namespace base
