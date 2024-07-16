// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/vector_buffer.h"

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/copy_only_int.h"
#include "base/test/move_only_int.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

namespace {

class TRIVIAL_ABI TrivialAbiWithCountingOperations {
 public:
  TrivialAbiWithCountingOperations(int* destruction_counter, int* move_counter)
      : destruction_counter_(destruction_counter),
        move_counter_(move_counter) {}

  ~TrivialAbiWithCountingOperations() { ++*destruction_counter_; }

  // Copy construction and assignment should not be used.
  TrivialAbiWithCountingOperations(const TrivialAbiWithCountingOperations&) =
      delete;
  TrivialAbiWithCountingOperations& operator=(
      const TrivialAbiWithCountingOperations&) = delete;

  // Count how many times the move constructor is used.
  TrivialAbiWithCountingOperations(TrivialAbiWithCountingOperations&& rhs)
      : destruction_counter_(rhs.destruction_counter_),
        move_counter_(rhs.move_counter_) {
    ++*move_counter_;
  }

  // Move assignment should not be used.
  TrivialAbiWithCountingOperations& operator=(
      TrivialAbiWithCountingOperations&&) = delete;

 private:
  raw_ptr<int> destruction_counter_;
  raw_ptr<int> move_counter_;
};

}  // namespace

TEST(VectorBuffer, DeletePOD) {
  constexpr int size = 10;
  VectorBuffer<int> buffer(size);
  for (int i = 0; i < size; i++)
    buffer[i] = i + 1;

  VectorBuffer<int>::DestructRange(buffer.as_span());

  // Delete should do nothing.
  for (int i = 0; i < size; i++)
    EXPECT_EQ(i + 1, buffer[i]);
}

TEST(VectorBuffer, DeleteMoveOnly) {
  constexpr int size = 10;
  VectorBuffer<MoveOnlyInt> buffer(size);
  for (int i = 0; i < size; i++) {
    // SAFETY: `i < size`, and `size` is the buffer's allocation size, so
    // `begin() + i` is inside the buffer.
    new (UNSAFE_BUFFERS(buffer.begin() + i)) MoveOnlyInt(i + 1);
  }

  std::vector<int> destroyed_instances;
  auto scoped_callback_cleanup =
      MoveOnlyInt::SetScopedDestructionCallback(BindLambdaForTesting(
          [&](int value) { destroyed_instances.push_back(value); }));
  VectorBuffer<MoveOnlyInt>::DestructRange(buffer.as_span());

  EXPECT_THAT(destroyed_instances,
              ::testing::ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

TEST(VectorBuffer, PODMove) {
  constexpr int size = 10;
  VectorBuffer<int> dest(size);

  VectorBuffer<int> original(size);
  for (int i = 0; i < size; i++)
    original[i] = i + 1;

  VectorBuffer<int>::MoveConstructRange(original.as_span(), dest.as_span());
  for (int i = 0; i < size; i++)
    EXPECT_EQ(i + 1, dest[i]);
}

TEST(VectorBuffer, MovableMove) {
  constexpr int size = 10;
  VectorBuffer<MoveOnlyInt> dest(size);

  VectorBuffer<MoveOnlyInt> original(size);
  for (int i = 0; i < size; i++) {
    // SAFETY: `i < size`, and `size` is the buffer's allocation size, so
    // `begin() + i` is inside the buffer.
    new (UNSAFE_BUFFERS(original.begin() + i)) MoveOnlyInt(i + 1);
  }

  std::vector<int> destroyed_instances;
  auto scoped_callback_cleanup =
      MoveOnlyInt::SetScopedDestructionCallback(BindLambdaForTesting(
          [&](int value) { destroyed_instances.push_back(value); }));
  VectorBuffer<MoveOnlyInt>::MoveConstructRange(original.as_span(),
                                                dest.as_span());

  for (int i = 0; i < size; i++) {
    EXPECT_EQ(i + 1, dest[i].data());
  }
  // The original values were consumed, so when the original elements are
  // destroyed, the destruction callback should report 0.
  EXPECT_THAT(destroyed_instances,
              ::testing::ElementsAre(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
}

TEST(VectorBuffer, CopyToMove) {
  constexpr int size = 10;
  VectorBuffer<CopyOnlyInt> dest(size);

  VectorBuffer<CopyOnlyInt> original(size);
  for (int i = 0; i < size; i++) {
    // SAFETY: `i < size`, and `size` is the buffer's allocation size, so
    // `begin() + i` is inside the buffer.
    new (UNSAFE_BUFFERS(original.begin() + i)) CopyOnlyInt(i + 1);
  }

  std::vector<int> destroyed_instances;
  auto scoped_callback_cleanup =
      CopyOnlyInt::SetScopedDestructionCallback(BindLambdaForTesting(
          [&](int value) { destroyed_instances.push_back(value); }));
  VectorBuffer<CopyOnlyInt>::MoveConstructRange(original.as_span(),
                                                dest.as_span());

  for (int i = 0; i < size; i++) {
    EXPECT_EQ(i + 1, dest[i].data());
  }

  EXPECT_THAT(destroyed_instances,
              ::testing::ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

TEST(VectorBuffer, TrivialAbiMove) {
  // Currently trivial relocation doesn't work on Windows for some reason, so
  // the test needs to handle both cases.
  constexpr bool kHaveTrivialRelocation =
      IS_TRIVIALLY_RELOCATABLE(TrivialAbiWithCountingOperations);
  constexpr int size = 10;
  VectorBuffer<TrivialAbiWithCountingOperations> dest(size);

  int destruction_count = 0;
  int move_count = 0;
  VectorBuffer<TrivialAbiWithCountingOperations> original(size);
  for (int i = 0; i < size; i++) {
    // SAFETY: `i < size`, and `size` is the buffer's allocation size, so
    // `begin() + i` is inside the buffer.
    new (UNSAFE_BUFFERS(original.begin() + i))
        TrivialAbiWithCountingOperations(&destruction_count, &move_count);
  }

  VectorBuffer<TrivialAbiWithCountingOperations>::MoveConstructRange(
      original.as_span(), dest.as_span());

  // We expect the move to have been performed via memcpy, without calling move
  // constructors or destructors.
  EXPECT_EQ(destruction_count, kHaveTrivialRelocation ? 0 : size);
  EXPECT_EQ(move_count, kHaveTrivialRelocation ? 0 : size);

  dest.DestructRange(dest.as_span());
  EXPECT_EQ(destruction_count, kHaveTrivialRelocation ? size : size * 2);
  EXPECT_EQ(move_count, kHaveTrivialRelocation ? 0 : size);
}

}  // namespace base::internal
