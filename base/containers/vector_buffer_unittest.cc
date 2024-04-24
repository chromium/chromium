// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/vector_buffer.h"

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/test/copy_only_int.h"
#include "base/test/move_only_int.h"
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
    buffer.begin()[i] = i + 1;

  buffer.DestructRange(buffer.begin(), buffer.end());

  // Delete should do nothing.
  for (int i = 0; i < size; i++)
    EXPECT_EQ(i + 1, buffer.begin()[i]);
}

TEST(VectorBuffer, DeleteMoveOnly) {
  constexpr int size = 10;
  VectorBuffer<MoveOnlyInt> buffer(size);
  for (int i = 0; i < size; i++)
    new (buffer.begin() + i) MoveOnlyInt(i + 1);

  buffer.DestructRange(buffer.begin(), buffer.end());

  // Delete should have reset all of the values to 0.
  for (int i = 0; i < size; i++)
    EXPECT_EQ(0, buffer.begin()[i].data());
}

TEST(VectorBuffer, PODMove) {
  constexpr int size = 10;
  VectorBuffer<int> dest(size);

  VectorBuffer<int> original(size);
  for (int i = 0; i < size; i++)
    original.begin()[i] = i + 1;

  original.MoveRange(original.begin(), original.end(), dest.begin());
  for (int i = 0; i < size; i++)
    EXPECT_EQ(i + 1, dest.begin()[i]);
}

TEST(VectorBuffer, MovableMove) {
  constexpr int size = 10;
  VectorBuffer<MoveOnlyInt> dest(size);

  VectorBuffer<MoveOnlyInt> original(size);
  for (int i = 0; i < size; i++)
    new (original.begin() + i) MoveOnlyInt(i + 1);

  original.MoveRange(original.begin(), original.end(), dest.begin());

  // Moving from a MoveOnlyInt resets to 0.
  for (int i = 0; i < size; i++) {
    EXPECT_EQ(0, original.begin()[i].data());
    EXPECT_EQ(i + 1, dest.begin()[i].data());
  }
}

TEST(VectorBuffer, CopyToMove) {
  constexpr int size = 10;
  VectorBuffer<CopyOnlyInt> dest(size);

  VectorBuffer<CopyOnlyInt> original(size);
  for (int i = 0; i < size; i++)
    new (original.begin() + i) CopyOnlyInt(i + 1);

  original.MoveRange(original.begin(), original.end(), dest.begin());

  // The original should have been destructed, which should reset the value to
  // 0. Technically this dereferences the destructed object.
  for (int i = 0; i < size; i++) {
    EXPECT_EQ(0, original.begin()[i].data());
    EXPECT_EQ(i + 1, dest.begin()[i].data());
  }
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
    new (original.begin() + i)
        TrivialAbiWithCountingOperations(&destruction_count, &move_count);
  }

  original.MoveRange(original.begin(), original.end(), dest.begin());

  // We expect the move to have been performed via memcpy, without calling move
  // constructors or destructors.
  EXPECT_EQ(destruction_count, kHaveTrivialRelocation ? 0 : size);
  EXPECT_EQ(move_count, kHaveTrivialRelocation ? 0 : size);

  dest.DestructRange(dest.begin(), dest.end());
  EXPECT_EQ(destruction_count, kHaveTrivialRelocation ? size : size * 2);
  EXPECT_EQ(move_count, kHaveTrivialRelocation ? 0 : size);
}

}  // namespace base::internal
