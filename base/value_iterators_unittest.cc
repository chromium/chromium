// Copyright 2017 The Chromium Authors
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

}  // namespace detail

}  // namespace base
