// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/value_iterators.h"

#include <type_traits>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace detail {

namespace {

// Implementation of std::equal variant that is missing in C++11.
template <class BinaryPredicate, class InputIterator1, class InputIterator2>
bool are_equal(InputIterator1 first1,
               InputIterator1 last1,
               InputIterator2 first2,
               InputIterator2 last2,
               BinaryPredicate pred) {
  for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
    if (!pred(*first1, *first2))
      return false;
  }
  return first1 == last1 && first2 == last2;
}

}  // namespace

TEST(ValueIteratorsTest, IsAssignable) {
  static_assert(
      !std::is_assignable<dict_iterator::reference::first_type, std::string>(),
      "Can assign strings to dict_iterator");

  static_assert(
      std::is_assignable<dict_iterator::reference::second_type, Value>(),
      "Can't assign Values to dict_iterator");

  static_assert(!std::is_assignable<const_dict_iterator::reference::first_type,
                                    std::string>(),
                "Can assign strings to const_dict_iterator");

  static_assert(
      !std::is_assignable<const_dict_iterator::reference::second_type, Value>(),
      "Can assign Values to const_dict_iterator");
}

TEST(ValueIteratorsTest, DictIteratorOperatorStar) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", (*iter).first);
  EXPECT_EQ(Value(0), (*iter).second);

  (*iter).second = Value(1);
  EXPECT_EQ(Value(1), *storage["0"]);
}

TEST(ValueIteratorsTest, DictIteratorOperatorArrow) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);

  iter->second = Value(1);
  EXPECT_EQ(Value(1), *storage["0"]);
}

TEST(ValueIteratorsTest, DictIteratorPreIncrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);

  iterator& iter_ref = ++iter;
  EXPECT_EQ(&iter, &iter_ref);

  EXPECT_EQ("1", iter_ref->first);
  EXPECT_EQ(Value(1), iter_ref->second);
}

TEST(ValueIteratorsTest, DictIteratorPostIncrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = dict_iterator;
  iterator iter(storage.begin());
  iterator iter_old = iter++;

  EXPECT_EQ("0", iter_old->first);
  EXPECT_EQ(Value(0), iter_old->second);

  EXPECT_EQ("1", iter->first);
  EXPECT_EQ(Value(1), iter->second);
}

TEST(ValueIteratorsTest, DictIteratorPreDecrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = dict_iterator;
  iterator iter(++storage.begin());
  EXPECT_EQ("1", iter->first);
  EXPECT_EQ(Value(1), iter->second);

  iterator& iter_ref = --iter;
  EXPECT_EQ(&iter, &iter_ref);

  EXPECT_EQ("0", iter_ref->first);
  EXPECT_EQ(Value(0), iter_ref->second);
}

TEST(ValueIteratorsTest, DictIteratorPostDecrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = dict_iterator;
  iterator iter(++storage.begin());
  iterator iter_old = iter--;

  EXPECT_EQ("1", iter_old->first);
  EXPECT_EQ(Value(1), iter_old->second);

  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);
}

TEST(ValueIteratorsTest, DictIteratorOperatorEQ) {
  DictStorage storage;
  using iterator = dict_iterator;
  EXPECT_EQ(iterator(storage.begin()), iterator(storage.begin()));
  EXPECT_EQ(iterator(storage.end()), iterator(storage.end()));
}

TEST(ValueIteratorsTest, DictIteratorOperatorNE) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = dict_iterator;
  EXPECT_NE(iterator(storage.begin()), iterator(storage.end()));
}

TEST(ValueIteratorsTest, ConstDictIteratorOperatorStar) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = const_dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", (*iter).first);
  EXPECT_EQ(Value(0), (*iter).second);
}

TEST(ValueIteratorsTest, ConstDictIteratorOperatorArrow) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = const_dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);
}

TEST(ValueIteratorsTest, ConstDictIteratorPreIncrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = const_dict_iterator;
  iterator iter(storage.begin());
  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);

  iterator& iter_ref = ++iter;
  EXPECT_EQ(&iter, &iter_ref);

  EXPECT_EQ("1", iter_ref->first);
  EXPECT_EQ(Value(1), iter_ref->second);
}

TEST(ValueIteratorsTest, ConstDictIteratorPostIncrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = const_dict_iterator;
  iterator iter(storage.begin());
  iterator iter_old = iter++;

  EXPECT_EQ("0", iter_old->first);
  EXPECT_EQ(Value(0), iter_old->second);

  EXPECT_EQ("1", iter->first);
  EXPECT_EQ(Value(1), iter->second);
}

TEST(ValueIteratorsTest, ConstDictIteratorPreDecrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = const_dict_iterator;
  iterator iter(++storage.begin());
  EXPECT_EQ("1", iter->first);
  EXPECT_EQ(Value(1), iter->second);

  iterator& iter_ref = --iter;
  EXPECT_EQ(&iter, &iter_ref);

  EXPECT_EQ("0", iter_ref->first);
  EXPECT_EQ(Value(0), iter_ref->second);
}

TEST(ValueIteratorsTest, ConstDictIteratorPostDecrement) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));
  storage.emplace("1", std::make_unique<Value>(1));

  using iterator = const_dict_iterator;
  iterator iter(++storage.begin());
  iterator iter_old = iter--;

  EXPECT_EQ("1", iter_old->first);
  EXPECT_EQ(Value(1), iter_old->second);

  EXPECT_EQ("0", iter->first);
  EXPECT_EQ(Value(0), iter->second);
}

TEST(ValueIteratorsTest, ConstDictIteratorOperatorEQ) {
  DictStorage storage;
  using iterator = const_dict_iterator;
  EXPECT_EQ(iterator(storage.begin()), iterator(storage.begin()));
  EXPECT_EQ(iterator(storage.end()), iterator(storage.end()));
}

TEST(ValueIteratorsTest, ConstDictIteratorOperatorNE) {
  DictStorage storage;
  storage.emplace("0", std::make_unique<Value>(0));

  using iterator = const_dict_iterator;
  EXPECT_NE(iterator(storage.begin()), iterator(storage.end()));
}

TEST(ValueIteratorsTest, DictIteratorProxy) {
  DictStorage storage;
  storage.emplace("null", std::make_unique<Value>(Value::Type::NONE));
  storage.emplace("bool", std::make_unique<Value>(Value::Type::BOOLEAN));
  storage.emplace("int", std::make_unique<Value>(Value::Type::INTEGER));
  storage.emplace("double", std::make_unique<Value>(Value::Type::DOUBLE));
  storage.emplace("string", std::make_unique<Value>(Value::Type::STRING));
  storage.emplace("blob", std::make_unique<Value>(Value::Type::BINARY));
  storage.emplace("dict", std::make_unique<Value>(Value::Type::DICTIONARY));
  storage.emplace("list", std::make_unique<Value>(Value::Type::LIST));

  using iterator = const_dict_iterator;
  using iterator_proxy = dict_iterator_proxy;
  iterator_proxy proxy(&storage);

  auto equal_to = [](const DictStorage::value_type& lhs,
                     const iterator::reference& rhs) {
    return std::tie(lhs.first, *lhs.second) == std::tie(rhs.first, rhs.second);
  };

  EXPECT_TRUE(are_equal(storage.begin(), storage.end(), proxy.begin(),
                        proxy.end(), equal_to));

  EXPECT_TRUE(are_equal(storage.rbegin(), storage.rend(), proxy.rbegin(),
                        proxy.rend(), equal_to));

  EXPECT_TRUE(are_equal(storage.cbegin(), storage.cend(), proxy.cbegin(),
                        proxy.cend(), equal_to));

  EXPECT_TRUE(are_equal(storage.crbegin(), storage.crend(), proxy.crbegin(),
                        proxy.crend(), equal_to));
}

TEST(ValueIteratorsTest, ConstDictIteratorProxy) {
  DictStorage storage;
  storage.emplace("null", std::make_unique<Value>(Value::Type::NONE));
  storage.emplace("bool", std::make_unique<Value>(Value::Type::BOOLEAN));
  storage.emplace("int", std::make_unique<Value>(Value::Type::INTEGER));
  storage.emplace("double", std::make_unique<Value>(Value::Type::DOUBLE));
  storage.emplace("string", std::make_unique<Value>(Value::Type::STRING));
  storage.emplace("blob", std::make_unique<Value>(Value::Type::BINARY));
  storage.emplace("dict", std::make_unique<Value>(Value::Type::DICTIONARY));
  storage.emplace("list", std::make_unique<Value>(Value::Type::LIST));

  using iterator = const_dict_iterator;
  using iterator_proxy = const_dict_iterator_proxy;
  iterator_proxy proxy(&storage);

  auto equal_to = [](const DictStorage::value_type& lhs,
                     const iterator::reference& rhs) {
    return std::tie(lhs.first, *lhs.second) == std::tie(rhs.first, rhs.second);
  };

  EXPECT_TRUE(are_equal(storage.begin(), storage.end(), proxy.begin(),
                        proxy.end(), equal_to));

  EXPECT_TRUE(are_equal(storage.rbegin(), storage.rend(), proxy.rbegin(),
                        proxy.rend(), equal_to));

  EXPECT_TRUE(are_equal(storage.cbegin(), storage.cend(), proxy.cbegin(),
                        proxy.cend(), equal_to));

  EXPECT_TRUE(are_equal(storage.crbegin(), storage.crend(), proxy.crbegin(),
                        proxy.crend(), equal_to));
}

}  // namespace detail

}  // namespace base
