// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contiguous_iterator.h"

#include <array>
#include <deque>
#include <forward_list>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <valarray>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ContiguousIteratorTest, ForwardIterator) {
  using ForwardIterator = std::forward_list<int>::iterator;
  static_assert(std::is_same<std::forward_iterator_tag,
                             typename std::iterator_traits<
                                 ForwardIterator>::iterator_category>::value,
                "Error: The iterator_category of ForwardIterator is not "
                "std::forward_iterator_tag.");
  static_assert(
      !IsContiguousIterator<ForwardIterator>::value,
      "Error: ForwardIterator should not be considered a contiguous iterator.");
  static_assert(!IsContiguousIterator<const ForwardIterator>::value,
                "Error: const ForwardIterator should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<ForwardIterator&>::value,
                "Error: ForwardIterator& should not be considered a contiguous "
                "iterator.");
  static_assert(!IsContiguousIterator<const ForwardIterator&>::value,
                "Error: const ForwardIterator& should not be considered a "
                "contiguous iterator.");
  static_assert(
      !IsContiguousIterator<std::reverse_iterator<ForwardIterator>>::value,
      "Error: A reverse ForwardIterator should not be considered a "
      "contiguous iterator.");
}

TEST(ContiguousIteratorTest, BidirectionalIterator) {
  using BidirectionalIterator = std::set<int>::iterator;
  static_assert(
      std::is_same<std::bidirectional_iterator_tag,
                   typename std::iterator_traits<
                       BidirectionalIterator>::iterator_category>::value,
      "Error: The iterator_category of BidirectionalIterator is not "
      "std::bidirectional_iterator_tag.");
  static_assert(!IsContiguousIterator<BidirectionalIterator>::value,
                "Error: BidirectionalIterator should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<const BidirectionalIterator>::value,
                "Error: const BidirectionalIterator should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<BidirectionalIterator&>::value,
                "Error: BidirectionalIterator& should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<const BidirectionalIterator&>::value,
                "Error: const BidirectionalIterator& should not be considered "
                "a contiguous iterator.");
  static_assert(!IsContiguousIterator<
                    std::reverse_iterator<BidirectionalIterator>>::value,
                "Error: A reverse BidirectionalIterator should not be "
                "considered a contiguous iterator.");
}

TEST(ContiguousIteratorTest, RandomAccessIterator) {
  using RandomAccessIterator = std::deque<int>::iterator;
  static_assert(
      std::is_same<std::random_access_iterator_tag,
                   typename std::iterator_traits<
                       RandomAccessIterator>::iterator_category>::value,
      "Error: The iterator_category of RandomAccessIterator is not "
      "std::random_access_iterator_tag.");
  static_assert(!IsContiguousIterator<RandomAccessIterator>::value,
                "Error: RandomAccessIterator should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<const RandomAccessIterator>::value,
                "Error: const RandomAccessIterator should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<RandomAccessIterator&>::value,
                "Error: RandomAccessIterator& should not be considered a "
                "contiguous iterator.");
  static_assert(!IsContiguousIterator<const RandomAccessIterator&>::value,
                "Error: const RandomAccessIterator& should not be considered "
                "a contiguous iterator.");
  static_assert(
      !IsContiguousIterator<std::reverse_iterator<RandomAccessIterator>>::value,
      "Error: A reverse RandomAccessIterator should not be "
      "considered a contiguous iterator.");
}

TEST(ContiguousIterator, Pointer) {
  static_assert(IsContiguousIterator<int*>::value,
                "Error: int* should be considered a contiguous iterator.");

  static_assert(
      IsContiguousIterator<int* const>::value,
      "Error: int* const should be considered a contiguous iterator.");

  static_assert(!IsContiguousIterator<void (*)()>::value,
                "Error: A function pointer should not be considered a "
                "contiguous iterator.");
}

TEST(ContiguousIterator, VectorInt) {
  static_assert(IsContiguousIterator<std::vector<int>::iterator>::value,
                "Error: std::vector<int>::iterator should be considered a "
                "contiguous iterator.");

  static_assert(IsContiguousIterator<std::vector<int>::const_iterator>::value,
                "Error: std::vector<int>::const_iterator should be considered "
                "a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::vector<int>::reverse_iterator>::value,
      "Error: std::vector<int>::reverse_iterator should not be considered a "
      "contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::vector<int>::const_reverse_iterator>::value,
      "Error: std::vector<int>::const_reverse_iterator should not be "
      "considered a contiguous iterator.");
}

TEST(ContiguousIterator, VectorString) {
  static_assert(IsContiguousIterator<std::vector<std::string>::iterator>::value,
                "Error: std::vector<std::string>::iterator should be "
                "considered a contiguous iterator.");

  static_assert(
      IsContiguousIterator<std::vector<std::string>::const_iterator>::value,
      "Error: std::vector<std::string>::const_iterator should be considered a "
      "contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::vector<std::string>::reverse_iterator>::value,
      "Error: std::vector<std::string>::reverse_iterator should not be "
      "considered a contiguous iterator.");

  static_assert(!IsContiguousIterator<
                    std::vector<std::string>::const_reverse_iterator>::value,
                "Error: std::vector<std::string>::const_reverse_iterator "
                "should not be considered a contiguous iterator.");
}

TEST(ContiguousIterator, VectorBool) {
  static_assert(!IsContiguousIterator<std::vector<bool>::iterator>::value,
                "Error: std::vector<bool>::iterator should not be considered "
                "a contiguous iterator.");

  static_assert(!IsContiguousIterator<std::vector<bool>::const_iterator>::value,
                "Error: std::vector<bool>::const_iterator should not be "
                "considered a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::vector<bool>::reverse_iterator>::value,
      "Error: std::vector<bool>::reverse_iterator should not be considered a "
      "contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::vector<bool>::const_reverse_iterator>::value,
      "Error: std::vector<bool>::const_reverse_iterator should not be "
      "considered a contiguous iterator.");
}

TEST(ContiguousIterator, ArrayInt) {
  static_assert(IsContiguousIterator<std::array<int, 1>::iterator>::value,
                "Error: std::array<int, 1>::iterator should be considered a "
                "contiguous iterator.");

  static_assert(IsContiguousIterator<std::array<int, 1>::const_iterator>::value,
                "Error: std::array<int, 1>::const_iterator should be "
                "considered a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::array<int, 1>::reverse_iterator>::value,
      "Error: std::array<int, 1>::reverse_iterator should not be considered "
      "a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::array<int, 1>::const_reverse_iterator>::value,
      "Error: std::array<int, 1>::const_reverse_iterator should not be "
      "considered a contiguous iterator.");
}

TEST(ContiguousIterator, ArrayString) {
  static_assert(
      IsContiguousIterator<std::array<std::string, 1>::iterator>::value,
      "Error: std::array<std::string, 1>::iterator should be considered a "
      "contiguous iterator.");

  static_assert(
      IsContiguousIterator<std::array<std::string, 1>::const_iterator>::value,
      "Error: std::array<std::string, 1>::const_iterator should be considered "
      "a contiguous iterator.");

  static_assert(!IsContiguousIterator<
                    std::array<std::string, 1>::reverse_iterator>::value,
                "Error: std::array<std::string, 1>::reverse_iterator should "
                "not be considered a contiguous iterator.");

  static_assert(!IsContiguousIterator<
                    std::array<std::string, 1>::const_reverse_iterator>::value,
                "Error: std::array<std::string, 1>::const_reverse_iterator "
                "should not be considered a contiguous iterator.");
}

TEST(ContiguousIterator, String) {
  static_assert(IsContiguousIterator<std::string::iterator>::value,
                "Error: std::string:iterator should be considered a contiguous"
                "iterator.");

  static_assert(IsContiguousIterator<std::string::const_iterator>::value,
                "Error: std::string::const_iterator should be considered a "
                "contiguous iterator.");

  static_assert(!IsContiguousIterator<std::string::reverse_iterator>::value,
                "Error: std::string::reverse_iterator should not be considered "
                "a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::string::const_reverse_iterator>::value,
      "Error: std::string::const_reverse_iterator should not be considered "
      "a contiguous iterator.");
}

TEST(ContiguousIterator, String16) {
  static_assert(IsContiguousIterator<std::u16string::iterator>::value,
                "Error: std::u16string:iterator should be considered a "
                "contiguous iterator.");

  static_assert(IsContiguousIterator<std::u16string::const_iterator>::value,
                "Error: std::u16string::const_iterator should be considered a "
                "contiguous iterator.");

  static_assert(!IsContiguousIterator<std::u16string::reverse_iterator>::value,
                "Error: std::u16string::reverse_iterator should not be "
                "considered a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<std::u16string::const_reverse_iterator>::value,
      "Error: std::u16string::const_reverse_iterator should not be considered a"
      "contiguous iterator.");
}

TEST(ContiguousIterator, ValarrayInt) {
  static_assert(IsContiguousIterator<decltype(
                    std::begin(std::declval<std::valarray<int>&>()))>::value,
                "Error: std::valarray<int>::iterator should be considered a "
                "contiguous iterator.");

  static_assert(
      IsContiguousIterator<decltype(
          std::begin(std::declval<const std::valarray<int>&>()))>::value,
      "Error: std::valarray<int>::const_iterator should be considered a "
      "contiguous iterator.");
}

TEST(ContiguousIterator, ValarrayString) {
  static_assert(IsContiguousIterator<decltype(std::begin(
                    std::declval<std::valarray<std::string>&>()))>::value,
                "Error: std::valarray<std::string>::iterator should be "
                "considered a contiguous iterator.");

  static_assert(IsContiguousIterator<decltype(std::begin(
                    std::declval<const std::valarray<std::string>&>()))>::value,
                "Error: std::valarray<std::string>::const_iterator should be "
                "considered a contiguous iterator.");
}

TEST(ContiguousIterator, StringPiece) {
  static_assert(
      IsContiguousIterator<base::StringPiece::const_iterator>::value,
      "Error: base::StringPiece::const_iterator should be considered a "
      "contiguous iterator.");

  static_assert(
      !IsContiguousIterator<base::StringPiece::const_reverse_iterator>::value,
      "Error: base::StringPiece::const_reverse_iterator should not be "
      "considered a contiguous iterator.");
}

TEST(ContiguousIterator, SpanInt) {
  static_assert(IsContiguousIterator<base::span<int>::iterator>::value,
                "Error: base::span<int>::iterator should be considered a "
                "contiguous iterator.");

  static_assert(!IsContiguousIterator<base::span<int>::reverse_iterator>::value,
                "Error: base::span<int>::reverse_iterator should not be "
                "considered a contiguous iterator.");
}

TEST(ContiguousIterator, SpanString) {
  static_assert(IsContiguousIterator<base::span<std::string>::iterator>::value,
                "Error: base::span<std::string>::iterator should be considered "
                "a contiguous iterator.");

  static_assert(
      !IsContiguousIterator<base::span<std::string>::reverse_iterator>::value,
      "Error: base::span<std::string>::reverse_iterator should not be "
      "considered a contiguous iterator.");
}

}  // namespace base
