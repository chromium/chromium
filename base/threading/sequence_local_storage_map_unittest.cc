// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_local_storage_map.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

constexpr int kSlotId = 1;

class TRIVIAL_ABI SetOnDestroy {
 public:
  SetOnDestroy(bool* was_destroyed_ptr)
      : was_destroyed_ptr_(was_destroyed_ptr) {
    DCHECK(was_destroyed_ptr_);
    DCHECK(!(*was_destroyed_ptr_));
  }

  SetOnDestroy(const SetOnDestroy&) = delete;
  SetOnDestroy& operator=(const SetOnDestroy&) = delete;

  SetOnDestroy(SetOnDestroy&& other) {
    swap(was_destroyed_ptr_, other.was_destroyed_ptr_);
  }
  SetOnDestroy& operator=(SetOnDestroy&& other) {
    swap(was_destroyed_ptr_, other.was_destroyed_ptr_);
    return *this;
  }

  ~SetOnDestroy() {
    if (!was_destroyed_ptr_) {
      return;
    }
    DCHECK(!(*was_destroyed_ptr_));
    *was_destroyed_ptr_ = true;
  }

 private:
  raw_ptr<bool> was_destroyed_ptr_;
};

template <typename T, typename... Args>
SequenceLocalStorageMap::ValueDestructorPair CreateExternalValueDestructorPair(
    Args... args) {
  internal::SequenceLocalStorageMap::ExternalValue value;
  value.emplace(new T(args...));
  auto* destructor =
      SequenceLocalStorageMap::MakeExternalDestructor<T,
                                                      std::default_delete<T>>();

  SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair{
      std::move(value), destructor};

  return value_destructor_pair;
}

template <typename T, typename... Args>
SequenceLocalStorageMap::ValueDestructorPair CreateInlineValueDestructorPair(
    Args... args) {
  internal::SequenceLocalStorageMap::InlineValue value;
  value.emplace<T>(args...);
  auto* destructor = SequenceLocalStorageMap::MakeInlineDestructor<T>();

  SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair{
      std::move(value), destructor};

  return value_destructor_pair;
}

}  // namespace

// Verify that setting a value in the SequenceLocalStorageMap, then getting
// it will yield the same value.
TEST(SequenceLocalStorageMapTest, SetGetExternal) {
  SequenceLocalStorageMap sequence_local_storage_map;
  ScopedSetSequenceLocalStorageMapForCurrentThread
      scoped_sequence_local_storage_map(&sequence_local_storage_map);

  SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
      CreateExternalValueDestructorPair<int>(5);

  sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));

  EXPECT_EQ(
      sequence_local_storage_map.Get(kSlotId)->external_value.value_as<int>(),
      5);
}

TEST(SequenceLocalStorageMapTest, SetGetInline) {
  SequenceLocalStorageMap sequence_local_storage_map;
  ScopedSetSequenceLocalStorageMapForCurrentThread
      scoped_sequence_local_storage_map(&sequence_local_storage_map);

  SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
      CreateInlineValueDestructorPair<int>(5);

  sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));

  EXPECT_EQ(
      sequence_local_storage_map.Get(kSlotId)->inline_value.value_as<int>(), 5);
}

// Verify that the destructor is called on a value stored in the
// SequenceLocalStorageMap when SequenceLocalStorageMap is destroyed.
TEST(SequenceLocalStorageMapTest, DestructorExternal) {
  bool set_on_destruction = false;

  {
    SequenceLocalStorageMap sequence_local_storage_map;
    ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage_map(&sequence_local_storage_map);

    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
        CreateExternalValueDestructorPair<SetOnDestroy>(&set_on_destruction);

    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));
  }

  EXPECT_TRUE(set_on_destruction);
}

// Verify that overwriting a value already in the SequenceLocalStorageMap
// calls value's destructor.
TEST(SequenceLocalStorageMapTest, DestructorCalledOnSetOverwriteExternal) {
  bool set_on_destruction = false;
  bool set_on_destruction2 = false;
  {
    SequenceLocalStorageMap sequence_local_storage_map;
    ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage_map(&sequence_local_storage_map);

    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
        CreateExternalValueDestructorPair<SetOnDestroy>(&set_on_destruction);
    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair2 =
        CreateExternalValueDestructorPair<SetOnDestroy>(&set_on_destruction2);

    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));

    ASSERT_FALSE(set_on_destruction);

    // Overwrites the old value in the slot.
    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair2));

    // Destructor should've been called for the old value in the slot, and not
    // yet called for the new value.
    EXPECT_TRUE(set_on_destruction);
    EXPECT_FALSE(set_on_destruction2);
  }
  EXPECT_TRUE(set_on_destruction2);
}

#if defined(__clang__) && HAS_ATTRIBUTE(trivial_abi)
#if !BUILDFLAG(IS_WIN)
// Test disabled on Windows due to
// https://github.com/llvm/llvm-project/issues/69394

TEST(SequenceLocalStorageMapTest, DestructorInline) {
  bool set_on_destruction = false;

  {
    SequenceLocalStorageMap sequence_local_storage_map;
    ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage_map(&sequence_local_storage_map);

    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
        CreateInlineValueDestructorPair<SetOnDestroy>(&set_on_destruction);

    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));
  }

  EXPECT_TRUE(set_on_destruction);
}

TEST(SequenceLocalStorageMapTest, DestructorCalledOnSetOverwriteInline) {
  bool set_on_destruction = false;
  bool set_on_destruction2 = false;
  {
    SequenceLocalStorageMap sequence_local_storage_map;
    ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_sequence_local_storage_map(&sequence_local_storage_map);

    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair =
        CreateInlineValueDestructorPair<SetOnDestroy>(&set_on_destruction);
    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair2 =
        CreateInlineValueDestructorPair<SetOnDestroy>(&set_on_destruction2);

    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair));

    ASSERT_FALSE(set_on_destruction);

    // Overwrites the old value in the slot.
    sequence_local_storage_map.Set(kSlotId, std::move(value_destructor_pair2));

    // Destructor should've been called for the old value in the slot, and not
    // yet called for the new value.
    EXPECT_TRUE(set_on_destruction);
    EXPECT_FALSE(set_on_destruction2);
  }
  EXPECT_TRUE(set_on_destruction2);
}

#else  // !BUILDFLAG(IS_WIN)

static_assert(!absl::is_trivially_relocatable<SetOnDestroy>(),
              "A compiler change on Windows indicates the preprocessor "
              "guarding the test above needs to be updated.");

#endif  // !BUILDFLAG(IS_WIN)
#endif  //  defined(__clang__) && HAS_ATTRIBUTE(trivial_abi)

}  // namespace internal
}  // namespace base
