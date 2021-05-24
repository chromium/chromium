// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/object_bitmap.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

using TestBitmap = ObjectBitmap<kSuperPageSize, kSuperPageSize, kAlignment>;
static_assert((kSuperPageSize / (kAlignment * CHAR_BIT)) == sizeof(TestBitmap),
              "Bitmap size must only depend on object alignment");

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

using AccessType = TestBitmap::AccessType;

// Wrap access types into types so that they can be used with typed gtests.
struct AtomicAccess {
  static constexpr AccessType value = AccessType::kAtomic;
};
struct NonAtomicAccess {
  static constexpr AccessType value = AccessType::kNonAtomic;
};

template <typename Access>
class PartitionAllocObjectBitmapTest : public ::testing::Test {
 protected:
  static constexpr AccessType kAccessType = Access::value;

  TestBitmap& bitmap() const { return page.bitmap(); }

  void SetBitForObject(size_t object_position) {
    page.bitmap().SetBit<kAccessType>(ObjectAddress(object_position));
  }

  void ClearBitForObject(size_t object_position) {
    page.bitmap().ClearBit<kAccessType>(ObjectAddress(object_position));
  }

  bool CheckBitForObject(size_t object_position) const {
    return page.bitmap().CheckBit<kAccessType>(ObjectAddress(object_position));
  }

  bool IsEmpty() const {
    size_t count = 0;
    bitmap().template Iterate<kAccessType>([&count](uintptr_t) { count++; });
    return count == 0;
  }

  uintptr_t ObjectAddress(size_t pos) const {
    return reinterpret_cast<uintptr_t>(page.base()) + sizeof(TestBitmap) +
           pos * kAlignment;
  }

  static constexpr uintptr_t LastIndex() {
    return TestBitmap::kMaxEntries - (sizeof(TestBitmap) / kAlignment) - 1;
  }

 private:
  PageWithBitmap page;
};

}  // namespace

using AccessTypes = ::testing::Types<AtomicAccess, NonAtomicAccess>;
TYPED_TEST_SUITE(PartitionAllocObjectBitmapTest, AccessTypes);

TYPED_TEST(PartitionAllocObjectBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = TestBitmap::kMaxEntries;
  EXPECT_LT(0u, max_entries);
}

TYPED_TEST(PartitionAllocObjectBitmapTest, InitialEmpty) {
  EXPECT_TRUE(this->IsEmpty());
}

TYPED_TEST(PartitionAllocObjectBitmapTest, SetBitImpliesNonEmpty) {
  this->SetBitForObject(0);
  EXPECT_FALSE(this->IsEmpty());
}

TYPED_TEST(PartitionAllocObjectBitmapTest, SetBitCheckBit) {
  this->SetBitForObject(0);
  EXPECT_TRUE(this->CheckBitForObject(0));
}

TYPED_TEST(PartitionAllocObjectBitmapTest, SetBitClearbitCheckBit) {
  this->SetBitForObject(0);
  this->ClearBitForObject(0);
  EXPECT_FALSE(this->CheckBitForObject(0));
}

TYPED_TEST(PartitionAllocObjectBitmapTest, SetBitClearBitImpliesEmpty) {
  this->SetBitForObject(this->LastIndex());
  this->ClearBitForObject(this->LastIndex());
  EXPECT_TRUE(this->IsEmpty());
}

TYPED_TEST(PartitionAllocObjectBitmapTest, AdjacentObjectsAtBegin) {
  static constexpr AccessType kAccessType = TestFixture::kAccessType;

  this->SetBitForObject(0);
  this->SetBitForObject(1);
  EXPECT_FALSE(this->CheckBitForObject(3));
  size_t count = 0;
  this->bitmap().template Iterate<kAccessType>(
      [&count, this](uintptr_t current) {
        if (count == 0) {
          EXPECT_EQ(this->ObjectAddress(0), current);
        } else if (count == 1) {
          EXPECT_EQ(this->ObjectAddress(1), current);
        }
        count++;
      });
  EXPECT_EQ(2u, count);
}

TYPED_TEST(PartitionAllocObjectBitmapTest, AdjacentObjectsAtEnd) {
  static constexpr AccessType kAccessType = TestFixture::kAccessType;
  static const size_t last_entry_index = this->LastIndex();

  this->SetBitForObject(last_entry_index - 1);
  this->SetBitForObject(last_entry_index);
  EXPECT_FALSE(this->CheckBitForObject(last_entry_index - 2));
  size_t count = 0;
  this->bitmap().template Iterate<kAccessType>(
      [&count, this](uintptr_t current) {
        if (count == 0) {
          EXPECT_EQ(this->ObjectAddress(last_entry_index - 1), current);
        } else if (count == 1) {
          EXPECT_EQ(this->ObjectAddress(last_entry_index), current);
        }
        count++;
      });
  EXPECT_EQ(2u, count);
}

TYPED_TEST(PartitionAllocObjectBitmapTest, IterateAndClearBitmap) {
  static constexpr AccessType kAccessType = TestFixture::kAccessType;

  size_t expected_count = 0;
  for (size_t i = 0; i < this->LastIndex(); i += 2, ++expected_count) {
    this->SetBitForObject(i);
  }

  size_t actual_count = 0;
  this->bitmap().template IterateAndClear<kAccessType>(
      [&actual_count](uintptr_t current) { ++actual_count; });

  EXPECT_EQ(expected_count, actual_count);
  EXPECT_TRUE(this->IsEmpty());
}

}  // namespace internal
}  // namespace base
