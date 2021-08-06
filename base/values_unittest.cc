// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/as_const.h"
#include "base/bits.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/include/perfetto/test/traced_value_test_support.h"  // no-presubmit-check nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

// Ensure that base::Value is as small as possible, i.e. that there is
// no wasted space after the inner value due to alignment constraints.
// Distinguish between the 'header' that includes |type_| and and the inner
// value that follows it, which can be a bool, int, double, string, blob, list
// or dict.
//
// This test is only enabled when NDEBUG is defined. This way the test will not
// fail in debug builds that sometimes contain larger versions of the standard
// containers used inside base::Value.
TEST(ValuesTest, SizeOfValue) {
  static_assert(
      std::max({alignof(size_t), alignof(bool), alignof(int),
                alignof(Value::DoubleStorage), alignof(std::string),
                alignof(Value::BlobStorage), alignof(Value::ListStorage),
                alignof(Value::DictStorage)}) == alignof(Value),
      "Value does not have smallest possible alignof");

  static_assert(
      sizeof(size_t) +
              std::max({sizeof(bool), sizeof(int), sizeof(Value::DoubleStorage),
                        sizeof(std::string), sizeof(Value::BlobStorage),
                        sizeof(Value::ListStorage),
                        sizeof(Value::DictStorage)}) ==
          sizeof(Value),
      "Value does not have smallest possible sizeof");
}

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

TEST(ValuesTest, EmptyValue) {
  Value value;
  EXPECT_EQ(Value::Type::NONE, value.type());
  EXPECT_EQ(absl::nullopt, value.GetIfBool());
  EXPECT_EQ(absl::nullopt, value.GetIfInt());
  EXPECT_EQ(absl::nullopt, value.GetIfDouble());
  EXPECT_EQ(nullptr, value.GetIfString());
  EXPECT_EQ(nullptr, value.GetIfBlob());
}

// Group of tests for the value constructors.
TEST(ValuesTest, ConstructBool) {
  Value true_value(true);
  EXPECT_EQ(Value::Type::BOOLEAN, true_value.type());
  EXPECT_THAT(true_value.GetIfBool(), testing::Optional(true));
  EXPECT_TRUE(true_value.GetBool());

  Value false_value(false);
  EXPECT_EQ(Value::Type::BOOLEAN, false_value.type());
  EXPECT_THAT(false_value.GetIfBool(), testing::Optional(false));
  EXPECT_FALSE(false_value.GetBool());
}

TEST(ValuesTest, ConstructFromPtrs) {
  static_assert(!std::is_constructible<Value, int*>::value, "");
  static_assert(!std::is_constructible<Value, const int*>::value, "");
  static_assert(!std::is_constructible<Value, wchar_t*>::value, "");
  static_assert(!std::is_constructible<Value, const wchar_t*>::value, "");

  static_assert(std::is_constructible<Value, char*>::value, "");
  static_assert(std::is_constructible<Value, const char*>::value, "");
  static_assert(std::is_constructible<Value, char16_t*>::value, "");
  static_assert(std::is_constructible<Value, const char16_t*>::value, "");
}

TEST(ValuesTest, ConstructInt) {
  Value value(-37);
  EXPECT_EQ(Value::Type::INTEGER, value.type());
  EXPECT_THAT(value.GetIfInt(), testing::Optional(-37));
  EXPECT_EQ(-37, value.GetInt());

  EXPECT_THAT(value.GetIfDouble(), testing::Optional(-37.0));
  EXPECT_EQ(-37.0, value.GetDouble());
}

TEST(ValuesTest, ConstructDouble) {
  Value value(-4.655);
  EXPECT_EQ(Value::Type::DOUBLE, value.type());
  EXPECT_THAT(value.GetIfDouble(), testing::Optional(-4.655));
  EXPECT_EQ(-4.655, value.GetDouble());
}

TEST(ValuesTest, ConstructStringFromConstCharPtr) {
  const char* str = "foobar";
  Value value(str);
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_THAT(value.GetIfString(), testing::Pointee(std::string("foobar")));
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStringPiece) {
  std::string str = "foobar";
  Value value{StringPiece(str)};
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_THAT(value.GetIfString(), testing::Pointee(std::string("foobar")));
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStdStringRRef) {
  std::string str = "foobar";
  Value value(std::move(str));
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_THAT(value.GetIfString(), testing::Pointee(std::string("foobar")));
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromConstChar16Ptr) {
  std::u16string str = u"foobar";
  Value value(str.c_str());
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_THAT(value.GetIfString(), testing::Pointee(std::string("foobar")));
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructStringFromStringPiece16) {
  std::u16string str = u"foobar";
  Value value{StringPiece16(str)};
  EXPECT_EQ(Value::Type::STRING, value.type());
  EXPECT_THAT(value.GetIfString(), testing::Pointee(std::string("foobar")));
  EXPECT_EQ("foobar", value.GetString());
}

TEST(ValuesTest, ConstructBinary) {
  Value::BlobStorage blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value value(blob);
  EXPECT_EQ(Value::Type::BINARY, value.type());
  EXPECT_THAT(value.GetIfBlob(), testing::Pointee(blob));
  EXPECT_EQ(blob, value.GetBlob());
}

TEST(ValuesTest, ConstructDict) {
  DictionaryValue value;
  EXPECT_EQ(Value::Type::DICTIONARY, value.type());
}

TEST(ValuesTest, ConstructDictFromStorage) {
  Value::DictStorage storage;
  storage.emplace("foo", "bar");
  {
    Value value(storage);
    EXPECT_EQ(Value::Type::DICTIONARY, value.type());
    EXPECT_EQ(Value::Type::STRING, value.FindKey("foo")->type());
    EXPECT_EQ("bar", value.FindKey("foo")->GetString());
  }

  storage["foo"] = base::Value("baz");
  {
    Value value(std::move(storage));
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

TEST(ValuesTest, HardenTests) {
  Value value;
  ASSERT_EQ(value.type(), Value::Type::NONE);
  EXPECT_DEATH_IF_SUPPORTED(value.GetBool(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.GetInt(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.GetDouble(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.GetString(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.GetBlob(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.DictItems(), "");
  EXPECT_DEATH_IF_SUPPORTED(value.GetList(), "");
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
  storage.emplace("Int", 123);
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
  storage.emplace("Int", 123);

  Value value(std::move(storage));
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::DICTIONARY, moved_value.type());
  EXPECT_EQ(123, moved_value.FindKey("Int")->GetInt());
}

TEST(ValuesTest, MoveAssignDictionary) {
  Value::DictStorage storage;
  storage.emplace("Int", 123);

  Value blank;
  blank = Value(std::move(storage));
  EXPECT_EQ(Value::Type::DICTIONARY, blank.type());
  EXPECT_EQ(123, blank.FindKey("Int")->GetInt());
}

TEST(ValuesTest, TakeDict) {
  // Prepare a dict with a value of each type.
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);
  Value value(std::move(storage));

  // Take ownership of the dict and make sure its contents are what we expect.
  auto dict = std::move(value).TakeDict();
  EXPECT_EQ(8u, dict.size());
  EXPECT_TRUE(dict["null"].is_none());
  EXPECT_TRUE(dict["bool"].is_bool());
  EXPECT_TRUE(dict["int"].is_int());
  EXPECT_TRUE(dict["double"].is_double());
  EXPECT_TRUE(dict["string"].is_string());
  EXPECT_TRUE(dict["blob"].is_blob());
  EXPECT_TRUE(dict["list"].is_list());
  EXPECT_TRUE(dict["dict"].is_dict());

  // Validate that |value| no longer contains values.
  EXPECT_TRUE(value.DictEmpty());
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

TEST(ValuesTest, TakeList) {
  // Prepare a list with a value of each type.
  ListValue value;
  value.Append(Value(Value::Type::NONE));
  value.Append(Value(true));
  value.Append(Value(123));
  value.Append(Value(123.456));
  value.Append(Value("string"));
  value.Append(Value(Value::Type::BINARY));
  value.Append(Value(Value::Type::LIST));
  value.Append(Value(Value::Type::DICTIONARY));

  // Take ownership of the list and make sure its contents are what we expect.
  auto list = std::move(value).TakeList();
  EXPECT_EQ(8u, list.size());
  EXPECT_TRUE(list[0].is_none());
  EXPECT_TRUE(list[1].is_bool());
  EXPECT_TRUE(list[2].is_int());
  EXPECT_TRUE(list[3].is_double());
  EXPECT_TRUE(list[4].is_string());
  EXPECT_TRUE(list[5].is_blob());
  EXPECT_TRUE(list[6].is_list());
  EXPECT_TRUE(list[7].is_dict());

  // Validate that |value| no longer contains values.
  EXPECT_TRUE(value.GetList().empty());
}

TEST(ValuesTest, Append) {
  ListValue value;
  value.Append(true);
  EXPECT_TRUE(value.GetList().back().is_bool());

  value.Append(123);
  EXPECT_TRUE(value.GetList().back().is_int());

  value.Append(3.14);
  EXPECT_TRUE(value.GetList().back().is_double());

  std::string str = "foo";
  value.Append(str.c_str());
  EXPECT_TRUE(value.GetList().back().is_string());

  value.Append(StringPiece(str));
  EXPECT_TRUE(value.GetList().back().is_string());

  value.Append(std::move(str));
  EXPECT_TRUE(value.GetList().back().is_string());

  std::u16string str16 = u"bar";
  value.Append(str16.c_str());
  EXPECT_TRUE(value.GetList().back().is_string());

  value.Append(base::StringPiece16(str16));
  EXPECT_TRUE(value.GetList().back().is_string());

  value.Append(Value());
  EXPECT_TRUE(value.GetList().back().is_none());

  value.Append(Value(Value::Type::DICTIONARY));
  EXPECT_TRUE(value.GetList().back().is_dict());

  value.Append(Value(Value::Type::LIST));
  EXPECT_TRUE(value.GetList().back().is_list());
}

TEST(ValuesTest, Insert) {
  ListValue value;
  auto GetList = [&value]() -> decltype(auto) { return value.GetList(); };
  auto GetConstList = [&value] { return as_const(value).GetList(); };

  auto storage_iter = value.Insert(GetList().end(), Value(true));
  EXPECT_TRUE(GetList().begin() == storage_iter);
  EXPECT_TRUE(storage_iter->is_bool());

  auto span_iter = value.Insert(GetConstList().begin(), Value(123));
  EXPECT_TRUE(GetConstList().begin() == span_iter);
  EXPECT_TRUE(span_iter->is_int());
}

TEST(ValuesTest, EraseListIter) {
  ListValue value;
  value.Append(1);
  value.Append(2);
  value.Append(3);

  EXPECT_TRUE(value.EraseListIter(value.GetList().begin() + 1));
  EXPECT_EQ(2u, value.GetList().size());
  EXPECT_EQ(1, value.GetList()[0].GetInt());
  EXPECT_EQ(3, value.GetList()[1].GetInt());

  EXPECT_TRUE(value.EraseListIter(value.GetList().begin()));
  EXPECT_EQ(1u, value.GetList().size());
  EXPECT_EQ(3, value.GetList()[0].GetInt());

  EXPECT_TRUE(value.EraseListIter(value.GetList().begin()));
  EXPECT_TRUE(value.GetList().empty());

  EXPECT_FALSE(value.EraseListIter(value.GetList().begin()));
}

TEST(ValuesTest, EraseListValue) {
  ListValue value;
  value.Append(1);
  value.Append(2);
  value.Append(2);
  value.Append(3);

  EXPECT_EQ(2u, value.EraseListValue(Value(2)));
  EXPECT_EQ(2u, value.GetList().size());
  EXPECT_EQ(1, value.GetList()[0].GetInt());
  EXPECT_EQ(3, value.GetList()[1].GetInt());

  EXPECT_EQ(1u, value.EraseListValue(Value(1)));
  EXPECT_EQ(1u, value.GetList().size());
  EXPECT_EQ(3, value.GetList()[0].GetInt());

  EXPECT_EQ(1u, value.EraseListValue(Value(3)));
  EXPECT_TRUE(value.GetList().empty());

  EXPECT_EQ(0u, value.EraseListValue(Value(3)));
}

TEST(ValuesTest, EraseListValueIf) {
  ListValue value;
  value.Append(1);
  value.Append(2);
  value.Append(2);
  value.Append(3);

  EXPECT_EQ(3u, value.EraseListValueIf(
                    [](const auto& val) { return val >= Value(2); }));
  EXPECT_EQ(1u, value.GetList().size());
  EXPECT_EQ(1, value.GetList()[0].GetInt());

  EXPECT_EQ(1u, value.EraseListValueIf([](const auto& val) { return true; }));
  EXPECT_TRUE(value.GetList().empty());

  EXPECT_EQ(0u, value.EraseListValueIf([](const auto& val) { return true; }));
}

TEST(ValuesTest, ClearList) {
  ListValue value;
  value.Append(1);
  value.Append(2);
  value.Append(3);
  EXPECT_EQ(3u, value.GetList().size());

  value.ClearList();
  EXPECT_TRUE(value.GetList().empty());

  // ClearList() should be idempotent.
  value.ClearList();
  EXPECT_TRUE(value.GetList().empty());
}

TEST(ValuesTest, FindKey) {
  Value::DictStorage storage;
  storage.emplace("foo", "bar");
  Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKey("foo"));
  EXPECT_EQ(nullptr, dict.FindKey("baz"));

  // Single not found key.
  bool found = dict.FindKey("notfound");
  EXPECT_FALSE(found);
}

TEST(ValuesTest, FindKeyChangeValue) {
  Value::DictStorage storage;
  storage.emplace("foo", "bar");
  Value dict(std::move(storage));
  Value* found = dict.FindKey("foo");
  EXPECT_NE(nullptr, found);
  EXPECT_EQ("bar", found->GetString());

  *found = Value(123);
  EXPECT_EQ(123, dict.FindKey("foo")->GetInt());
}

TEST(ValuesTest, FindKeyConst) {
  Value::DictStorage storage;
  storage.emplace("foo", "bar");
  const Value dict(std::move(storage));
  EXPECT_NE(nullptr, dict.FindKey("foo"));
  EXPECT_EQ(nullptr, dict.FindKey("baz"));
}

TEST(ValuesTest, FindKeyOfType) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

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
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

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

TEST(ValuesTest, FindBoolKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("null"));
  EXPECT_NE(absl::nullopt, dict.FindBoolKey("bool"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("int"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("double"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("string"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("blob"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("list"));
  EXPECT_EQ(absl::nullopt, dict.FindBoolKey("dict"));
}

TEST(ValuesTest, FindIntKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("null"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("bool"));
  EXPECT_NE(absl::nullopt, dict.FindIntKey("int"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("double"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("string"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("blob"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("list"));
  EXPECT_EQ(absl::nullopt, dict.FindIntKey("dict"));
}

TEST(ValuesTest, FindDoubleKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("null"));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("bool"));
  EXPECT_NE(absl::nullopt, dict.FindDoubleKey("int"));
  EXPECT_NE(absl::nullopt, dict.FindDoubleKey("double"));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("string"));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("blob"));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("list"));
  EXPECT_EQ(absl::nullopt, dict.FindDoubleKey("dict"));
}

TEST(ValuesTest, FindStringKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(nullptr, dict.FindStringKey("null"));
  EXPECT_EQ(nullptr, dict.FindStringKey("bool"));
  EXPECT_EQ(nullptr, dict.FindStringKey("int"));
  EXPECT_EQ(nullptr, dict.FindStringKey("double"));
  EXPECT_NE(nullptr, dict.FindStringKey("string"));
  EXPECT_EQ(nullptr, dict.FindStringKey("blob"));
  EXPECT_EQ(nullptr, dict.FindStringKey("list"));
  EXPECT_EQ(nullptr, dict.FindStringKey("dict"));
}

TEST(ValuesTest, MutableFindStringKey) {
  Value::DictStorage storage;
  storage.emplace("string", "foo");
  Value dict(std::move(storage));

  *(dict.FindStringKey("string")) = "bar";

  Value::DictStorage expected_storage;
  expected_storage.emplace("string", "bar");
  Value expected_dict(std::move(expected_storage));

  EXPECT_EQ(expected_dict, dict);
}

TEST(ValuesTest, FindDictKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(nullptr, dict.FindDictKey("null"));
  EXPECT_EQ(nullptr, dict.FindDictKey("bool"));
  EXPECT_EQ(nullptr, dict.FindDictKey("int"));
  EXPECT_EQ(nullptr, dict.FindDictKey("double"));
  EXPECT_EQ(nullptr, dict.FindDictKey("string"));
  EXPECT_EQ(nullptr, dict.FindDictKey("blob"));
  EXPECT_EQ(nullptr, dict.FindDictKey("list"));
  EXPECT_NE(nullptr, dict.FindDictKey("dict"));
}

TEST(ValuesTest, FindListKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(nullptr, dict.FindListKey("null"));
  EXPECT_EQ(nullptr, dict.FindListKey("bool"));
  EXPECT_EQ(nullptr, dict.FindListKey("int"));
  EXPECT_EQ(nullptr, dict.FindListKey("double"));
  EXPECT_EQ(nullptr, dict.FindListKey("string"));
  EXPECT_EQ(nullptr, dict.FindListKey("blob"));
  EXPECT_NE(nullptr, dict.FindListKey("list"));
  EXPECT_EQ(nullptr, dict.FindListKey("dict"));
}

TEST(ValuesTest, FindBlobKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

  const Value dict(std::move(storage));
  EXPECT_EQ(nullptr, dict.FindBlobKey("null"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("bool"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("int"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("double"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("string"));
  EXPECT_NE(nullptr, dict.FindBlobKey("blob"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("list"));
  EXPECT_EQ(nullptr, dict.FindBlobKey("dict"));
}

TEST(ValuesTest, SetKey) {
  Value::DictStorage storage;
  storage.emplace("null", Value::Type::NONE);
  storage.emplace("bool", Value::Type::BOOLEAN);
  storage.emplace("int", Value::Type::INTEGER);
  storage.emplace("double", Value::Type::DOUBLE);
  storage.emplace("string", Value::Type::STRING);
  storage.emplace("blob", Value::Type::BINARY);
  storage.emplace("list", Value::Type::LIST);
  storage.emplace("dict", Value::Type::DICTIONARY);

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

TEST(ValuesTest, SetBoolKey) {
  absl::optional<bool> value;

  DictionaryValue dict;
  dict.SetBoolKey("true_key", true);
  dict.SetBoolKey("false_key", false);

  value = dict.FindBoolKey("true_key");
  ASSERT_TRUE(value);
  ASSERT_TRUE(*value);

  value = dict.FindBoolKey("false_key");
  ASSERT_TRUE(value);
  ASSERT_FALSE(*value);

  value = dict.FindBoolKey("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, SetIntKey) {
  absl::optional<int> value;

  DictionaryValue dict;
  dict.SetIntKey("one_key", 1);
  dict.SetIntKey("minus_one_key", -1);

  value = dict.FindIntKey("one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ(1, *value);

  value = dict.FindIntKey("minus_one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ(-1, *value);

  value = dict.FindIntKey("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, SetDoubleKey) {
  DictionaryValue dict;
  dict.SetDoubleKey("one_key", 1.0);
  dict.SetDoubleKey("minus_one_key", -1.0);
  dict.SetDoubleKey("pi_key", 3.1415);

  // NOTE: Use FindKey() instead of FindDoubleKey() because the latter will
  // auto-convert integers to doubles as well.
  const Value* value;

  value = dict.FindKey("one_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(1.0, value->GetDouble());

  value = dict.FindKey("minus_one_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(-1.0, value->GetDouble());

  value = dict.FindKey("pi_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(3.1415, value->GetDouble());
}

TEST(ValuesTest, SetStringKey) {
  DictionaryValue dict;
  dict.SetStringKey("one_key", "one");
  dict.SetStringKey("hello_key", "hello world");

  std::string movable_value("movable_value");
  dict.SetStringKey("movable_key", std::move(movable_value));
  ASSERT_TRUE(movable_value.empty());

  const std::string* value;

  value = dict.FindStringKey("one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("one", *value);

  value = dict.FindStringKey("hello_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("hello world", *value);

  value = dict.FindStringKey("movable_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("movable_value", *value);

  value = dict.FindStringKey("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, FindPath) {
  // Construct a dictionary path {root}.foo.bar = 123
  Value foo(Value::Type::DICTIONARY);
  foo.SetKey("bar", Value(123));

  Value root(Value::Type::DICTIONARY);
  root.SetKey("foo", std::move(foo));

  // No key (stupid but well-defined and takes work to prevent).
  Value* found = root.FindPath("");
  EXPECT_EQ(&root, found);

  // Double key, second not found.
  found = root.FindPath("foo.notfound");
  EXPECT_FALSE(found);

  // Double key, found.
  found = root.FindPath("foo.bar");
  EXPECT_TRUE(found);
  EXPECT_TRUE(found->is_int());
  EXPECT_EQ(123, found->GetInt());
}

TEST(ValuesTest, SetPath) {
  Value root(Value::Type::DICTIONARY);

  Value* inserted = root.SetPath("one.two", Value(123));
  Value* found = root.FindPathOfType("one.two", Value::Type::INTEGER);
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  inserted = root.SetPath("foo.bar", Value(123));
  found = root.FindPathOfType("foo.bar", Value::Type::INTEGER);
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  // Overwrite with a different value.
  root.SetPath("foo.bar", Value("hello"));
  found = root.FindPathOfType("foo.bar", Value::Type::STRING);
  ASSERT_TRUE(found);
  EXPECT_EQ("hello", found->GetString());

  // Can't change existing non-dictionary keys to dictionaries.
  found = root.SetPath("foo.bar.baz", Value(123));
  EXPECT_FALSE(found);
}

TEST(ValuesTest, SetBoolPath) {
  DictionaryValue root;
  Value* inserted = root.SetBoolPath("foo.bar", true);
  Value* found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_bool());
  EXPECT_TRUE(found->GetBool());

  // Overwrite with a different value.
  root.SetBoolPath("foo.bar", false);
  found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_bool());
  EXPECT_FALSE(found->GetBool());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetBoolPath("foo.bar.zoo", true));
}

TEST(ValuesTest, SetIntPath) {
  DictionaryValue root;
  Value* inserted = root.SetIntPath("foo.bar", 123);
  Value* found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_int());
  EXPECT_EQ(123, found->GetInt());

  // Overwrite with a different value.
  root.SetIntPath("foo.bar", 234);
  found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_int());
  EXPECT_EQ(234, found->GetInt());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetIntPath("foo.bar.zoo", 567));
}

TEST(ValuesTest, SetDoublePath) {
  DictionaryValue root;
  Value* inserted = root.SetDoublePath("foo.bar", 1.23);
  Value* found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_double());
  EXPECT_EQ(1.23, found->GetDouble());

  // Overwrite with a different value.
  root.SetDoublePath("foo.bar", 2.34);
  found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_double());
  EXPECT_EQ(2.34, found->GetDouble());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetDoublePath("foo.bar.zoo", 5.67));
}

TEST(ValuesTest, SetStringPath) {
  DictionaryValue root;
  Value* inserted = root.SetStringPath("foo.bar", "hello world");
  Value* found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_string());
  EXPECT_EQ("hello world", found->GetString());

  // Overwrite with a different value.
  root.SetStringPath("foo.bar", "bonjour monde");
  found = root.FindPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_string());
  EXPECT_EQ("bonjour monde", found->GetString());

  ASSERT_TRUE(root.SetStringPath("foo.bar", StringPiece("rah rah")));
  ASSERT_TRUE(root.SetStringPath("foo.bar", std::string("temp string")));
  ASSERT_TRUE(root.SetStringPath("foo.bar", u"temp string"));

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetStringPath("foo.bar.zoo", "ola mundo"));
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

TEST(ValuesTest, ExtractKey) {
  Value root(Value::Type::DICTIONARY);
  root.SetKey("one", Value(123));

  // Extraction of missing key should fail.
  EXPECT_EQ(absl::nullopt, root.ExtractKey("two"));

  // Extraction of existing key should succeed.
  EXPECT_EQ(Value(123), root.ExtractKey("one"));

  // Second extraction of previously existing key should fail.
  EXPECT_EQ(absl::nullopt, root.ExtractKey("one"));
}

TEST(ValuesTest, RemovePath) {
  Value root(Value::Type::DICTIONARY);
  root.SetPath("one.two.three", Value(123));

  // Removal of missing key should fail.
  EXPECT_FALSE(root.RemovePath("one.two.four"));

  // Removal of existing key should succeed.
  EXPECT_TRUE(root.RemovePath("one.two.three"));

  // Second removal of previously existing key should fail.
  EXPECT_FALSE(root.RemovePath("one.two.three"));

  // Intermediate empty dictionaries should be cleared.
  EXPECT_EQ(nullptr, root.FindKey("one"));

  root.SetPath("one.two.three", Value(123));
  root.SetPath("one.two.four", Value(124));

  EXPECT_TRUE(root.RemovePath("one.two.three"));
  // Intermediate non-empty dictionaries should be kept.
  EXPECT_NE(nullptr, root.FindKey("one"));
  EXPECT_NE(nullptr, root.FindPath("one.two"));
  EXPECT_NE(nullptr, root.FindPath("one.two.four"));
}

TEST(ValuesTest, ExtractPath) {
  Value root(Value::Type::DICTIONARY);
  root.SetPath("one.two.three", Value(123));

  // Extraction of missing key should fail.
  EXPECT_EQ(absl::nullopt, root.ExtractPath("one.two.four"));

  // Extraction of existing key should succeed.
  EXPECT_EQ(Value(123), root.ExtractPath("one.two.three"));

  // Second extraction of previously existing key should fail.
  EXPECT_EQ(absl::nullopt, root.ExtractPath("one.two.three"));

  // Intermediate empty dictionaries should be cleared.
  EXPECT_EQ(nullptr, root.FindKey("one"));

  root.SetPath("one.two.three", Value(123));
  root.SetPath("one.two.four", Value(124));

  EXPECT_EQ(Value(123), root.ExtractPath("one.two.three"));
  // Intermediate non-empty dictionaries should be kept.
  EXPECT_NE(nullptr, root.FindKey("one"));
  EXPECT_NE(nullptr, root.FindPath("one.two"));
  EXPECT_NE(nullptr, root.FindPath("one.two.four"));
}

TEST(ValuesTest, Basic) {
  // Test basic dictionary getting/setting
  DictionaryValue settings;
  ASSERT_FALSE(settings.FindPath("global.homepage"));

  ASSERT_FALSE(settings.FindKey("global"));
  settings.SetKey("global", Value(true));
  ASSERT_TRUE(settings.FindKey("global"));
  settings.RemoveKey("global");
  settings.SetPath("global.homepage", Value("http://scurvy.com"));
  ASSERT_TRUE(settings.FindKey("global"));
  const std::string* homepage = settings.FindStringPath("global.homepage");
  ASSERT_TRUE(homepage);
  ASSERT_EQ(std::string("http://scurvy.com"), *homepage);

  // Test storing a dictionary in a list.
  ASSERT_FALSE(settings.FindPath("global.toolbar.bookmarks"));

  ListValue new_toolbar_bookmarks;
  settings.SetPath("global.toolbar.bookmarks",
                   std::move(new_toolbar_bookmarks));
  Value* toolbar_bookmarks = settings.FindListPath("global.toolbar.bookmarks");
  ASSERT_TRUE(toolbar_bookmarks);

  DictionaryValue new_bookmark;
  new_bookmark.SetKey("name", Value("Froogle"));
  new_bookmark.SetKey("url", Value("http://froogle.com"));
  toolbar_bookmarks->Append(std::move(new_bookmark));

  Value* bookmark_list = settings.FindPath("global.toolbar.bookmarks");
  ASSERT_TRUE(bookmark_list);
  ASSERT_EQ(1U, bookmark_list->GetList().size());
  Value* bookmark = &bookmark_list->GetList()[0];
  ASSERT_TRUE(bookmark);
  ASSERT_TRUE(bookmark->is_dict());
  const std::string* bookmark_name = bookmark->FindStringKey("name");
  ASSERT_TRUE(bookmark_name);
  ASSERT_EQ(std::string("Froogle"), *bookmark_name);
  const std::string* bookmark_url = bookmark->FindStringKey("url");
  ASSERT_TRUE(bookmark_url);
  ASSERT_EQ(std::string("http://froogle.com"), *bookmark_url);
}

TEST(ValuesTest, List) {
  Value mixed_list(Value::Type::LIST);
  mixed_list.Append(true);
  mixed_list.Append(42);
  mixed_list.Append(88.8);
  mixed_list.Append("foo");

  Value::ConstListView list_view = mixed_list.GetList();
  ASSERT_EQ(4u, list_view.size());

  ASSERT_FALSE(list_view[0].is_int());
  ASSERT_FALSE(list_view[1].is_bool());
  ASSERT_FALSE(list_view[2].is_string());
  ASSERT_FALSE(list_view[2].is_int());
  ASSERT_FALSE(list_view[2].is_bool());

  ASSERT_TRUE(list_view[0].is_bool());
  ASSERT_TRUE(list_view[1].is_int());
  ASSERT_EQ(42, list_view[1].GetInt());
  // Implicit conversion from Integer to Double should be possible.
  ASSERT_EQ(42, list_view[1].GetDouble());
  ASSERT_EQ(88.8, list_view[2].GetDouble());
  ASSERT_EQ("foo", list_view[3].GetString());

  // Try searching in the mixed list.
  ASSERT_TRUE(Contains(list_view, base::Value(42)));
  ASSERT_FALSE(Contains(list_view, base::Value(false)));
}

TEST(ValuesTest, BinaryValue) {
  // Default constructor creates a BinaryValue with a buffer of size 0.
  Value binary(Value::Type::BINARY);
  ASSERT_TRUE(binary.GetBlob().empty());

  // Test the common case of a non-empty buffer
  Value::BlobStorage buffer(15);
  uint8_t* original_buffer = buffer.data();
  binary = Value(std::move(buffer));
  ASSERT_TRUE(binary.GetBlob().data());
  ASSERT_EQ(original_buffer, binary.GetBlob().data());
  ASSERT_EQ(15U, binary.GetBlob().size());

  char stack_buffer[42];
  memset(stack_buffer, '!', 42);
  binary = Value(Value::BlobStorage(stack_buffer, stack_buffer + 42));
  ASSERT_TRUE(binary.GetBlob().data());
  ASSERT_NE(stack_buffer,
            reinterpret_cast<const char*>(binary.GetBlob().data()));
  ASSERT_EQ(42U, binary.GetBlob().size());
  ASSERT_EQ(0, memcmp(stack_buffer, binary.GetBlob().data(),
                      binary.GetBlob().size()));
}

TEST(ValuesTest, StringValue) {
  // Test overloaded StringValue constructor.
  std::unique_ptr<Value> narrow_value(new Value("narrow"));
  ASSERT_TRUE(narrow_value.get());
  ASSERT_TRUE(narrow_value->is_string());
  std::unique_ptr<Value> utf16_value(new Value(u"utf16"));
  ASSERT_TRUE(utf16_value.get());
  ASSERT_TRUE(utf16_value->is_string());

  // Test overloaded GetAsString.
  std::string narrow = "http://google.com";
  std::u16string utf16 = u"http://google.com";
  const Value* string_value = nullptr;
  ASSERT_TRUE(narrow_value->GetAsString(&narrow));
  ASSERT_TRUE(narrow_value->GetAsString(&utf16));
  ASSERT_TRUE(narrow_value->GetAsString(&string_value));
  ASSERT_EQ(std::string("narrow"), narrow);
  ASSERT_EQ(u"narrow", utf16);
  ASSERT_EQ(string_value->GetString(), narrow);

  ASSERT_TRUE(utf16_value->GetAsString(&narrow));
  ASSERT_TRUE(utf16_value->GetAsString(&utf16));
  ASSERT_TRUE(utf16_value->GetAsString(&string_value));
  ASSERT_EQ(std::string("utf16"), narrow);
  ASSERT_EQ(u"utf16", utf16);
  ASSERT_EQ(string_value->GetString(), narrow);

  // Don't choke on NULL values.
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<std::u16string*>(nullptr)));
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<std::string*>(nullptr)));
  ASSERT_TRUE(narrow_value->GetAsString(static_cast<const Value**>(nullptr)));
}

TEST(ValuesTest, ListDeletion) {
  ListValue list;
  list.Append(std::make_unique<Value>());
  EXPECT_FALSE(list.GetList().empty());
  list.ClearList();
  EXPECT_TRUE(list.GetList().empty());
}

TEST(ValuesTest, DictionaryDeletion) {
  std::string key = "test";
  DictionaryValue dict;
  dict.Set(key, std::make_unique<Value>());
  EXPECT_FALSE(dict.DictEmpty());
  EXPECT_FALSE(dict.DictEmpty());
  EXPECT_EQ(1U, dict.DictSize());
  dict.DictClear();
  EXPECT_TRUE(dict.DictEmpty());
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
    Value* blank_ptr = dict.SetKey("foo.bar", base::Value());
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
    Value* string16_ptr = dict.SetString("foo.bar", u"baz");
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

TEST(ValuesTest, DictionaryWithoutPathExpansion) {
  DictionaryValue dict;
  dict.Set("this.is.expanded", std::make_unique<Value>());
  dict.SetKey("this.isnt.expanded", Value());

  EXPECT_FALSE(dict.HasKey("this.is.expanded"));
  EXPECT_TRUE(dict.HasKey("this"));
  Value* value1;
  EXPECT_TRUE(dict.Get("this", &value1));
  DictionaryValue* value2;
  ASSERT_TRUE(dict.GetDictionaryWithoutPathExpansion("this", &value2));
  EXPECT_EQ(value1, value2);
  EXPECT_EQ(1U, value2->DictSize());

  EXPECT_TRUE(dict.HasKey("this.isnt.expanded"));
  Value* value3;
  EXPECT_FALSE(dict.Get("this.isnt.expanded", &value3));
  Value* value4 = dict.FindKey("this.isnt.expanded");
  ASSERT_TRUE(value4);
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
  EXPECT_EQ(1U, value2->DictSize());

  EXPECT_TRUE(dict.HasKey("this.isnt.expanded"));
  Value* value3;
  EXPECT_FALSE(dict.Get("this.isnt.expanded", &value3));
  Value* value4 = dict.FindKey("this.isnt.expanded");
  ASSERT_TRUE(value4);
  EXPECT_EQ(Value::Type::NONE, value4->type());
}

TEST(ValuesTest, DeepCopy) {
  DictionaryValue original_dict;
  Value* null_weak = original_dict.SetKey("null", Value());
  Value* bool_weak = original_dict.SetKey("bool", Value(true));
  Value* int_weak = original_dict.SetKey("int", Value(42));
  Value* double_weak = original_dict.SetKey("double", Value(3.14));
  Value* string_weak = original_dict.SetKey("string", Value("hello"));
  Value* string16_weak = original_dict.SetKey("string16", Value(u"hello16"));

  Value* binary_weak =
      original_dict.SetKey("binary", Value(Value::BlobStorage(42, '!')));

  Value::ListStorage storage;
  storage.emplace_back(0);
  storage.emplace_back(1);
  Value* list_weak = original_dict.SetKey("list", Value(std::move(storage)));
  Value* list_element_0_weak = &list_weak->GetList()[0];
  Value* list_element_1_weak = &list_weak->GetList()[1];

  DictionaryValue* dict_weak = original_dict.SetDictionary(
      "dictionary", std::make_unique<DictionaryValue>());
  dict_weak->SetStringKey("key", "value");

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
  ASSERT_TRUE(copy_bool->GetBool());

  Value* copy_int = nullptr;
  ASSERT_TRUE(copy_dict->Get("int", &copy_int));
  ASSERT_TRUE(copy_int);
  ASSERT_NE(copy_int, int_weak);
  ASSERT_TRUE(copy_int->is_int());
  ASSERT_EQ(42, copy_int->GetInt());

  Value* copy_double = nullptr;
  ASSERT_TRUE(copy_dict->Get("double", &copy_double));
  ASSERT_TRUE(copy_double);
  ASSERT_NE(copy_double, double_weak);
  ASSERT_TRUE(copy_double->is_double());
  ASSERT_EQ(3.14, copy_double->GetDouble());

  Value* copy_string = nullptr;
  ASSERT_TRUE(copy_dict->Get("string", &copy_string));
  ASSERT_TRUE(copy_string);
  ASSERT_NE(copy_string, string_weak);
  ASSERT_TRUE(copy_string->is_string());
  std::string copy_string_value;
  std::u16string copy_string16_value;
  ASSERT_TRUE(copy_string->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello"), copy_string_value);
  ASSERT_EQ(u"hello", copy_string16_value);

  Value* copy_string16 = nullptr;
  ASSERT_TRUE(copy_dict->Get("string16", &copy_string16));
  ASSERT_TRUE(copy_string16);
  ASSERT_NE(copy_string16, string16_weak);
  ASSERT_TRUE(copy_string16->is_string());
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello16"), copy_string_value);
  ASSERT_EQ(u"hello16", copy_string16_value);

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
  ASSERT_TRUE(copy_list_element_0->is_int());
  ASSERT_EQ(0, copy_list_element_0->GetInt());

  Value* copy_list_element_1;
  ASSERT_TRUE(copy_list->Get(1, &copy_list_element_1));
  ASSERT_TRUE(copy_list_element_1);
  ASSERT_NE(copy_list_element_1, list_element_1_weak);
  ASSERT_TRUE(copy_list_element_1->is_int());
  ASSERT_EQ(1, copy_list_element_1->GetInt());

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
  dv.SetBoolKey("a", false);
  dv.SetIntKey("b", 2);
  dv.SetDoubleKey("c", 2.5);
  dv.SetStringKey("d1", "string");
  dv.SetStringKey("d2", u"http://google.com");
  dv.SetKey("e", Value());

  auto copy = dv.CreateDeepCopy();
  EXPECT_EQ(dv, *copy);

  std::unique_ptr<ListValue> list(new ListValue);
  list->Append(std::make_unique<Value>());
  list->Append(std::make_unique<DictionaryValue>());
  Value list_copy(list->Clone());

  ListValue* list_weak = dv.SetList("f", std::move(list));
  EXPECT_NE(dv, *copy);
  copy->SetKey("f", std::move(list_copy));
  EXPECT_EQ(dv, *copy);

  list_weak->Append(std::make_unique<Value>(true));
  EXPECT_NE(dv, *copy);

  // Check if Equals detects differences in only the keys.
  copy = dv.CreateDeepCopy();
  EXPECT_EQ(dv, *copy);
  copy->RemoveKey("a");
  copy->SetBoolKey("aa", false);
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
  int_dict1.SetIntKey("key", 1);
  int_dict2.SetIntKey("key", 2);
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
  Value* string16_weak = original_dict.SetKey("string16", Value(u"hello16"));
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
  root->SetKey("empty_dict", DictionaryValue());
  root->SetKey("empty_list", ListValue());
  root->SetPath("a.b.c.d.e", DictionaryValue());
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_TRUE(root->DictEmpty());

  // Make sure we don't prune too much.
  root->SetBoolKey("bool", true);
  root->SetKey("empty_dict", DictionaryValue());
  root->SetStringKey("empty_string", std::string());
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_EQ(2U, root->DictSize());

  // Should do nothing.
  root = root->DeepCopyWithoutEmptyChildren();
  EXPECT_EQ(2U, root->DictSize());

  // Nested test cases.  These should all reduce back to the bool and string
  // set above.
  {
    root->SetPath("a.b.c.d.e", DictionaryValue());
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->DictSize());
  }
  {
    Value inner(Value::Type::DICTIONARY);
    inner.SetKey("empty_dict", DictionaryValue());
    inner.SetKey("empty_list", ListValue());
    root->SetKey("dict_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->DictSize());
  }
  {
    ListValue inner;
    inner.Append(std::make_unique<DictionaryValue>());
    inner.Append(std::make_unique<ListValue>());
    root->SetKey("list_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->DictSize());
  }

  // Nested with siblings.
  {
    ListValue inner;
    inner.Append(std::make_unique<DictionaryValue>());
    inner.Append(std::make_unique<ListValue>());
    root->SetKey("list_with_empty_children", std::move(inner));
    DictionaryValue inner2;
    inner2.SetKey("empty_dict", DictionaryValue());
    inner2.SetKey("empty_list", ListValue());
    root->SetKey("dict_with_empty_children", std::move(inner2));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(2U, root->DictSize());
  }

  // Make sure nested values don't get pruned.
  {
    ListValue inner;
    auto inner2 = std::make_unique<ListValue>();
    inner2->Append(std::make_unique<Value>("hello"));
    inner.Append(std::make_unique<DictionaryValue>());
    inner.Append(std::move(inner2));
    root->SetKey("list_with_empty_children", std::move(inner));
    root = root->DeepCopyWithoutEmptyChildren();
    EXPECT_EQ(3U, root->DictSize());

    ListValue* inner_value;
    EXPECT_TRUE(root->GetList("list_with_empty_children", &inner_value));
    ASSERT_EQ(1U, inner_value->GetSize());  // Dictionary was pruned.
    const Value& inner_value2 = inner_value->GetList()[0];
    ASSERT_TRUE(inner_value2.is_list());
    EXPECT_EQ(1U, inner_value2.GetList().size());
  }
}

TEST(ValuesTest, MergeDictionary) {
  std::unique_ptr<DictionaryValue> base(new DictionaryValue);
  base->SetStringKey("base_key", "base_key_value_base");
  base->SetStringKey("collide_key", "collide_key_value_base");
  DictionaryValue base_sub_dict;
  base_sub_dict.SetStringKey("sub_base_key", "sub_base_key_value_base");
  base_sub_dict.SetStringKey("sub_collide_key", "sub_collide_key_value_base");
  base->SetKey("sub_dict_key", std::move(base_sub_dict));

  std::unique_ptr<DictionaryValue> merge(new DictionaryValue);
  merge->SetStringKey("merge_key", "merge_key_value_merge");
  merge->SetStringKey("collide_key", "collide_key_value_merge");
  DictionaryValue merge_sub_dict;
  merge_sub_dict.SetStringKey("sub_merge_key", "sub_merge_key_value_merge");
  merge_sub_dict.SetStringKey("sub_collide_key", "sub_collide_key_value_merge");
  merge->SetKey("sub_dict_key", std::move(merge_sub_dict));

  base->MergeDictionary(merge.get());

  EXPECT_EQ(4U, base->DictSize());
  std::string base_key_value;
  EXPECT_TRUE(base->GetString("base_key", &base_key_value));
  EXPECT_EQ("base_key_value_base", base_key_value);  // Base value preserved.
  std::string collide_key_value;
  EXPECT_TRUE(base->GetString("collide_key", &collide_key_value));
  EXPECT_EQ("collide_key_value_merge", collide_key_value);  // Replaced.
  std::string merge_key_value;
  EXPECT_TRUE(base->GetString("merge_key", &merge_key_value));
  EXPECT_EQ("merge_key_value_merge", merge_key_value);  // Merged in.

  DictionaryValue* res_sub_dict;
  EXPECT_TRUE(base->GetDictionary("sub_dict_key", &res_sub_dict));
  EXPECT_EQ(3U, res_sub_dict->DictSize());
  std::string sub_base_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_base_key", &sub_base_key_value));
  EXPECT_EQ("sub_base_key_value_base", sub_base_key_value);  // Preserved.
  std::string sub_collide_key_value;
  EXPECT_TRUE(
      res_sub_dict->GetString("sub_collide_key", &sub_collide_key_value));
  EXPECT_EQ("sub_collide_key_value_merge", sub_collide_key_value);  // Replaced.
  std::string sub_merge_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_merge_key", &sub_merge_key_value));
  EXPECT_EQ("sub_merge_key_value_merge", sub_merge_key_value);  // Merged in.
}

TEST(ValuesTest, MergeDictionaryDeepCopy) {
  std::unique_ptr<DictionaryValue> child(new DictionaryValue);
  DictionaryValue* original_child = child.get();
  child->SetStringKey("test", "value");
  EXPECT_EQ(1U, child->DictSize());

  std::string value;
  EXPECT_TRUE(child->GetString("test", &value));
  EXPECT_EQ("value", value);

  std::unique_ptr<DictionaryValue> base(new DictionaryValue);
  base->Set("dict", std::move(child));
  EXPECT_EQ(1U, base->DictSize());

  DictionaryValue* ptr;
  EXPECT_TRUE(base->GetDictionary("dict", &ptr));
  EXPECT_EQ(original_child, ptr);

  std::unique_ptr<DictionaryValue> merged(new DictionaryValue);
  merged->MergeDictionary(base.get());
  EXPECT_EQ(1U, merged->DictSize());
  EXPECT_TRUE(merged->GetDictionary("dict", &ptr));
  EXPECT_NE(original_child, ptr);
  EXPECT_TRUE(ptr->GetString("test", &value));
  EXPECT_EQ("value", value);

  original_child->SetStringKey("test", "overwrite");
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

TEST(ValuesTest, MutatingCopiedPairsInDictItemsMutatesUnderlyingValues) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("key", Value("initial value"));

  // Because the non-const DictItems() iterates over
  // <const std::string&, Value&> pairs, it's possible
  // to alter iterated-over values in place even when
  // "copying" the key-value pair:
  for (auto kv : dict.DictItems())
    kv.second.GetString() = "replacement";

  std::string* found = dict.FindStringKey("key");
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, "replacement");
}

TEST(ValuesTest, StdDictionaryIterator) {
  DictionaryValue dict;
  for (auto it = dict.DictItems().begin(); it != dict.DictItems().end(); ++it) {
    ADD_FAILURE();
  }

  Value value1("value1");
  dict.SetKey("key1", value1.Clone());
  bool seen1 = false;
  for (auto it : dict.DictItems()) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it.first);
    EXPECT_EQ(value1, it.second);
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  Value value2("value2");
  dict.SetKey("key2", value2.Clone());
  bool seen2 = seen1 = false;
  for (auto it : dict.DictItems()) {
    if (it.first == "key1") {
      EXPECT_FALSE(seen1);
      EXPECT_EQ(value1, it.second);
      seen1 = true;
    } else if (it.first == "key2") {
      EXPECT_FALSE(seen2);
      EXPECT_EQ(value2, it.second);
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

  EXPECT_FALSE(
      main_dict.GetString("bool", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("int", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("double", static_cast<std::u16string*>(nullptr)));
  EXPECT_TRUE(
      main_dict.GetString("string", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("binary", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("dict", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("list", static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(
      main_dict.GetString("DNE", static_cast<std::u16string*>(nullptr)));

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

  EXPECT_FALSE(main_list.GetString(0, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(1, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(2, static_cast<std::string*>(nullptr)));
  EXPECT_TRUE(main_list.GetString(3, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(4, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(5, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(6, static_cast<std::string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(7, static_cast<std::string*>(nullptr)));

  EXPECT_FALSE(main_list.GetString(0, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(1, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(2, static_cast<std::u16string*>(nullptr)));
  EXPECT_TRUE(main_list.GetString(3, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(4, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(5, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(6, static_cast<std::u16string*>(nullptr)));
  EXPECT_FALSE(main_list.GetString(7, static_cast<std::u16string*>(nullptr)));

  EXPECT_FALSE(main_list.GetDictionary(0, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(1, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(2, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(3, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(4, nullptr));
  EXPECT_TRUE(main_list.GetDictionary(5, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(6, nullptr));
  EXPECT_FALSE(main_list.GetDictionary(7, nullptr));
}

TEST(ValuesTest, SelfSwap) {
  base::Value test(1);
  std::swap(test, test);
  EXPECT_EQ(1, test.GetInt());
}

TEST(ValuesTest, FromToUniquePtrValue) {
  std::unique_ptr<DictionaryValue> dict = std::make_unique<DictionaryValue>();
  dict->SetStringKey("name", "Froogle");
  dict->SetStringKey("url", "http://froogle.com");
  Value dict_copy = dict->Clone();

  Value dict_converted = Value::FromUniquePtrValue(std::move(dict));
  EXPECT_EQ(dict_copy, dict_converted);

  std::unique_ptr<Value> val =
      Value::ToUniquePtrValue(std::move(dict_converted));
  EXPECT_EQ(dict_copy, *val);
}

TEST(ValuesTest, MutableFindStringPath) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetStringPath("foo.bar", "value");

  *(dict.FindStringPath("foo.bar")) = "new_value";

  Value expected_dict(Value::Type::DICTIONARY);
  expected_dict.SetStringPath("foo.bar", "new_value");

  EXPECT_EQ(expected_dict, dict);
}

TEST(ValuesTest, MutableGetString) {
  Value value("value");
  value.GetString() = "new_value";
  EXPECT_EQ("new_value", value.GetString());
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
TEST(ValuesTest, TracingSupport) {
  EXPECT_EQ(perfetto::TracedValueToString(Value(false)), "false");
  EXPECT_EQ(perfetto::TracedValueToString(Value(1)), "1");
  EXPECT_EQ(perfetto::TracedValueToString(Value(1.5)), "1.5");
  EXPECT_EQ(perfetto::TracedValueToString(Value("value")), "value");
  EXPECT_EQ(perfetto::TracedValueToString(Value(Value::Type::NONE)), "<none>");
  {
    Value::ListStorage list;
    list.emplace_back(2);
    list.emplace_back(3);
    EXPECT_EQ(perfetto::TracedValueToString(Value(list)), "[2,3]");
  }
  {
    Value::DictStorage dict;
    dict["key"] = Value("value");
    EXPECT_EQ(perfetto::TracedValueToString(Value(dict)), "{key:value}");
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

}  // namespace base
