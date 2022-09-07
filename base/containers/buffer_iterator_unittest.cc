// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/buffer_iterator.h"

#include <string.h>

#include <limits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct TestStruct {
  uint32_t one;
  uint8_t two;
};

bool operator==(const TestStruct& lhs, const TestStruct& rhs) {
  return lhs.one == rhs.one && lhs.two == rhs.two;
}

TestStruct CreateTestStruct() {
  TestStruct expected;
  expected.one = 0xabcdef12;
  expected.two = 0x34;
  return expected;
}

TEST(BufferIteratorTest, Object) {
  TestStruct expected = CreateTestStruct();

  char buffer[sizeof(TestStruct)];
  memcpy(buffer, &expected, sizeof(buffer));

  {
    // Read the object.
    BufferIterator<char> iterator(buffer, sizeof(buffer));
    const TestStruct* actual = iterator.Object<TestStruct>();
    EXPECT_EQ(expected, *actual);
  }
  {
    // Iterator's view of the data is not large enough to read the object.
    BufferIterator<char> iterator(buffer, sizeof(buffer) - 1);
    const TestStruct* actual = iterator.Object<TestStruct>();
    EXPECT_FALSE(actual);
  }
}

TEST(BufferIteratorTest, MutableObject) {
  TestStruct expected = CreateTestStruct();

  char buffer[sizeof(TestStruct)];

  BufferIterator<char> iterator(buffer, sizeof(buffer));

  {
    // Write the object.
    TestStruct* actual = iterator.MutableObject<TestStruct>();
    actual->one = expected.one;
    actual->two = expected.two;
  }

  // Rewind the iterator.
  iterator.Seek(0);

  {
    // Read the object back.
    const TestStruct* actual = iterator.Object<TestStruct>();
    EXPECT_EQ(expected, *actual);
  }
}

TEST(BufferIteratorTest, ObjectSizeOverflow) {
  char buffer[64];
  BufferIterator<char> iterator(buffer, std::numeric_limits<size_t>::max());

  auto* pointer = iterator.Object<uint64_t>();
  EXPECT_TRUE(pointer);

  iterator.Seek(iterator.total_size() - 1);

  auto* invalid_pointer = iterator.Object<uint64_t>();
  EXPECT_FALSE(invalid_pointer);
}

TEST(BufferIteratorTest, Span) {
  TestStruct expected = CreateTestStruct();

  std::vector<char> buffer(sizeof(TestStruct) * 3);

  {
    // Load the span with data.
    BufferIterator<char> iterator(buffer);
    span<TestStruct> span = iterator.MutableSpan<TestStruct>(3);
    for (auto& ts : span) {
      memcpy(&ts, &expected, sizeof(expected));
    }
  }
  {
    // Read the data back out.
    BufferIterator<char> iterator(buffer);

    const TestStruct* actual = iterator.Object<TestStruct>();
    EXPECT_EQ(expected, *actual);

    actual = iterator.Object<TestStruct>();
    EXPECT_EQ(expected, *actual);

    actual = iterator.Object<TestStruct>();
    EXPECT_EQ(expected, *actual);

    EXPECT_EQ(iterator.total_size(), iterator.position());
  }
  {
    // Cannot create spans larger than there are data for.
    BufferIterator<char> iterator(buffer);
    span<const TestStruct> span = iterator.Span<TestStruct>(4);
    EXPECT_TRUE(span.empty());
  }
}

TEST(BufferIteratorTest, SpanOverflow) {
  char buffer[64];

  BufferIterator<char> iterator(buffer, std::numeric_limits<size_t>::max());
  {
    span<const uint64_t> empty_span = iterator.Span<uint64_t>(
        (std::numeric_limits<size_t>::max() / sizeof(uint64_t)) + 1);
    EXPECT_TRUE(empty_span.empty());
  }
  {
    span<const uint64_t> empty_span =
        iterator.Span<uint64_t>(std::numeric_limits<size_t>::max());
    EXPECT_TRUE(empty_span.empty());
  }
  {
    iterator.Seek(iterator.total_size() - 7);
    span<const uint64_t> empty_span = iterator.Span<uint64_t>(1);
    EXPECT_TRUE(empty_span.empty());
  }
}

TEST(BufferIteratorTest, Position) {
  char buffer[64];
  BufferIterator<char> iterator(buffer, sizeof(buffer));
  EXPECT_EQ(sizeof(buffer), iterator.total_size());

  size_t position = iterator.position();
  EXPECT_EQ(0u, position);

  iterator.Object<uint8_t>();
  EXPECT_EQ(sizeof(uint8_t), iterator.position() - position);
  position = iterator.position();

  iterator.Object<uint32_t>();
  EXPECT_EQ(sizeof(uint32_t), iterator.position() - position);
  position = iterator.position();

  iterator.Seek(32);
  EXPECT_EQ(32u, iterator.position());

  EXPECT_EQ(sizeof(buffer), iterator.total_size());
}

}  // namespace
}  // namespace base
