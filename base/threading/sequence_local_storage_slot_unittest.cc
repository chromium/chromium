// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/threading/sequence_local_storage_slot.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/sequence_local_storage_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

template <class T>
class SequenceLocalStorageSlotTest : public testing::Test {
 public:
  SequenceLocalStorageSlotTest(const SequenceLocalStorageSlotTest&) = delete;
  SequenceLocalStorageSlotTest& operator=(const SequenceLocalStorageSlotTest&) =
      delete;

 protected:
  SequenceLocalStorageSlotTest()
      : scoped_sequence_local_storage_(&sequence_local_storage_) {}

  internal::SequenceLocalStorageMap sequence_local_storage_;
  internal::ScopedSetSequenceLocalStorageMapForCurrentThread
      scoped_sequence_local_storage_;
};

}  // namespace

struct GenericSLS {
  template <class T>
  using Type = GenericSequenceLocalStorageSlot<T>;
};

struct SmallSLS {
  template <class T>
  using Type = GenericSequenceLocalStorageSlot<T>;
};

using StorageTypes = testing::Types<GenericSLS, SmallSLS>;
TYPED_TEST_SUITE(SequenceLocalStorageSlotTest, StorageTypes);

// Verify that a value stored with emplace() can be retrieved with operator*().
TYPED_TEST(SequenceLocalStorageSlotTest, GetEmplace) {
  using SLSType = typename TypeParam::template Type<int>;
  SLSType slot;
  slot.emplace(5);
  EXPECT_EQ(*slot, 5);
}

// Verify that inserting an object in a SequenceLocalStorageSlot creates a copy
// of that object independent of the original one.
TYPED_TEST(SequenceLocalStorageSlotTest, EmplaceObjectIsIndependent) {
  using SLSType = typename TypeParam::template Type<bool>;
  bool should_be_false = false;

  SLSType slot;

  slot.emplace(should_be_false);

  EXPECT_FALSE(*slot);
  *slot = true;
  EXPECT_TRUE(*slot);

  EXPECT_NE(should_be_false, *slot);
}

// Verify that multiple slots work and that calling emplace after overwriting
// a value in a slot yields the new value.
TYPED_TEST(SequenceLocalStorageSlotTest, GetEmplaceMultipleSlots) {
  using SLSType = typename TypeParam::template Type<int>;
  SLSType slot1;
  SLSType slot2;
  SLSType slot3;
  EXPECT_FALSE(slot1);
  EXPECT_FALSE(slot2);
  EXPECT_FALSE(slot3);

  slot1.emplace(1);
  slot2.emplace(2);
  slot3.emplace(3);

  EXPECT_TRUE(slot1);
  EXPECT_TRUE(slot2);
  EXPECT_TRUE(slot3);
  EXPECT_EQ(*slot1, 1);
  EXPECT_EQ(*slot2, 2);
  EXPECT_EQ(*slot3, 3);

  slot3.emplace(4);
  slot2.emplace(5);
  slot1.emplace(6);

  EXPECT_EQ(*slot3, 4);
  EXPECT_EQ(*slot2, 5);
  EXPECT_EQ(*slot1, 6);
}

// Verify that changing the value returned by Get() changes the value
// in sequence local storage.
TYPED_TEST(SequenceLocalStorageSlotTest, GetReferenceModifiable) {
  using SLSType = typename TypeParam::template Type<bool>;
  SLSType slot;
  slot.emplace(false);
  *slot = true;
  EXPECT_TRUE(*slot);
}

// Verify that a move-only type can be stored in sequence local storage.
TYPED_TEST(SequenceLocalStorageSlotTest, EmplaceGetWithMoveOnlyType) {
  struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    int x = 0x12345678;
  };
  using SLSType = typename TypeParam::template Type<MoveOnly>;
  MoveOnly move_only;

  SLSType slot;
  slot.emplace(std::move(move_only));

  EXPECT_EQ(slot->x, 0x12345678);
}

// Verify that a Get() without a previous Set() on a slot returns a
// default-constructed value.
TYPED_TEST(SequenceLocalStorageSlotTest, GetWithoutSetDefaultConstructs) {
  struct DefaultConstructable {
    int x = 0x12345678;
  };
  using SLSType = typename TypeParam::template Type<DefaultConstructable>;

  SLSType slot;

  EXPECT_EQ(slot.GetOrCreateValue().x, 0x12345678);
}

// Verify that a GetOrCreateValue() without a previous emplace() on a slot with
// a POD-type returns a default-constructed value.
// Note: this test could be flaky and give a false pass. If it's flaky, the test
// might've "passed" because the memory for the slot happened to be zeroed.
TYPED_TEST(SequenceLocalStorageSlotTest, GetWithoutSetDefaultConstructsPOD) {
  using SLSType = typename TypeParam::template Type<void*>;
  SLSType slot;

  EXPECT_EQ(slot.GetOrCreateValue(), nullptr);
}

// Verify that the value of a slot is specific to a SequenceLocalStorageMap
TEST(SequenceLocalStorageSlotMultipleMapTest, EmplaceGetMultipleMapsOneSlot) {
  SequenceLocalStorageSlot<unsigned int> slot;
  internal::SequenceLocalStorageMap sequence_local_storage_maps[5];

  // Set the value of the slot to be the index of the current
  // SequenceLocalStorageMaps in the vector
  for (unsigned int i = 0; i < std::size(sequence_local_storage_maps); ++i) {
    internal::ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage(&sequence_local_storage_maps[i]);

    slot.emplace(i);
  }

  for (unsigned int i = 0; i < std::size(sequence_local_storage_maps); ++i) {
    internal::ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage(&sequence_local_storage_maps[i]);

    EXPECT_EQ(*slot, i);
  }
}

TEST(SequenceLocalStorageComPtrTest,
     TestClassesWithNoAddressOfOperatorCanCompile) {
  internal::SequenceLocalStorageMap sequence_local_storage_map;
  internal::ScopedSetSequenceLocalStorageMapForCurrentThread
      scoped_sequence_local_storage(&sequence_local_storage_map);
  // Microsoft::WRL::ComPtr overrides & operator to release the underlying
  // pointer.
  // https://learn.microsoft.com/en-us/cpp/cppcx/wrl/comptr-class?view=msvc-170#operator-ampersand
  // Types stored in SequenceLocalStorage may override `operator&` to have
  // additional side effects, e.g. Microsoft::WRL::ComPtr. Make sure
  // SequenceLocalStorage does not invoke/use custom `operator&`s to avoid
  // triggering those side effects.
  class TestNoAddressOfOperator {
   public:
    TestNoAddressOfOperator() = default;
    ~TestNoAddressOfOperator() {
      // Define a non-trivial destructor so that SequenceLocalStorageSlot
      // will use the external value path.
    }
    // See note above class definition for the reason this operator is deleted.
    TestNoAddressOfOperator* operator&() = delete;
  };
  SequenceLocalStorageSlot<TestNoAddressOfOperator> slot;
  slot.emplace(TestNoAddressOfOperator());
  EXPECT_NE(slot.GetValuePointer(), nullptr);
}

}  // namespace base
