// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include <stddef.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ValuesTest, TestNothrow) {
  static_assert(std::is_nothrow_move_constructible<Value>::value,
                "IsNothrowMoveConstructible");
  static_assert(std::is_nothrow_default_constructible<Value>::value,
                "IsNothrowDefaultConstructible");
  static_assert(std::is_nothrow_constructible<Value, std::string&&>::value,
                "IsNothrowMoveConstructibleFromString");
  static_assert(
      std::is_nothrow_constructible<Value, Value::BlobStorage&&>::value,
      "IsNothrowMoveConstructibleFromBlob");
  static_assert(
      std::is_nothrow_constructible<Value, Value::ListStorage&&>::value,
      "IsNothrowMoveConstructibleFromList");
  static_assert(std::is_nothrow_move_assignable<Value>::value,
                "IsNothrowMoveAssignable");
  static_assert(
      std::is_nothrow_constructible<ListValue, Value::ListStorage&&>::value,
      "ListIsNothrowMoveConstructibleFromList");
}

// Group of tests for the value constructors.
TEST(ValuesTest, ConstructBool) {
  Value true_value(true);
  EXPECT_EQ(Value::Type::BOOLEAN, true_value.type());
  EXPECT_TRUE(true_value.GetBool());

  Value false_value(false);
  EXPECT_EQ(Value::Type::BOOLEAN, false_value.type());
  EXPECT_FALSE(false_value.GetBool());
}

TEST(ValuesTest, ConstructInt) {
  Value value(-37);
  EXPECT_EQ(Value::Type::INTEGER, value.type());
  EXPECT_EQ(-37, value.GetInt());
}

TEST(ValuesTest, ConstructDouble) {
  Value value(-4.655);
  EXPECT_EQ(Value::Type::DOUBLE, value.type());
  EXPECT_EQ(-4.655, value.GetDouble());
}

TEST(ValuesTest, ConstructStringFromConstCharPtr) {
  const char* str = "foobar";
  Value value(str);
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStringPiece) {
  std::string str = "foobar";
  Value value{StringPiece(str)};
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStdStringRRef) {
  std::string str = "foobar";
  Value value(std::move(str));
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromConstChar16Ptr) {
  string16 str = ASCIIToUTF16("foobar");
  Value value(str.c_str());
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStringPiece16) {
  string16 str = ASCIIToUTF16("foobar");
  Value value{StringPiece16(str)};
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructBinary) {
  Value value(Value::BlobStorage({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  EXPECT_EQ(Value::Type::BINARY, value.type());
  EXPECT_EQ(Value::BlobStorage({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}),
            value.GetBlob());
}

TEST(ValuesTest, ConstructDict) {
  DictionaryValue value;
  EXPECT_EQ(Value::Type::DICTIONARY, value.type());
}

TEST(ValuesTest, ConstructDictFromStorage) {
  Value::DictStorage storage;
  storage.emplace("foo", std::make_unique<Value>("bar"));
  {
    DictionaryValue value(storage);
    EXPECT_EQ(Value::Type::DICTIONARY, value.type());
    EXPECT_EQ(Value::Type::STRING, value.FindKey("foo")->type());
    EXPECT_EQ("bar", value.FindKey("foo")->GetString());
  }

  *storage["foo"] = base::Value("baz");
  {
    DictionaryValue value(std::move(storage));
    EXPECT_EQ(Value::Type::DICTIONARY, value.type());
    EXPECT_EQ(Value::Type::STRING, value.FindKey("foo")->type());
    EXPECT_EQ("baz", value.FindKey("foo")->GetString());
  }
}

TEST(ValuesTest, ConstructList) {
  ListValue value;
  EXPECT_EQ(Value::Type::LIST, value.type());
}

TEST(ValuesTest, ConstructListFromStorage) {
  Value::ListStorage storage;
  storage.emplace_back("foo");
  {
    ListValue value(storage);
    EXPECT_EQ(Value::Type::LIST, value.type());
    EXPECT_EQ(1u, value.GetList().size());
    EXPECT_EQ(Value::Type::STRING, value.GetList()[0].type());
    EXPECT_EQ("foo", value.GetList()[0].GetString());
  }

  storage.back() = base::Value("bar");
  {
    ListValue value(std::move(storage));
    EXPECT_EQ(Value::Type::LIST, value.type());
    EXPECT_EQ(1u, value.GetList().size());
    EXPECT_EQ(Value::Type::STRING, value.GetList()[0].type());
    EXPECT_EQ("bar", value.GetList()[0].GetString());
  }
}

// Group of tests for the copy constructors and copy-assigmnent. For equality
// checks comparisons of the interesting fields are done instead of relying on
// Equals being correct.
TEST(ValuesTest, CopyBool) {
  Value true_value(true);
  Value copied_true_value(true_value.Clone());
  EXPECT_EQ(true_value.type(), copied_true_value.type());
  EXPECT_EQ(true_value.GetBool(), copied_true_value.GetBool());

  Value false_value(false);
  Value copied_false_value(false_value.Clone());
  EXPECT_EQ(false_value.type(), copied_false_value.type());
  EXPECT_EQ(false_value.GetBool(), copied_false_value.GetBool());

  Value blank;

  blank = true_value.Clone();
  EXPECT_EQ(true_value.type(), blank.type());
  EXPECT_EQ(true_value.GetBool(), blank.GetBool());

  blank = false_value.Clone();
  EXPECT_EQ(false_value.type(), blank.type());
  EXPECT_EQ(false_value.GetBool(), blank.GetBool());
}

TEST(ValuesTest, CopyInt) {
  Value value(74);
  Value copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetInt(), copied_value.GetInt());

  Value blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetInt(), blank.GetInt());
}

TEST(ValuesTest, CopyDouble) {
  Value value(74.896);
  Value copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetDouble(), copied_value.GetDouble());

  Value blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetDouble(), blank.GetDouble());
}

TEST(ValuesTest, CopyString) {
  Value value("foobar");
  Value copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetString(), copied_value.GetString());

  Value blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetString(), blank.GetString());
}

TEST(ValuesTest, CopyBinary) {
  Value value(Value::BlobStorage({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  Value copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetBlob(), copied_value.GetBlob());

  Value blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetBlob(), blank.GetBlob());
}

TEST(ValuesTest, CopyDictionary) {
  Value::DictStorage storage;
  storage.emplace("Int", std::make_unique<Value>(123));
  Value value(std::move(storage));

  Value copied_value(value.Clone());
  EXPECT_EQ(value, copied_value);

  Value blank;
  blank = value.Clone();
  EXPECT_EQ(value, blank);
}

TEST(ValuesTest, CopyList) {
  Value::ListStorage storage;
  storage.emplace_back(123);
  Value value(std::move(storage));

  Value copied_value(value.Clone());
  EXPECT_EQ(value, copied_value);

  Value blank;
  blank = value.Clone();
  EXPECT_EQ(value, blank);
}

// Group of tests for the move constructors and move-assigmnent.
TEST(ValuesTest, MoveBool) {
  Value true_value(true);
  Value moved_true_value(std::move(true_value));
  EXPECT_EQ(Value::Type::BOOLEAN, moved_true_value.type());
  EXPECT_TRUE(moved_true_value.GetBool());

  Value false_value(false);
  Value moved_false_value(std::move(false_value));
  EXPECT_EQ(Value::Type::BOOLEAN, moved_false_value.type());
  EXPECT_FALSE(moved_false_value.GetBool());

  Value blank;

  blank = Value(true);
  EXPECT_EQ(Value::Type::BOOLEAN, blank.type());
  EXPECT_TRUE(blank.GetBool());

  blank = Value(false);
  EXPECT_EQ(Value::Type::BOOLEAN, blank.type());
  EXPECT_FALSE(blank.GetBool());
}

TEST(ValuesTest, MoveInt) {
  Value value(74);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::INTEGER, moved_value.type());
  EXPECT_EQ(74, moved_value.GetInt());

  Value blank;

  blank = Value(47);
  EXPECT_EQ(Value::Type::INTEGER, blank.type());
  EXPECT_EQ(47, blank.GetInt());
}

TEST(ValuesTest, MoveDouble) {
  Value value(74.896);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::DOUBLE, moved_value.type());
  EXPECT_EQ(74.896, moved_value.GetDouble());

  Value blank;

  blank = Value(654.38);
  EXPECT_EQ(Value::Type::DOUBLE, blank.type());
  EXPECT_EQ(654.38, blank.GetDouble());
}

TEST(ValuesTest, MoveString) {
  Value value("foobar");
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::STRING, moved_value.type());
  EXPECT_EQ("foobar", moved_value.GetString());

  Value blank;

  blank = Value("foobar");
  EXPECT_EQ(Value::Type::STRING, blank.type());
  EXPECT_EQ("foobar", blank.GetString());
}

TEST(ValuesTest, MoveBinary) {
  const Value::BlobStorage buffer = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value value(buffer);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::BINARY, moved_value.type());
  EXPECT_EQ(buffer, moved_value.GetBlob());

  Value blank;

  blank = Value(buffer);
  EXPECT_EQ(Value::Type::BINARY, blank.type());
  EXPECT_EQ(buffer, blank.GetBlob());
}

TEST(ValuesTest, MoveConstructDictionary) {
  Value::DictStorage storage;
  storage.emplace("Int", std::make_unique<Value>(123));

  Value value(std::move(storage));
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::DICTIONARY, moved_value.type());
  EXPECT_EQ(123, moved_value.FindKey("Int")->GetInt());
}

TEST(ValuesTest, MoveAssignDictionary) {
  Value::DictStorage storage;
  storage.emplace("Int", std::make_unique<Value>(123));

  Value blank;
  blank = Value(std::move(storage));
  EXPECT_EQ(Value::Type::DICTIONARY, blank.type());
  EXPECT_EQ(123, blank.FindKey("Int")->GetInt());
}

TEST(ValuesTest, MoveList) {
  Value::ListStorage storage;
  storage.emplace_back(123);
  Value value(storage);
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::LIST, moved_value.type());
  EXPECT_EQ(123, moved_value.GetList().back().GetInt());

  Value blank;
  blank = Value(std::move(storage));
  EXPECT_EQ(Value::Type::LIST, blank.type());
  EXPECT_EQ(123, blank.GetList().back().GetInt());
}

TEST(ValuesTest, FindKey) {
  Value::DictStorage storage;
  storage.emplace("foo", std::make_unique<Value>("bar"));
  Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKey("foo"));
  EXPECT_EQ(nullptr, dict.FindKey("baz"));

  // Single not found key.
  bool found = dict.FindKey("notfound");
  EXPECT_FALSE(found);
}

TEST(ValuesTest, FindKeyChangeValue) {
  Value::DictStorage storage;
  storage.emplace("foo", std::make_unique<Value>("bar"));
  Value dict(std::move(storage));
  Value* found = dict.FindKey("foo");
  EXPECT_NE(nullptr, found);
  EXPECT_EQ("bar", found->GetString());

  *found = Value(123);
  EXPECT_EQ(123, dict.FindKey("foo")->GetInt());
}

TEST(ValuesTest, FindKeyConst) {
  Value::DictStorage storage;
  storage.emplace("foo", std::make_unique<Value>("bar"));
  const Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKey("foo"));
  EXPECT_EQ(nullptr, dict.FindKey("baz"));
}

TEST(ValuesTest, FindKeyOfType) {
  Value::DictStorage storage;
  storage.emplace("null", std::make_unique<Value>(Value::Type::NONE));
  storage.emplace("bool", std::make_unique<Value>(Value::Type::BOOLEAN));
  storage.emplace("int", std::make_unique<Value>(Value::Type::INTEGER));
  storage.emplace("double", std::make_unique<Value>(Value::Type::DOUBLE));
  storage.emplace("string", std::make_unique<Value>(Value::Type::STRING));
  storage.emplace("blob", std::make_unique<Value>(Value::Type::BINARY));
  storage.emplace("list", std::make_unique<Value>(Value::Type::LIST));
  storage.emplace("dict", std::make_unique<Value>(Value::Type::DICTIONARY));

  Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKeyOfType("null", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::NONE));
  EXPECT_NE(nullptr, dict.FindKeyOfType("bool", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::BOOLEAN));
  EXPECT_NE(nullptr, dict.FindKeyOfType("int", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::INTEGER));
  EXPECT_NE(nullptr, dict.FindKeyOfType("double", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::DOUBLE));
  EXPECT_NE(nullptr, dict.FindKeyOfType("string", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::STRING));
  EXPECT_NE(nullptr, dict.FindKeyOfType("blob", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::BINARY));
  EXPECT_NE(nullptr, dict.FindKeyOfType("list", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::LIST));
  EXPECT_NE(nullptr, dict.FindKeyOfType("dict", Value::Type::DICTIONARY));
}

TEST(ValuesTest, FindKeyOfTypeConst) {
  Value::DictStorage storage;
  storage.emplace("null", std::make_unique<Value>(Value::Type::NONE));
  storage.emplace("bool", std::make_unique<Value>(Value::Type::BOOLEAN));
  storage.emplace("int", std::make_unique<Value>(Value::Type::INTEGER));
  storage.emplace("double", std::make_unique<Value>(Value::Type::DOUBLE));
  storage.emplace("string", std::make_unique<Value>(Value::Type::STRING));
  storage.emplace("blob", std::make_unique<Value>(Value::Type::BINARY));
  storage.emplace("list", std::make_unique<Value>(Value::Type::LIST));
  storage.emplace("dict", std::make_unique<Value>(Value::Type::DICTIONARY));

  const Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKeyOfType("null", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("null", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::NONE));
  EXPECT_NE(nullptr, dict.FindKeyOfType("bool", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("bool", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::BOOLEAN));
  EXPECT_NE(nullptr, dict.FindKeyOfType("int", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("int", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::INTEGER));
  EXPECT_NE(nullptr, dict.FindKeyOfType("double", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("double", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::DOUBLE));
  EXPECT_NE(nullptr, dict.FindKeyOfType("string", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("string", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::STRING));
  EXPECT_NE(nullptr, dict.FindKeyOfType("blob", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("blob", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::BINARY));
  EXPECT_NE(nullptr, dict.FindKeyOfType("list", Value::Type::LIST));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("list", Value::Type::DICTIONARY));

  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::NONE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::INTEGER));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::DOUBLE));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::STRING));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::BINARY));
  EXPECT_EQ(nullptr, dict.FindKeyOfType("dict", Value::Type::LIST));
  EXPECT_NE(nullptr, dict.FindKeyOfType("dict", Value::Type::DICTIONARY));
}

TEST(ValuesTest, SetKey) {
  Value::DictStorage storage;
  storage.emplace("null", std::make_unique<Value>(Value::Type::NONE));
  storage.emplace("bool", std::make_unique<Value>(Value::Type::BOOLEAN));
  storage.emplace("int", std::make_unique<Value>(Value::Type::INTEGER));
  storage.emplace("double", std::make_unique<Value>(Value::Type::DOUBLE));
  storage.emplace("string", std::make_unique<Value>(Value::Type::STRING));
  storage.emplace("blob", std::make_unique<Value>(Value::Type::BINARY));
  storage.emplace("list", std::make_unique<Value>(Value::Type::LIST));
  storage.emplace("dict", std::make_unique<Value>(Value::Type::DICTIONARY));

  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(StringPiece("null"), Value(Value::Type::NONE));
  dict.SetKey(StringPiece("bool"), Value(Value::Type::BOOLEAN));
  dict.SetKey(std::string("int"), Value(Value::Type::INTEGER));
  dict.SetKey(std::string("double"), Value(Value::Type::DOUBLE));
  dict.SetKey(std::string("string"), Value(Value::Type::STRING));
  dict.SetKey("blob", Value(Value::Type::BINARY));
  dict.SetKey("list", Value(Value::Type::LIST));
  dict.SetKey("dict", Value(Value::Type::DICTIONARY));

  EXPECT_EQ(Value(std::move(storage)), dict);
}

TEST(ValuesTest, FindPath) {
  // Construct a dictionary path {root}.foo.bar = 123
  Value foo(Value::Type::DICTIONARY);
  foo.SetKey("bar", Value(123));

  Value root(Value::Type::DICTIONARY);
  root.SetKey("foo", std::move(foo));

  // No key (stupid but well-defined and takes work to prevent).
  Value* found = root.FindPath(std::vector<StringPiece>{});
  EXPECT_EQ(&root, found);

  // Double key, second not found.
  found = root.FindPath(std::vector<StringPiece>{"foo", "notfound"});
  EXPECT_FALSE(found);

  // Double key, found.
  found = root.FindPath(std::vector<StringPiece>{"foo", "bar"});
  EXPECT_TRUE(found);
  EXPECT_TRUE(found->is_int());
  EXPECT_EQ(123, found->GetInt());
}

TEST(ValuesTest, SetPath) {
  Value root(Value::Type::DICTIONARY);

  Value* inserted = root.SetPath({"one", "two"}, Value(123));
  Value* found = root.FindPathOfType({"one", "two"}, Value::Type::INTEGER);
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  inserted = root.SetPath(std::vector<StringPiece>{"foo", "bar"}, Value(123));
  found = root.FindPathOfType({"foo", "bar"}, Value::Type::INTEGER);
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  // Overwrite with a different value.
  root.SetPath({"foo", "bar"}, Value("hello"));
  found = root.FindPathOfType(std::vector<StringPiece>{"foo", "bar"},
                              Value::Type::STRING);
  ASSERT_TRUE(found);
  EXPECT_EQ("hello", found->GetString());

  // Can't change existing non-dictionary keys to dictionaries.
  found =
      root.SetPath(std::vector<StringPiece>{"foo", "bar", "baz"}, Value(123));
  EXPECT_FALSE(found);
}

TEST(ValuesTest, RemoveKey) {
  Value root(Value::Type::DICTIONARY);
  root.SetKey("one", Value(123));

  // Removal of missing key should fail.
  EXPECT_FALSE(root.RemoveKey("two"));

  // Removal of existing key should succeed.
  EXPECT_TRUE(root.RemoveKey("one"));

  // Second removal of previously existing key should fail.
  EXPECT_FALSE(root.RemoveKey("one"));
}

TEST(ValuesTest, RemovePath) {
  Value root(Value::Type::DICTIONARY);
  root.SetPath({"one", "two", "three"}, Value(123));

  // Removal of missing key should fail.
  EXPECT_FALSE(root.RemovePath({"one", "two", "four"}));

  // Removal of existing key should succeed.
  EXPECT_TRUE(root.RemovePath({"one", "two", "three"}));

  // Second removal of previously existing key should fail.
  EXPECT_FALSE(root.RemovePath({"one", "two", "three"}));

  // Intermediate empty dictionaries should be cleared.
  EXPECT_FALSE(root.FindKey("one"));

  root.SetPath({"one", "two", "three"}, Value(123));
  root.SetPath({"one", "two", "four"}, Value(124));

  EXPECT_TRUE(root.RemovePath(std::vector<StringPiece>{"one", "two", "three"}));
  // Intermediate non-empty dictionaries should be kept.
  EXPECT_TRUE(root.FindKey("one"));
  EXPECT_TRUE(root.FindPath({"one", "two"}));
  EXPECT_TRUE(root.FindPath({"one", "two", "four"}));
}

TEST(ValuesTest, Basic) {
  // Test basic dictionary getting/setting
  DictionaryValue settings;
  std::string homepage = "http://google.com";
  ASSERT_FALSE(settings.GetString("global.homepage", &homepage));
  ASSERT_EQ(std::string("http://google.com"), homepage);

  ASSERT_FALSE(settings.Get("global", nullptr));
  settings.SetBoolean("global", true);
  ASSERT_TRUE(settings.Get("global", nullptr));
  settings.SetString("global.homepage", "http://scurvy.com");
  ASSERT_TRUE(settings.Get("global", nullptr));
  homepage = "http://google.com";
  ASSERT_TRUE(settings.GetString("global.homepage", &homepage));
  ASSERT_EQ(std::string("http://scurvy.com"), homepage);

  // Test storing a dictionary in a list.
  ListValue* toolbar_bookmarks;
  ASSERT_FALSE(
    settings.GetList("global.toolbar.bookmarks", &toolbar_bookmarks));

  std::unique_ptr<ListValue> new_toolbar_bookmarks(new ListValue);
  settings.Set("global.toolbar.bookmarks", std::move(new_toolbar_bookmarks));
  ASSERT_TRUE(settings.GetList("global.toolbar.bookmarks", &toolbar_bookmarks));

  std::unique_ptr<DictionaryValue> new_bookmark(new DictionaryValue);
  new_bookmark->SetString("name", "Froogle");
  new_bookmark->SetString("url", "http://froogle.com");
  toolbar_bookmarks->Append(std::move(new_bookmark));

  ListValue* bookmark_list;
  ASSERT_TRUE(settings.GetList("global.toolbar.bookmarks", &bookmark_list));
  DictionaryValue* bookmark;
  ASSERT_EQ(1U, bookmark_list->GetSize());
  ASSERT_TRUE(bookmark_list->GetDictionary(0, &bookmark));
  std::string bookmark_name = "Unnamed";
  ASSERT_TRUE(bookmark->GetString("name", &bookmark_name));
  ASSERT_EQ(std::string("Froogle"), bookmark_name);
  std::string bookmark_url;
  ASSERT_TRUE(bookmark->GetString("url", &bookmark_url));
  ASSERT_EQ(std::string("http://froogle.com"), bookmark_url);
}

TEST(ValuesTest, List) {
  std::unique_ptr<ListValue> mixed_list(new ListValue());
  mixed_list->Set(0, std::make_unique<Value>(true));
  mixed_list->Set(1, std::make_unique<Value>(42));
  mixed_list->Set(2, std::make_unique<Value>(88.8));
  mixed_list->Set(3, std::make_unique<Value>("foo"));
  ASSERT_EQ(4u, mixed_list->GetSize());

  Value* value = nullptr;
  bool bool_value = false;
  int int_value = 0;
  double double_value = 0.0;
  std::string string_value;

  ASSERT_FALSE(mixed_list->Get(4, &value));

  ASSERT_FALSE(mixed_list->GetInteger(0, &int_value));
  ASSERT_EQ(0, int_value);
  ASSERT_FALSE(mixed_list->GetBoolean(1, &bool_value));
  ASSERT_FALSE(bool_value);
  ASSERT_FALSE(mixed_list->GetString(2, &string_value));
  ASSERT_EQ("", string_value);
  ASSERT_FALSE(mixed_list->GetInteger(2, &int_value));
  ASSERT_EQ(0, int_value);
  ASSERT_FALSE(mixed_list->GetBoolean(3, &bool_value));
  ASSERT_FALSE(bool_value);

  ASSERT_TRUE(mixed_list->GetBoolean(0, &bool_value));
  ASSERT_TRUE(bool_value);
  ASSERT_TRUE(mixed_list->GetInteger(1, &int_value));
  ASSERT_EQ(42, int_value);
  // implicit conversion from Integer to Double should be possible.
  ASSERT_TRUE(mixed_list->GetDouble(1, &double_value));
  ASSERT_EQ(42, double_value);
  ASSERT_TRUE(mixed_list->GetDouble(2, &double_value));
  ASSERT_EQ(88.8, double_value);
  ASSERT_TRUE(mixed_list->GetString(3, &string_value));
  ASSERT_EQ("foo", string_value);

  // Try searching in the mixed list.
  base::Value sought_value(42);
  base::Value not_found_value(false);

  ASSERT_NE(mixed_list->end(), mixed_list->Find(sought_value));
  ASSERT_TRUE((*mixed_list->Find(sought_value)).GetAsInteger(&int_value));
  ASSERT_EQ(42, int_value);
  ASSERT_EQ(mixed_list->end(), mixed_list->Find(not_found_value));
}

TEST(ValuesTest, BinaryValue) {
  // Default constructor creates a BinaryValue with a buffer of size 0.
  auto binary = std::make_unique<Value>(Value::Type::BINARY);
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBlob().empty());

  // Test the common case of a non-empty buffer
  Value::BlobStorage buffer(15);
  uint8_t* original_buffer = buffer.data();
  binary.reset(new Value(std::move(buffer)));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBlob().data());
  ASSERT_EQ(original_buffer, binary->GetBlob().data());
  ASSERT_EQ(15U, binary->GetBlob().size());

  char stack_buffer[42];
  memset(stack_buffer, '!', 42);
  binary = Value::CreateWithCopiedBuffer(stack_buffer, 42);
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBlob().data());
  ASSERT_NE(stack_buffer,
            reinterpret_cast<const char*>(binary->GetBlob().data()));
  ASSERT_EQ(42U, binary->GetBlob().size());
  ASSERT_EQ(0, memcmp(stack_buffer, binary->GetBlob().data(),
                      binary->GetBlob().size()));
}

TEST(ValuesTest, StringValue) {
  // Test overloaded StringValue constructor.
  std::unique_ptr<Value> narrow_value(new Value("narrow"));
  ASSERT_TRUE(narrow_value.get());
  ASSERT_TRUE(narrow_value->is_string());
  std::unique_ptr<Value> utf16_value(new Value(ASCIIToUTF16("utf16")));
  ASSERT_TRUE(utf16_value.get());
  ASSERT_TRUE(utf16_value->is_string());

  // Test overloaded GetAsString.
  std::string narrow = "http://google.com";
  string16 utf16 = ASCIIToUTF16("http://google.com");
  const Value* string_value = nullptr;
  ASSERT_TRUE(narrow_value->GetAsString(&narrow));
  ASSERT_TRUE(narrow_value->GetAsString(&utf16));
  ASSERT_TRUE(narrow_value->GetAsString(&string_value));
  ASSERT_EQ(std::string("narrow"), narrow);
  ASSERT_EQ(ASCIIToUTF16("narrow"), utf16);
  ASSERT_EQ(string_value->GetString(), narrow);

  ASSERT_TRUE(utf16_value->GetAsString(&narrow));
  ASSERT_TRUE(utf16_value->GetAsString(&utf16));
  ASSERT_TRUE(utf16_value->GetAsString(&string_value));
  ASSERT_EQ(std::string("utf16"), narrow);
  ASSERT_EQ(ASCIIToUTF16("utf16"), utf16);
  ASSERT_EQ(string_value->GetString(), narrow);

  // Don't choke on NULL values.
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<string16*>(nullptr)));
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<std::string*>(nullptr)));
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<const Value**>(nullptr)));
}

TEST(ValuesTest, ListDeletion) {
  ListValue list;
  list.Append(std::make_unique<Value>());
  EXPECT_FALSE(list.empty());
  list.Clear();
  EXPECT_TRUE(list.empty());
}

TEST(ValuesTest, ListRemoval) {
  std::unique_ptr<Value> removed_item;

  {
    ListValue list;
    list.Append(std::make_unique<Value>());
    EXPECT_EQ(1U, list.GetSize());
    EXPECT_FALSE(list.Remove(std::numeric_limits<size_t>::max(),
                             &removed_item));
    EXPECT_FALSE(list.Remove(1, &removed_item));
    EXPECT_TRUE(list.Remove(0, &removed_item));
    ASSERT_TRUE(removed_item);
    EXPECT_EQ(0U, list.GetSize());
  }
  removed_item.reset();

  {
    ListValue list;
    list.Append(std::make_unique<Value>());
    EXPECT_TRUE(list.Remove(0, nullptr));
    EXPECT_EQ(0U, list.GetSize());
  }

  {
    ListValue list;
    auto value = std::make_unique<Value>();
    Value original_value = value->Clone();
    list.Append(std::move(value));
    size_t index = 0;
    list.Remove(original_value, &index);
    EXPECT_EQ(0U, index);
    EXPECT_EQ(0U, list.GetSize());
  }
}

TEST(ValuesTest, DictionaryDeletion) {
  std::string key = "test";
  DictionaryValue dict;
  dict.Set(key, std::make_unique<Value>());
  EXPECT_FALSE(dict.empty());
  EXPECT_FALSE(dict.DictEmpty());
  EXPECT_EQ(1U, dict.DictSize());
  dict.Clear();
  EXPECT_TRUE(dict.empty());
  EXPECT_TRUE(dict.DictEmpty());
  EXPECT_EQ(0U, dict.DictSize());
}

TEST(ValuesTest, DictionarySetReturnsPointer) {
  {
    DictionaryValue dict;
    Value* blank_ptr = dict.Set("foo.bar", std::make_unique<base::Value>());
    EXPECT_EQ(Value::Type::NONE, blank_ptr->type());
  }

  {
    DictionaryValue dict;
    Value* blank_ptr = dict.SetWithoutPathExpansion(
        "foo.bar", std::make_unique<base::Value>());
    EXPECT_EQ(Value::Type::NONE, blank_ptr->type());
  }

  {
    DictionaryValue dict;
    Value* int_ptr = dict.SetInteger("foo.bar", 42);
    EXPECT_EQ(Value::Type::INTEGER, int_ptr->type());
    EXPECT_EQ(42, int_ptr->GetInt());
  }

  {
    DictionaryValue dict;
    Value* double_ptr = dict.SetDouble("foo.bar", 3.142);
    EXPECT_EQ(Value::Type::DOUBLE, double_ptr->type());
    EXPECT_EQ(3.142, double_ptr->GetDouble());
  }

  {
    DictionaryValue dict;
    Value* string_ptr = dict.SetString("foo.bar", "foo");
    EXPECT_EQ(Value::Type::STRING, string_ptr->type());
    EXPECT_EQ("foo", string_ptr->GetString());
  }

  {
    DictionaryValue dict;
    Value* string16_ptr = dict.SetString("foo.bar", ASCIIToUTF16("baz"));
    EXPECT_EQ(Value::Type::STRING, string16_ptr->type());
    EXPECT_EQ("baz", string16_ptr->GetString());
  }

  {
    DictionaryValue dict;
    DictionaryValue* dict_ptr = dict.SetDictionary(
        "foo.bar", std::make_unique<base::DictionaryValue>());
    EXPECT_EQ(Value::Type::DICTIONARY, dict_ptr->type());
  }

  {
    DictionaryValue dict;
    ListValue* list_ptr =
        dict.SetList("foo.bar", std::make_unique<base::ListValue>());
    EXPECT_EQ(Value::Type::LIST, list_ptr->type());
  }
}

TEST(ValuesTest, DictionaryRemoval) {
  std::string key = "test";
  std::unique_ptr<Value> removed_item;

  {
    DictionaryValue dict;
    EXPECT_EQ(0U, dict.DictSize());
    EXPECT_TRUE(dict.DictEmpty());
    dict.Set(key, std::make_unique<Value>());
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_FALSE(dict.Remove("absent key", &removed_item));
    EXPECT_EQ(1U, dict.DictSize());
    EXPECT_FALSE(dict.DictEmpty());

    EXPECT_TRUE(dict.Remove(key, &removed_item));
    EXPECT_FALSE(dict.HasKey(key));
    ASSERT_TRUE(removed_item);
    EXPECT_EQ(0U, dict.DictSize());
    EXPECT_TRUE(dict.DictEmpty());
  }

  {
    DictionaryValue dict;
    dict.Set(key, std::make_unique<Value>());
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_TRUE(dict.Remove(key, nullptr));
    EXPECT_FALSE(dict.HasKey(key));
  }
}

TEST(ValuesTest, DictionaryWithoutPathExpansion) {
  DictionaryValue dict;
  dict.Set("this.is.expanded", std::make_unique<Value>());
  dict.SetWithoutPathExpansion("this.isnt.expanded", std::make_unique<Value>());

  EXPECT_FALSE(dict.HasKey("this.is.expanded"));
  EXPECT_TRUE(dict.HasKey("this"));
  Value* value1;
  EXPECT_TRUE(dict.Get("this", &value1));
  DictionaryValue* value2;
  ASSERT_TRUE(dict.GetDictionaryWithoutPathExpansion("this", &value2));
  EXPECT_EQ(value1, value2);
  EXPECT_EQ(1U, value2->size());

  EXPECT_TRUE(dict.HasKey("this.isnt.expanded"));
  Value* value3;
  EXPECT_FALSE(dict.Get("this.isnt.expanded", &value3));
  Value* value4;
  ASSERT_TRUE(dict.GetWithoutPathExpansion("this.isnt.expanded", &value4));
  EXPECT_EQ(Value::Type::NONE, value4->type());
}

// Tests the deprecated version of SetWithoutPathExpansion.
// TODO(estade): remove.
TEST(ValuesTest, DictionaryWithoutPathExpansionDeprecated) {
  DictionaryValue dict;
  dict.Set("this.is.expanded", std::make_unique<Value>());
  dict.SetWithoutPathExpansion("this.isnt.expanded", std::make_unique<Value>());

  EXPECT_FALSE(dict.HasKey("this.is.expanded"));
  EXPECT_TRUE(dict.HasKey("this"));
  Value* value1;
  EXPECT_TRUE(dict.Get("this", &value1));
  DictionaryValue* value2;
  ASSERT_TRUE(dict.GetDictionaryWithoutPathExpansion("this", &value2));
  EXPECT_EQ(value1, value2);
  EXPECT_EQ(1U, value2->size());

  EXPECT_TRUE(dict.HasKey("this.isnt.expanded"));
  Value* value3;
  EXPECT_FALSE(dict.Get("this.isnt.expanded", &value3));
  Value* value4;
  ASSERT_TRUE(dict.GetWithoutPathExpansion("this.isnt.expanded", &value4));
  EXPECT_EQ(Value::Type::NONE, value4->type());
}

TEST(ValuesTest, DictionaryRemovePath) {
  DictionaryValue dict;
  dict.SetInteger("a.long.way.down", 1);
  dict.SetBoolean("a.long.key.path", true);

  std::unique_ptr<Value> removed_item;
  EXPECT_TRUE(dict.RemovePath("a.long.way.down", &removed_item));
  ASSERT_TRUE(removed_item);
  EXPECT_TRUE(removed_item->is_int());
  EXPECT_FALSE(dict.HasKey("a.long.way.down"));
  EXPECT_FALSE(dict.HasKey("a.long.way"));
  EXPECT_TRUE(dict.Get("a.long.key.path", nullptr));

  removed_item.reset();
  EXPECT_FALSE(dict.RemovePath("a.long.way.down", &removed_item));
  EXPECT_FALSE(removed_item);
  EXPECT_TRUE(dict.Get("a.long.key.path", nullptr));

  removed_item.reset();
  EXPECT_TRUE(dict.RemovePath("a.long.key.path", &removed_item));
  ASSERT_TRUE(removed_item);
  EXPECT_TRUE(removed_item->is_bool());
  EXPECT_TRUE(dict.empty());
}

TEST(ValuesTest, DeepCopy) {
  DictionaryValue original_dict;
  Value* null_weak = original_dict.Set("null", std::make_unique<Value>());
  Value* bool_weak = original_dict.Set("bool", std::make_unique<Value>(true));
  Value* int_weak = original_dict.Set("int", std::make_unique<Value>(42));
  Value* double_weak =
      original_dict.Set("double", std::make_unique<Value>(3.14));
  Value* string_weak =
      original_dict.Set("string", std::make_unique<Value>("hello"));
  Value* string16_weak = original_dict.Set(
      "string16", std::make_unique<Value>(ASCIIToUTF16("hello16")));

  Value* binary_weak = original_dict.Set(
      "binary", std::make_unique<Value>(Value::BlobStorage(42, '!')));

  Value::ListStorage storage;
  storage.emplace_back(0);
  storage.emplace_back(1);
  Value* list_weak =
      original_dict.Set("list", std::make_unique<Value>(std::move(storage)));
  Value* list_element_0_weak = &list_weak->GetList()[0];
  Value* list_element_1_weak = &list_weak->GetList()[1];

  DictionaryValue* dict_weak = original_dict.SetDictionary(
      "dictionary", std::make_unique<DictionaryValue>());
  dict_weak->SetString("key", "value");

  auto copy_dict = original_dict.CreateDeepCopy();
  ASSERT_TRUE(copy_dict.get());
  ASSERT_NE(copy_dict.get(), &original_dict);

  Value* copy_null = nullptr;
  ASSERT_TRUE(copy_dict->Get("null", &copy_null));
  ASSERT_TRUE(copy_null);
  ASSERT_NE(copy_null, null_weak);
  ASSERT_TRUE(copy_null->is_none());

  Value* copy_bool = nullptr;
  ASSERT_TRUE(copy_dict->Get("bool", &copy_bool));
  ASSERT_TRUE(copy_bool);
  ASSERT_NE(copy_bool, bool_weak);
  ASSERT_TRUE(copy_bool->is_bool());
  bool copy_bool_value = false;
  ASSERT_TRUE(copy_bool->GetAsBoolean(&copy_bool_value));
  ASSERT_TRUE(copy_bool_value);

  Value* copy_int = nullptr;
  ASSERT_TRUE(copy_dict->Get("int", &copy_int));
  ASSERT_TRUE(copy_int);
  ASSERT_NE(copy_int, int_weak);
  ASSERT_TRUE(copy_int->is_int());
  int copy_int_value = 0;
  ASSERT_TRUE(copy_int->GetAsInteger(&copy_int_value));
  ASSERT_EQ(42, copy_int_value);

  Value* copy_double = nullptr;
  ASSERT_TRUE(copy_dict->Get("double", &copy_double));
  ASSERT_TRUE(copy_double);
  ASSERT_NE(copy_double, double_weak);
  ASSERT_TRUE(copy_double->is_double());
  double copy_double_value = 0;
  ASSERT_TRUE(copy_double->GetAsDouble(&copy_double_value));
  ASSERT_EQ(3.14, copy_double_value);

  Value* copy_string = nullptr;
  ASSERT_TRUE(copy_dict->Get("string", &copy_string));
  ASSERT_TRUE(copy_string);
  ASSERT_NE(copy_string, string_weak);
  ASSERT_TRUE(copy_string->is_string());
  std::string copy_string_value;
  string16 copy_string16_value;
  ASSERT_TRUE(copy_string->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello"), copy_string_value);
  ASSERT_EQ(ASCIIToUTF16("hello"), copy_string16_value);

  Value* copy_string16 = nullptr;
  ASSERT_TRUE(copy_dict->Get("string16", &copy_string16));
  ASSERT_TRUE(copy_string16);
  ASSERT_NE(copy_string16, string16_weak);
  ASSERT_TRUE(copy_string16->is_string());
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello16"), copy_string_value);
  ASSERT_EQ(ASCIIToUTF16("hello16"), copy_string16_value);

  Value* copy_binary = nullptr;
  ASSERT_TRUE(copy_dict->Get("binary", &copy_binary));
  ASSERT_TRUE(copy_binary);
  ASSERT_NE(copy_binary, binary_weak);
  ASSERT_TRUE(copy_binary->is_blob());
  ASSERT_NE(binary_weak->GetBlob().data(), copy_binary->GetBlob().data());
  ASSERT_EQ(binary_weak->GetBlob(), copy_binary->GetBlob());

  Value* copy_value = nullptr;
  ASSERT_TRUE(copy_dict->Get("list", &copy_value));
  ASSERT_TRUE(copy_value);
  ASSERT_NE(copy_value, list_weak);
  ASSERT_TRUE(copy_value->is_list());
  ListValue* copy_list = nullptr;
  ASSERT_TRUE(copy_value->GetAsList(&copy_list));
  ASSERT_TRUE(copy_list);
  ASSERT_EQ(2U, copy_list->GetSize());

  Value* copy_list_element_0;
  ASSERT_TRUE(copy_list->Get(0, &copy_list_element_0));
  ASSERT_TRUE(copy_list_element_0);
  ASSERT_NE(copy_list_element_0, list_element_0_weak);
  int copy_list_element_0_value;
  ASSERT_TRUE(copy_list_element_0->GetAsInteger(&copy_list_element_0_value));
  ASSERT_EQ(0, copy_list_element_0_value);

  Value* copy_list_element_1;
  ASSERT_TRUE(copy_list->Get(1, &copy_list_element_1));
  ASSERT_TRUE(copy_list_element_1);
  ASSERT_NE(copy_list_element_1, list_element_1_weak);
  int copy_list_element_1_value;
  ASSERT_TRUE(copy_list_element_1->GetAsInteger(&copy_list_element_1_value));
  ASSERT_EQ(1, copy_list_element_1_value);

  copy_value = nullptr;
  ASSERT_TRUE(copy_dict->Get("dictionary", &copy_value));
  ASSERT_TRUE(copy_value);
  ASSERT_NE(copy_value, dict_weak);
  ASSERT_TRUE(copy_value->is_dict());
  DictionaryValue* copy_nested_dictionary = nullptr;
  ASSERT_TRUE(copy_value->GetAsDictionary(&copy_nested_dictionary));
  ASSERT_TRUE(copy_nested_dictionary);
  EXPECT_TRUE(copy_nested_dictionary->HasKey("key"));
}

TEST(ValuesTest, Equals) {
  auto null1 = std::make_unique<Value>();
  auto null2 = std::make_unique<Value>();
  EXPECT_NE(null1.get(), null2.get());
  EXPECT_EQ(*null1, *null2);

  Value boolean(false);
  EXPECT_NE(*null1, boolean);

  DictionaryValue dv;
  dv.SetBoolean("a", false);
  dv.SetInteger("b", 2);
  dv.SetDouble("c", 2.5);
  dv.SetString("d1", "string");
  dv.SetString("d2", ASCIIToUTF16("http://google.com"));
  dv.Set("e", std::make_unique<Value>());

  auto copy = dv.CreateDeepCopy();
  EXPECT_EQ(dv, *copy);

  std::unique_ptr<ListValue> list(new ListValue);
  list->Append(std::make_unique<Value>());
  list->Append(std::make_unique<DictionaryValue>());
  auto list_copy = std::make_unique<Value>(list->Clone());

  ListValue* list_weak = dv.SetList("f", std::move(list));
  EXPECT_NE(dv, *copy);
  copy->Set("f", std::move(list_copy));
  EXPECT_EQ(dv, *copy);

  list_weak->Append(std::make_unique<Value>(true));
  EXPECT_NE(dv, *copy);

  // Check if Equals detects differences in only the keys.
  copy = dv.CreateDeepCopy();
  EXPECT_EQ(dv, *copy);
  copy->Remove("a", nullptr);
  copy->SetBoolean("aa", false);
  EXPECT_NE(dv, *copy);
}

TEST(ValuesTest, Comparisons) {
  // Test None Values.
  Value null1;
  Value null2;
  EXPECT_EQ(null1, null2);
  EXPECT_FALSE(null1 != null2);
  EXPECT_FALSE(null1 < null2);
  EXPECT_FALSE(null1 > null2);
  EXPECT_LE(null1, null2);
  EXPECT_GE(null1, null2);

  // Test Bool Values.
  Value bool1(false);
  Value bool2(true);
  EXPECT_FALSE(bool1 == bool2);
  EXPECT_NE(bool1, bool2);
  EXPECT_LT(bool1, bool2);
  EXPECT_FALSE(bool1 > bool2);
  EXPECT_LE(bool1, bool2);
  EXPECT_FALSE(bool1 >= bool2);

  // Test Int Values.
  Value int1(1);
  Value int2(2);
  EXPECT_FALSE(int1 == int2);
  EXPECT_NE(int1, int2);
  EXPECT_LT(int1, int2);
  EXPECT_FALSE(int1 > int2);
  EXPECT_LE(int1, int2);
  EXPECT_FALSE(int1 >= int2);

  // Test Double Values.
  Value double1(1.0);
  Value double2(2.0);
  EXPECT_FALSE(double1 == double2);
  EXPECT_NE(double1, double2);
  EXPECT_LT(double1, double2);
  EXPECT_FALSE(double1 > double2);
  EXPECT_LE(double1, double2);
  EXPECT_FALSE(double1 >= double2);

  // Test String Values.
  Value string1("1");
  Value string2("2");
  EXPECT_FALSE(string1 == string2);
  EXPECT_NE(string1, string2);
  EXPECT_LT(string1, string2);
  EXPECT_FALSE(string1 > string2);
  EXPECT_LE(string1, string2);
  EXPECT_FALSE(string1 >= string2);

  // Test Binary Values.
  Value binary1(Value::BlobStorage{0x01});
  Value binary2(Value::BlobStorage{0x02});
  EXPECT_FALSE(binary1 == binary2);
  EXPECT_NE(binary1, binary2);
  EXPECT_LT(binary1, binary2);
  EXPECT_FALSE(binary1 > binary2);
  EXPECT_LE(binary1, binary2);
  EXPECT_FALSE(binary1 >= binary2);

  // Test Empty List Values.
  ListValue null_list1;
  ListValue null_list2;
  EXPECT_EQ(null_list1, null_list2);
  EXPECT_FALSE(null_list1 != null_list2);
  EXPECT_FALSE(null_list1 < null_list2);
  EXPECT_FALSE(null_list1 > null_list2);
  EXPECT_LE(null_list1, null_list2);
  EXPECT_GE(null_list1, null_list2);

  // Test Non Empty List Values.
  ListValue int_list1;
  ListValue int_list2;
  int_list1.AppendInteger(1);
  int_list2.AppendInteger(2);
  EXPECT_FALSE(int_list1 == int_list2);
  EXPECT_NE(int_list1, int_list2);
  EXPECT_LT(int_list1, int_list2);
  EXPECT_FALSE(int_list1 > int_list2);
  EXPECT_LE(int_list1, int_list2);
  EXPECT_FALSE(int_list1 >= int_list2);

  // Test Empty Dict Values.
  DictionaryValue null_dict1;
  DictionaryValue null_dict2;
  EXPECT_EQ(null_dict1, null_dict2);
  EXPECT_FALSE(null_dict1 != null_dict2);
  EXPECT_FALSE(null_dict1 < null_dict2);
  EXPECT_FALSE(null_dict1 > null_dict2);
  EXPECT_LE(null_dict1, null_dict2);
  EXPECT_GE(null_dict1, null_dict2);

  // Test Non Empty Dict Values.
  DictionaryValue int_dict1;
  DictionaryValue int_dict2;
  int_dict1.SetInteger("key", 1);
  int_dict2.SetInteger("key", 2);
  EXPECT_FALSE(int_dict1 == int_dict2);
  EXPECT_NE(int_dict1, int_dict2);
  EXPECT_LT(int_dict1, int_dict2);
  EXPECT_FALSE(int_dict1 > int_dict2);
  EXPECT_LE(int_dict1, int_dict2);
  EXPECT_FALSE(int_dict1 >= int_dict2);

  // Test Values of different types.
  std::vector<Value> values;
  values.emplace_back(std::move(null1));
  values.emplace_back(std::move(bool1));
  values.emplace_back(std::move(int1));
  values.emplace_back(std::move(double1));
  values.emplace_back(std::move(string1));
  values.emplace_back(std::move(binary1));
  values.emplace_back(std::move(int_dict1));
  values.emplace_back(std::move(int_list1));
  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      EXPECT_FALSE(values[i] == values[j]);
      EXPECT_NE(values[i], values[j]);
      EXPECT_LT(values[i], values[j]);
      EXPECT_FALSE(values[i] > values[j]);
      EXPECT_LE(values[i], values[j]);
      EXPECT_FALSE(values[i] >= values[j]);
    }
  }
}

TEST(ValuesTest, DeepCopyCovariantReturnTypes) {
  DictionaryValue original_dict;
  Value* null_weak = original_dict.SetKey("null", Value());
  Value* bool_weak = original_dict.SetKey("bool", Value(true));
  Value* int_weak = original_dict.SetKey("int", Value(42));
  Value* double_weak = original_dict.SetKey("double", Value(3.14));
  Value* string_weak = original_dict.SetKey("string", Value("hello"));
  Value* string16_weak =
      original_dict.SetKey("string16", Value(ASCIIToUTF16("hello16")));
  Value* binary_weak =
      original_dict.SetKey("binary", Value(Value::BlobStorage(42, '!')));

  Value::ListStorage storage;
  storage.emplace_back(0);
  storage.emplace_back(1);
  Value* list_weak = original_dict.SetKey("list", Value(std::move(storage)));

  auto copy_dict = std::make_unique<Value>(original_dict.Clone());
  auto copy_null = std::make_unique<Value>(null_weak->Clone());
  auto copy_bool = std::make_unique<Value>(bool_weak->Clone());
  auto copy_int = std::make_unique<Value>(int_weak->Clone());
  auto copy_double = std::make_unique<Value>(double_weak->Clone());
  auto copy_string = std::make_unique<Value>(string_weak->Clone());
  auto copy_string16 = std::make_unique<Value>(string16_weak->Clone());
  auto copy_binary = std::make_unique<Value>(binary_weak->Clone());
  auto copy_list = std::make_unique<Value>(list_weak->Clone());

  EXPECT_EQ(original_dict, *copy_dict);
  EXPECT_EQ(*null_weak, *copy_null);
  EXPECT_EQ(*bool_weak, *copy_bool);
  EXPECT_EQ(*int_weak, *copy_int);
  EXPECT_EQ(*double_weak, *copy_double);
  EXPECT_EQ(*string_weak, *copy_string);
  EXPECT_EQ(*string16_weak, *copy_string16);
  EXPECT_EQ(*binary_weak, *copy_binary);
  EXPECT_EQ(*list_weak, *copy_list);
}

TEST(ValuesTest, RemoveEmptyChildren) {
  auto root = std::make_unique<DictionaryValue>();
  // Remove empty lists and dictionaries.
  root->Set("empty_dict", std::make_unique<DictionaryValue>());
  root->Set("empty_list", std::make_unique<ListValue>());
  root->SetWithoutPathExpansion("a.b.c.d.e",
                                std::make_unique<DictionaryValue>());
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_TRUE(root->empty());

  // Make sure we don't prune too much.
  root->SetBoolean("bool", true);
  root->Set("empty_dict", std::make_unique<DictionaryValue>());
  root->SetString("empty_string", std::string());
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_EQ(2U, root->size());

  // Should do nothing.
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_EQ(2U, root->size());

  // Nested test cases.  These should all reduce back to the bool and string
  // set above.
  {
    root->Set("a.b.c.d.e", std::make_unique<DictionaryValue>());
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->size());
  }
  {
    auto inner = std::make_unique<DictionaryValue>();
    inner->Set("empty_dict", std::make_unique<DictionaryValue>());
    inner->Set("empty_list", std::make_unique<ListValue>());
    root->Set("dict_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->size());
  }
  {
    auto inner = std::make_unique<ListValue>();
    inner->Append(std::make_unique<DictionaryValue>());
    inner->Append(std::make_unique<ListValue>());
    root->Set("list_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->size());
  }

  // Nested with siblings.
  {
    auto inner = std::make_unique<ListValue>();
    inner->Append(std::make_unique<DictionaryValue>());
    inner->Append(std::make_unique<ListValue>());
    root->Set("list_with_empty_children", std::move(inner));
    auto inner2 = std::make_unique<DictionaryValue>();
    inner2->Set("empty_dict", std::make_unique<DictionaryValue>());
    inner2->Set("empty_list", std::make_unique<ListValue>());
    root->Set("dict_with_empty_children", std::move(inner2));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->size());
  }

  // Make sure nested values don't get pruned.
  {
    auto inner = std::make_unique<ListValue>();
    auto inner2 = std::make_unique<ListValue>();
    inner2->Append(std::make_unique<Value>("hello"));
    inner->Append(std::make_unique<DictionaryValue>());
    inner->Append(std::move(inner2));
    root->Set("list_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(3U, root->size());

    ListValue* inner_value, *inner_value2;
    EXPECT_TRUE(root->GetList("list_with_empty_children", &inner_value));
    EXPECT_EQ(1U, inner_value->GetSize());  // Dictionary was pruned.
    EXPECT_TRUE(inner_value->GetList(0, &inner_value2));
    EXPECT_EQ(1U, inner_value2->GetSize());
  }
}

TEST(ValuesTest, MergeDictionary) {
  std::unique_ptr<DictionaryValue> base(new DictionaryValue);
  base->SetString("base_key", "base_key_value_base");
  base->SetString("collide_key", "collide_key_value_base");
  std::unique_ptr<DictionaryValue> base_sub_dict(new DictionaryValue);
  base_sub_dict->SetString("sub_base_key", "sub_base_key_value_base");
  base_sub_dict->SetString("sub_collide_key", "sub_collide_key_value_base");
  base->Set("sub_dict_key", std::move(base_sub_dict));

  std::unique_ptr<DictionaryValue> merge(new DictionaryValue);
  merge->SetString("merge_key", "merge_key_value_merge");
  merge->SetString("collide_key", "collide_key_value_merge");
  std::unique_ptr<DictionaryValue> merge_sub_dict(new DictionaryValue);
  merge_sub_dict->SetString("sub_merge_key", "sub_merge_key_value_merge");
  merge_sub_dict->SetString("sub_collide_key", "sub_collide_key_value_merge");
  merge->Set("sub_dict_key", std::move(merge_sub_dict));

  base->MergeDictionary(merge.get());

  EXPECT_EQ(4U, base->size());
  std::string base_key_value;
  EXPECT_TRUE(base->GetString("base_key", &base_key_value));
  EXPECT_EQ("base_key_value_base", base_key_value); // Base value preserved.
  std::string collide_key_value;
  EXPECT_TRUE(base->GetString("collide_key", &collide_key_value));
  EXPECT_EQ("collide_key_value_merge", collide_key_value); // Replaced.
  std::string merge_key_value;
  EXPECT_TRUE(base->GetString("merge_key", &merge_key_value));
  EXPECT_EQ("merge_key_value_merge", merge_key_value); // Merged in.

  DictionaryValue* res_sub_dict;
  EXPECT_TRUE(base->GetDictionary("sub_dict_key", &res_sub_dict));
  EXPECT_EQ(3U, res_sub_dict->size());
  std::string sub_base_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_base_key", &sub_base_key_value));
  EXPECT_EQ("sub_base_key_value_base", sub_base_key_value); // Preserved.
  std::string sub_collide_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_collide_key",
                                      &sub_collide_key_value));
  EXPECT_EQ("sub_collide_key_value_merge", sub_collide_key_value); // Replaced.
  std::string sub_merge_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_merge_key", &sub_merge_key_value));
  EXPECT_EQ("sub_merge_key_value_merge", sub_merge_key_value); // Merged in.
}

TEST(ValuesTest, MergeDictionaryDeepCopy) {
  std::unique_ptr<DictionaryValue> child(new DictionaryValue);
  DictionaryValue* original_child = child.get();
  child->SetString("test", "value");
  EXPECT_EQ(1U, child->size());

  std::string value;
  EXPECT_TRUE(child->GetString("test", &value));
  EXPECT_EQ("value", value);

  std::unique_ptr<DictionaryValue> base(new DictionaryValue);
  base->Set("dict", std::move(child));
  EXPECT_EQ(1U, base->size());

  DictionaryValue* ptr;
  EXPECT_TRUE(base->GetDictionary("dict", &ptr));
  EXPECT_EQ(original_child, ptr);

  std::unique_ptr<DictionaryValue> merged(new DictionaryValue);
  merged->MergeDictionary(base.get());
  EXPECT_EQ(1U, merged->size());
  EXPECT_TRUE(merged->GetDictionary("dict", &ptr));
  EXPECT_NE(original_child, ptr);
  EXPECT_TRUE(ptr->GetString("test", &value));
  EXPECT_EQ("value", value);

  original_child->SetString("test", "overwrite");
  base.reset();
  EXPECT_TRUE(ptr->GetString("test", &value));
  EXPECT_EQ("value", value);
}

TEST(ValuesTest, DictionaryIterator) {
  DictionaryValue dict;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    ADD_FAILURE();
  }

  Value value1("value1");
  dict.SetKey("key1", value1.Clone());
  bool seen1 = false;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it.key());
    EXPECT_EQ(value1, it.value());
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  Value value2("value2");
  dict.SetKey("key2", value2.Clone());
  bool seen2 = seen1 = false;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    if (it.key() == "key1") {
      EXPECT_FALSE(seen1);
      EXPECT_EQ(value1, it.value());
      seen1 = true;
    } else if (it.key() == "key2") {
      EXPECT_FALSE(seen2);
      EXPECT_EQ(value2, it.value());
      seen2 = true;
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(seen1);
  EXPECT_TRUE(seen2);
}

TEST(ValuesTest, StdDictionaryIterator) {
  DictionaryValue dict;
  for (auto it = dict.begin(); it != dict.end(); ++it) {
    ADD_FAILURE();
  }

  Value value1("value1");
  dict.SetKey("key1", value1.Clone());
  bool seen1 = false;
  for (const auto& it : dict) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it.first);
    EXPECT_EQ(value1, *it.second);
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  Value value2("value2");
  dict.SetKey("key2", value2.Clone());
  bool seen2 = seen1 = false;
  for (const auto& it : dict) {
    if (it.first == "key1") {
      EXPECT_FALSE(seen1);
      EXPECT_EQ(value1, *it.second);
      seen1 = true;
    } else if (it.first == "key2") {
      EXPECT_FALSE(seen2);
      EXPECT_EQ(value2, *it.second);
      seen2 = true;
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(seen1);
  EXPECT_TRUE(seen2);
}

// DictionaryValue/ListValue's Get*() methods should accept NULL as an out-value
// and still return true/false based on success.
TEST(ValuesTest, GetWithNullOutValue) {
  DictionaryValue main_dict;
  ListValue main_list;

  Value bool_value(false);
  Value int_value(1234);
  Value double_value(12.34567);
  Value string_value("foo");
  Value binary_value(Value::Type::BINARY);
  DictionaryValue dict_value;
  ListValue list_value;

  main_dict.SetKey("bool", bool_value.Clone());
  main_dict.SetKey("int", int_value.Clone());
  main_dict.SetKey("double", double_value.Clone());
  main_dict.SetKey("string", string_value.Clone());
  main_dict.SetKey("binary", binary_value.Clone());
  main_dict.SetKey("dict", dict_value.Clone());
  main_dict.SetKey("list", list_value.Clone());

  main_list.Append(std::make_unique<Value>(bool_value.Clone()));
  main_list.Append(std::make_unique<Value>(int_value.Clone()));
  main_list.Append(std::make_unique<Value>(double_value.Clone()));
  main_list.Append(std::make_unique<Value>(string_value.Clone()));
  main_list.Append(std::make_unique<Value>(binary_value.Clone()));
  main_list.Append(std::make_unique<Value>(dict_value.Clone()));
  main_list.Append(std::make_unique<Value>(list_value.Clone()));

  EXPECT_TRUE(main_dict.Get("bool", nullptr));
  EXPECT_TRUE(main_dict.Get("int", nullptr));
  EXPECT_TRUE(main_dict.Get("double", nullptr));
  EXPECT_TRUE(main_dict.Get("string", nullptr));
  EXPECT_TRUE(main_dict.Get("binary", nullptr));
  EXPECT_TRUE(main_dict.Get("dict", nullptr));
  EXPECT_TRUE(main_dict.Get("list", nullptr));
  EXPECT_FALSE(main_dict.Get("DNE", nullptr));

  EXPECT_TRUE(main_dict.GetBoolean("bool", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("int", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("double", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("string", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("binary", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("dict", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("list", nullptr));
  EXPECT_FALSE(main_dict.GetBoolean("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetInteger("bool", nullptr));
  EXPECT_TRUE(main_dict.GetInteger("int", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("double", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("string", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("binary", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("dict", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("list", nullptr));
  EXPECT_FALSE(main_dict.GetInteger("DNE", nullptr));

  // Both int and double values can be obtained from GetDouble.
  EXPECT_FALSE(main_dict.GetDouble("bool", nullptr));
  EXPECT_TRUE(main_dict.GetDouble("int", nullptr));
  EXPECT_TRUE(main_dict.GetDouble("double", nullptr));
  EXPECT_FALSE(main_dict.GetDouble("string", nullptr));
  EXPECT_FALSE(main_dict.GetDouble("binary", nullptr));
  EXPECT_FALSE(main_dict.GetDouble("dict", nullptr));
  EXPECT_FALSE(main_dict.GetDouble("list", nullptr));
  EXPECT_FALSE(main_dict.GetDouble("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetString("bool", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("int", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("double", static_cast<std::string*>(nullptr)));
  EXPECT_TRUE(
      main_dict.GetString("string", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("binary", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("dict", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("list", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("DNE", static_cast<std::string*>(nullptr)));

  EXPECT_FALSE(main_dict.GetString("bool", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("int", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("double", static_cast<string16*>(nullptr)));
  EXPECT_TRUE(main_dict.GetString("string", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("binary", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("dict", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("list", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetString("DNE", static_cast<string16*>(nullptr)));

  EXPECT_FALSE(main_dict.GetBinary("bool", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("int", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("double", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("string", nullptr));
  EXPECT_TRUE(main_dict.GetBinary("binary", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("dict", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("list", nullptr));
  EXPECT_FALSE(main_dict.GetBinary("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetDictionary("bool", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("int", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("double", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("string", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("binary", nullptr));
  EXPECT_TRUE(main_dict.GetDictionary("dict", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("list", nullptr));
  EXPECT_FALSE(main_dict.GetDictionary("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetList("bool", nullptr));
  EXPECT_FALSE(main_dict.GetList("int", nullptr));
  EXPECT_FALSE(main_dict.GetList("double", nullptr));
  EXPECT_FALSE(main_dict.GetList("string", nullptr));
  EXPECT_FALSE(main_dict.GetList("binary", nullptr));
  EXPECT_FALSE(main_dict.GetList("dict", nullptr));
  EXPECT_TRUE(main_dict.GetList("list", nullptr));
  EXPECT_FALSE(main_dict.GetList("DNE", nullptr));

  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("bool", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("int", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("double", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("string", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("binary", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("dict", nullptr));
  EXPECT_TRUE(main_dict.GetWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetWithoutPathExpansion("DNE", nullptr));

  EXPECT_TRUE(main_dict.GetBooleanWithoutPathExpansion("bool", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("int", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("double", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("string", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("binary", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("dict", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetBooleanWithoutPathExpansion("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("bool", nullptr));
  EXPECT_TRUE(main_dict.GetIntegerWithoutPathExpansion("int", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("double", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("string", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("binary", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("dict", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetIntegerWithoutPathExpansion("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("bool", nullptr));
  EXPECT_TRUE(main_dict.GetDoubleWithoutPathExpansion("int", nullptr));
  EXPECT_TRUE(main_dict.GetDoubleWithoutPathExpansion("double", nullptr));
  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("string", nullptr));
  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("binary", nullptr));
  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("dict", nullptr));
  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetDoubleWithoutPathExpansion("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "bool", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "int", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "double", static_cast<std::string*>(nullptr)));
  EXPECT_TRUE(main_dict.GetStringWithoutPathExpansion(
      "string", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "binary", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "dict", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "list", static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "DNE", static_cast<std::string*>(nullptr)));

  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "bool", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "int", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "double", static_cast<string16*>(nullptr)));
  EXPECT_TRUE(main_dict.GetStringWithoutPathExpansion(
      "string", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "binary", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "dict", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "list", static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_dict.GetStringWithoutPathExpansion(
      "DNE", static_cast<string16*>(nullptr)));

  // There is no GetBinaryWithoutPathExpansion for some reason, but if there
  // were it should be tested here...

  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("bool", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("int", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("double", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("string", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("binary", nullptr));
  EXPECT_TRUE(main_dict.GetDictionaryWithoutPathExpansion("dict", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetDictionaryWithoutPathExpansion("DNE", nullptr));

  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("bool", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("int", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("double", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("string", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("binary", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("dict", nullptr));
  EXPECT_TRUE(main_dict.GetListWithoutPathExpansion("list", nullptr));
  EXPECT_FALSE(main_dict.GetListWithoutPathExpansion("DNE", nullptr));

  EXPECT_TRUE(main_list.Get(0, nullptr));
  EXPECT_TRUE(main_list.Get(1, nullptr));
  EXPECT_TRUE(main_list.Get(2, nullptr));
  EXPECT_TRUE(main_list.Get(3, nullptr));
  EXPECT_TRUE(main_list.Get(4, nullptr));
  EXPECT_TRUE(main_list.Get(5, nullptr));
  EXPECT_TRUE(main_list.Get(6, nullptr));
  EXPECT_FALSE(main_list.Get(7, nullptr));

  EXPECT_TRUE(main_list.GetBoolean(0, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(1, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(2, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(3, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(4, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(5, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(6, nullptr));
  EXPECT_FALSE(main_list.GetBoolean(7, nullptr));

  EXPECT_FALSE(main_list.GetInteger(0, nullptr));
  EXPECT_TRUE(main_list.GetInteger(1, nullptr));
  EXPECT_FALSE(main_list.GetInteger(2, nullptr));
  EXPECT_FALSE(main_list.GetInteger(3, nullptr));
  EXPECT_FALSE(main_list.GetInteger(4, nullptr));
  EXPECT_FALSE(main_list.GetInteger(5, nullptr));
  EXPECT_FALSE(main_list.GetInteger(6, nullptr));
  EXPECT_FALSE(main_list.GetInteger(7, nullptr));

  EXPECT_FALSE(main_list.GetDouble(0, nullptr));
  EXPECT_TRUE(main_list.GetDouble(1, nullptr));
  EXPECT_TRUE(main_list.GetDouble(2, nullptr));
  EXPECT_FALSE(main_list.GetDouble(3, nullptr));
  EXPECT_FALSE(main_list.GetDouble(4, nullptr));
  EXPECT_FALSE(main_list.GetDouble(5, nullptr));
  EXPECT_FALSE(main_list.GetDouble(6, nullptr));
  EXPECT_FALSE(main_list.GetDouble(7, nullptr));

  EXPECT_FALSE(main_list.GetString(0, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(1, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(2, static_cast<std::string*>(nullptr)));
  EXPECT_TRUE(main_list.GetString(3, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(4, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(5, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(6, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(7, static_cast<std::string*>(nullptr)));

  EXPECT_FALSE(main_list.GetString(0, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(1, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(2, static_cast<string16*>(nullptr)));
  EXPECT_TRUE(main_list.GetString(3, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(4, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(5, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(6, static_cast<string16*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(7, static_cast<string16*>(nullptr)));

  EXPECT_FALSE(main_list.GetDictionary(0, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(1, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(2, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(3, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(4, nullptr));
  EXPECT_TRUE(main_list.GetDictionary(5, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(6, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(7, nullptr));

  EXPECT_FALSE(main_list.GetList(0, nullptr));
  EXPECT_FALSE(main_list.GetList(1, nullptr));
  EXPECT_FALSE(main_list.GetList(2, nullptr));
  EXPECT_FALSE(main_list.GetList(3, nullptr));
  EXPECT_FALSE(main_list.GetList(4, nullptr));
  EXPECT_FALSE(main_list.GetList(5, nullptr));
  EXPECT_TRUE(main_list.GetList(6, nullptr));
  EXPECT_FALSE(main_list.GetList(7, nullptr));
}

TEST(ValuesTest, SelfSwap) {
  base::Value test(1);
  std::swap(test, test);
  EXPECT_EQ(1, test.GetInt());
}

TEST(ValuesTest, FromToUniquePtrValue) {
  std::unique_ptr<DictionaryValue> dict = std::make_unique<DictionaryValue>();
  dict->SetString("name", "Froogle");
  dict->SetString("url", "http://froogle.com");
  Value dict_copy = dict->Clone();

  Value dict_converted = Value::FromUniquePtrValue(std::move(dict));
  EXPECT_EQ(dict_copy, dict_converted);

  std::unique_ptr<Value> val =
      Value::ToUniquePtrValue(std::move(dict_converted));
  EXPECT_EQ(dict_copy, *val);
}

}  // namespace base
