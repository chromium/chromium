// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/values.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include <optional>

#include "third_party/perfetto/include/perfetto/test/traced_value_test_support.h"  // no-presubmit-check nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

#ifdef NDEBUG
// `Value` should have a (relatively) small size to avoid creating excess
// overhead, e.g. for lists of values that are all ints.
//
// This test is limited to NDEBUG builds, since some containers may require
// extra storage for supporting debug checks for things like iterators.
TEST(ValuesTest, SizeOfValue) {
#if defined(__GLIBCXX__)
  // libstdc++ std::string takes already 4 machine words, so the absl::variant
  // takes 5
  constexpr size_t kExpectedSize = 5 * sizeof(void*);
#else   // !defined(__GLIBCXX__)
  // libc++'s std::string and std::vector both take 3 machine words. An
  // additional word is used by absl::variant for the type index.
  constexpr size_t kExpectedSize = 4 * sizeof(void*);
#endif  // defined(__GLIBCXX__)

  // Use std::integral_constant so the compiler error message includes the
  // evaluated size. In future versions of clang, it should be possible to
  // simplify this to an equality comparison (i.e. newer clangs print out
  // "comparison reduces to '(1 == 2)'").
  static_assert(std::is_same_v<std::integral_constant<size_t, sizeof(Value)>,
                               std::integral_constant<size_t, kExpectedSize>>,
                "base::Value has an unexpected size!");
}
#endif

TEST(ValuesTest, TestNothrow) {
  static_assert(std::is_nothrow_move_constructible_v<Value>,
                "IsNothrowMoveConstructible");
  static_assert(std::is_nothrow_default_constructible_v<Value>,
                "IsNothrowDefaultConstructible");
  static_assert(std::is_nothrow_constructible_v<Value, std::string&&>,
                "IsNothrowMoveConstructibleFromString");
  static_assert(std::is_nothrow_constructible_v<Value, Value::BlobStorage&&>,
                "IsNothrowMoveConstructibleFromBlob");
  static_assert(std::is_nothrow_move_assignable_v<Value>,
                "IsNothrowMoveAssignable");
}

TEST(ValuesTest, EmptyValue) {
  Value value;
  EXPECT_EQ(Value::Type::NONE, value.type());
  EXPECT_EQ(std::nullopt, value.GetIfBool());
  EXPECT_EQ(std::nullopt, value.GetIfInt());
  EXPECT_EQ(std::nullopt, value.GetIfDouble());
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
  static_assert(!std::is_constructible_v<Value, int*>, "");
  static_assert(!std::is_constructible_v<Value, const int*>, "");
  static_assert(!std::is_constructible_v<Value, wchar_t*>, "");
  static_assert(!std::is_constructible_v<Value, const wchar_t*>, "");

  static_assert(std::is_constructible_v<Value, char*>, "");
  static_assert(std::is_constructible_v<Value, const char*>, "");
  static_assert(std::is_constructible_v<Value, char16_t*>, "");
  static_assert(std::is_constructible_v<Value, const char16_t*>, "");
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
  Value value{std::string_view(str)};
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
  Value value{std::u16string_view(str)};
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
  Value::Dict value;
  EXPECT_EQ(Value::Type::DICT, Value(std::move(value)).type());
}

TEST(ValuesTest, ConstructDictFromValueDict) {
  Value::Dict dict;
  dict.Set("foo", "bar");
  {
    Value value(dict.Clone());
    EXPECT_EQ(Value::Type::DICT, value.type());
    EXPECT_TRUE(value.GetIfDict());
    EXPECT_TRUE(value.GetDict().FindString("foo"));
    EXPECT_EQ("bar", *value.GetDict().FindString("foo"));
  }

  dict.Set("foo", "baz");
  {
    Value value(std::move(dict));
    EXPECT_EQ(Value::Type::DICT, value.type());
    EXPECT_TRUE(value.GetIfDict());
    EXPECT_TRUE(value.GetDict().FindString("foo"));
    EXPECT_EQ("baz", *value.GetDict().FindString("foo"));
  }
}

TEST(ValuesTest, ConstructList) {
  Value value(Value::List{});
  EXPECT_EQ(Value::Type::LIST, value.type());
}

TEST(ValuesTest, UseTestingEachOnValueList) {
  Value::List list;
  list.Append(true);
  list.Append(true);

  // This will only work if `Value::List::value_type` is defined.
  EXPECT_THAT(list, testing::Each(testing::ResultOf(
                        [](const Value& value) { return value.GetBool(); },
                        testing::Eq(true))));
}

TEST(ValuesTest, ConstructListFromValueList) {
  Value::List list;
  list.Append("foo");
  {
    Value value(list.Clone());
    EXPECT_EQ(Value::Type::LIST, value.type());
    EXPECT_EQ(1u, value.GetList().size());
    EXPECT_EQ(Value::Type::STRING, value.GetList()[0].type());
    EXPECT_EQ("foo", value.GetList()[0].GetString());
  }

  list.back() = base::Value("bar");
  {
    Value value(std::move(list));
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
  Value::Dict dict;
  dict.Set("Int", 123);
  Value value(std::move(dict));

  Value copied_value(value.Clone());
  EXPECT_EQ(value, copied_value);

  Value blank;
  blank = value.Clone();
  EXPECT_EQ(value, blank);
}

TEST(ValuesTest, CopyList) {
  Value::List list;
  list.Append(123);
  Value value(std::move(list));

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
  Value::Dict dict;
  dict.Set("Int", 123);

  Value value(std::move(dict));
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::DICT, moved_value.type());
  EXPECT_EQ(123, moved_value.GetDict().Find("Int")->GetInt());
}

TEST(ValuesTest, MoveAssignDictionary) {
  Value::Dict dict;
  dict.Set("Int", 123);

  Value blank;
  blank = Value(std::move(dict));
  EXPECT_EQ(Value::Type::DICT, blank.type());
  EXPECT_EQ(123, blank.GetDict().Find("Int")->GetInt());
}

TEST(ValuesTest, ConstructDictWithIterators) {
  std::vector<std::pair<std::string, Value>> values;
  values.emplace_back(std::make_pair("Int", 123));

  Value blank;
  blank = Value(Value::Dict(std::make_move_iterator(values.begin()),
                            std::make_move_iterator(values.end())));
  EXPECT_EQ(Value::Type::DICT, blank.type());
  EXPECT_EQ(123, blank.GetDict().Find("Int")->GetInt());
}

TEST(ValuesTest, MoveList) {
  Value::List list;
  list.Append(123);
  Value value(list.Clone());
  Value moved_value(std::move(value));
  EXPECT_EQ(Value::Type::LIST, moved_value.type());
  EXPECT_EQ(123, moved_value.GetList().back().GetInt());

  Value blank;
  blank = Value(std::move(list));
  EXPECT_EQ(Value::Type::LIST, blank.type());
  EXPECT_EQ(123, blank.GetList().back().GetInt());
}

TEST(ValuesTest, Append) {
  Value::List list;
  list.Append(true);
  EXPECT_TRUE(list.back().is_bool());

  list.Append(123);
  EXPECT_TRUE(list.back().is_int());

  list.Append(3.14);
  EXPECT_TRUE(list.back().is_double());

  std::string str = "foo";
  list.Append(str.c_str());
  EXPECT_TRUE(list.back().is_string());

  list.Append(std::string_view(str));
  EXPECT_TRUE(list.back().is_string());

  list.Append(std::move(str));
  EXPECT_TRUE(list.back().is_string());

  std::u16string str16 = u"bar";
  list.Append(str16.c_str());
  EXPECT_TRUE(list.back().is_string());

  list.Append(std::u16string_view(str16));
  EXPECT_TRUE(list.back().is_string());

  list.Append(Value());
  EXPECT_TRUE(list.back().is_none());

  list.Append(Value::Dict());
  EXPECT_TRUE(list.back().is_dict());

  list.Append(Value::List());
  EXPECT_TRUE(list.back().is_list());
}

TEST(ValuesTest, ListInsert) {
  Value::List list;
  const Value::List& const_list = list;

  auto iter = list.Insert(list.end(), Value(true));
  EXPECT_TRUE(list.begin() == iter);
  EXPECT_EQ(*iter, true);

  iter = list.Insert(const_list.begin(), Value(123));
  EXPECT_TRUE(const_list.begin() == iter);
  EXPECT_EQ(*iter, 123);

  iter = list.Insert(list.begin() + 1, Value("Hello world!"));
  EXPECT_TRUE(list.begin() + 1 == iter);
  EXPECT_EQ(*iter, "Hello world!");
}

TEST(ValuesTest, ListResize) {
  auto list = base::Value::List().Append("Hello world!");
  EXPECT_EQ(list.size(), 1U);

  list.resize(2);
  // Adds an empty entry to the back to match the size.
  EXPECT_EQ(list.size(), 2U);
  EXPECT_TRUE(list[0].is_string());
  EXPECT_TRUE(list[1].is_none());

  list.resize(1);
  // Shrinks the list and kicks the new entry out.
  EXPECT_EQ(list.size(), 1U);
  EXPECT_TRUE(list[0].is_string());

  list.resize(0);
  // Removes the remaining entry too.
  EXPECT_EQ(list.size(), 0U);
}

TEST(ValuesTest, ReverseIter) {
  Value::List list;
  const Value::List& const_list = list;

  list.Append(Value(true));
  list.Append(Value(123));
  list.Append(Value("Hello world!"));

  auto iter = list.rbegin();
  EXPECT_TRUE(const_list.rbegin() == iter);
  EXPECT_EQ(*iter, "Hello world!");

  ++iter;
  EXPECT_EQ(*iter, 123);

  ++iter;
  EXPECT_EQ(*iter, true);

  ++iter;
  EXPECT_TRUE(list.rend() == iter);
  EXPECT_TRUE(const_list.rend() == iter);
}

// Test all three behaviors of EnsureDict() (Create a new dict where no
// matchining values exist, return an existing dict, create a dict overwriting
// a value of another type).
TEST(ValuesTest, DictEnsureDict) {
  Value::Dict root;

  // This call should create a new nested dictionary.
  Value::Dict* foo_dict = root.EnsureDict("foo");
  EXPECT_TRUE(foo_dict->empty());
  foo_dict->Set("a", "b");

  // This call should retrieve the dictionary created above, rather than
  // creating a new one.
  std::string* a_string = root.EnsureDict("foo")->FindString("a");
  ASSERT_NE(nullptr, a_string);
  EXPECT_EQ(*a_string, "b");

  // Use EnsureDict() to overwrite an existing non-dictionary value.
  root.Set("bar", 3);
  Value::Dict* bar_dict = root.EnsureDict("bar");
  EXPECT_TRUE(bar_dict->empty());
  bar_dict->Set("b", "c");

  // Test that the above call created a "bar" entry.
  bar_dict = root.FindDict("bar");
  ASSERT_NE(nullptr, bar_dict);
  std::string* b_string = bar_dict->FindString("b");
  ASSERT_NE(nullptr, b_string);
  EXPECT_EQ(*b_string, "c");
}

// Test all three behaviors of EnsureList() (Create a new list where no
// matchining value exists, return an existing list, create a list overwriting
// a value of another type).
TEST(ValuesTest, DictEnsureList) {
  Value::Dict root;

  // This call should create a new list.
  Value::List* foo_list = root.EnsureList("foo");
  EXPECT_TRUE(foo_list->empty());
  foo_list->Append("a");

  // This call should retrieve the list created above, rather than creating a
  // new one.
  foo_list = root.EnsureList("foo");
  ASSERT_EQ(1u, foo_list->size());
  EXPECT_EQ((*foo_list)[0], Value("a"));

  // Use EnsureList() to overwrite an existing non-list value.
  root.Set("bar", 3);
  Value::List* bar_list = root.EnsureList("bar");
  EXPECT_TRUE(bar_list->empty());
  bar_list->Append("b");

  // Test that the above call created a "bar" entry.
  bar_list = root.FindList("bar");
  ASSERT_NE(nullptr, bar_list);
  ASSERT_EQ(1u, bar_list->size());
  EXPECT_EQ((*bar_list)[0], Value("b"));
}

// TODO(dcheng): Add more tests directly exercising the updated dictionary and
// list APIs. For now, most of the updated APIs are tested indirectly via the
// legacy APIs that are largely backed by the updated APIs.
TEST(ValuesTest, DictFindByDottedPath) {
  Value::Dict dict;

  EXPECT_EQ(nullptr, dict.FindByDottedPath("a.b.c"));

  Value::Dict& a_dict = dict.Set("a", Value::Dict())->GetDict();
  EXPECT_EQ(nullptr, dict.FindByDottedPath("a.b.c"));

  Value::Dict& b_dict = a_dict.Set("b", Value::Dict())->GetDict();
  EXPECT_EQ(nullptr, dict.FindByDottedPath("a.b.c"));

  b_dict.Set("c", true);
  const Value* value = dict.FindByDottedPath("a.b.c");
  ASSERT_NE(nullptr, value);
  EXPECT_TRUE(value->GetBool());
}

TEST(ValuesTest, DictSetByDottedPath) {
  Value::Dict dict;

  Value* c = dict.SetByDottedPath("a.b.c", Value());
  ASSERT_TRUE(c);

  Value::Dict* a = dict.FindDict("a");
  ASSERT_TRUE(a);
  EXPECT_EQ(1U, a->size());

  Value::Dict* b = a->FindDict("b");
  ASSERT_TRUE(b);
  EXPECT_EQ(1U, b->size());

  EXPECT_EQ(c, b->Find("c"));
}

TEST(ValuesTest, RvalueDictSetByDottedPath) {
  Value::Dict dict =
      Value::Dict()
          .SetByDottedPath("nested.dictionary.null", Value())
          .SetByDottedPath("nested.dictionary.bool", false)
          .SetByDottedPath("nested.dictionary.int", 42)
          .SetByDottedPath("nested.dictionary.double", 1.2)
          .SetByDottedPath("nested.dictionary.string", "value")
          .SetByDottedPath("nested.dictionary.u16-string", u"u16-value")
          .SetByDottedPath("nested.dictionary.std-string",
                           std::string("std-value"))
          .SetByDottedPath("nested.dictionary.blob", Value::BlobStorage({1, 2}))
          .SetByDottedPath("nested.dictionary.list",
                           Value::List().Append("value in list"))
          .SetByDottedPath("nested.dictionary.dict",
                           Value::Dict().Set("key", "value"));

  Value::Dict expected =
      Value::Dict()  //
          .Set("nested",
               base::Value::Dict()  //
                   .Set("dictionary",
                        base::Value::Dict()
                            .Set("null", Value())
                            .Set("bool", false)
                            .Set("int", 42)
                            .Set("double", 1.2)
                            .Set("string", "value")
                            .Set("u16-string", u"u16-value")
                            .Set("std-string", std::string("std-value"))
                            .Set("blob", Value::BlobStorage({1, 2}))
                            .Set("list", Value::List().Append("value in list"))
                            .Set("dict", Value::Dict().Set("key", "value"))));

  EXPECT_EQ(dict, expected);
}

TEST(ValuesTest, DictSetWithDottedKey) {
  Value::Dict dict;

  Value* abc = dict.Set("a.b.c", Value());
  ASSERT_TRUE(abc);

  EXPECT_FALSE(dict.FindByDottedPath("a"));
  EXPECT_FALSE(dict.FindByDottedPath("a.b"));
  EXPECT_FALSE(dict.FindByDottedPath("a.b.c"));

  EXPECT_EQ(abc, dict.Find("a.b.c"));
}

TEST(ValuesTest, ListFront) {
  Value::List list;
  const Value::List& const_list = list;

  list.Append(1);
  list.Append(2);
  list.Append(3);

  EXPECT_EQ(Value(1), list.front());
  EXPECT_EQ(Value(1), const_list.front());
}

TEST(ValuesTest, ListFrontWhenEmpty) {
  Value::List list;
  const Value::List& const_list = list;

  EXPECT_CHECK_DEATH(list.front());
  EXPECT_CHECK_DEATH(const_list.front());
}

TEST(ValuesTest, ListBack) {
  Value::List list;
  const Value::List& const_list = list;

  list.Append(1);
  list.Append(2);
  list.Append(3);

  EXPECT_EQ(Value(3), list.back());
  EXPECT_EQ(Value(3), const_list.back());
}

TEST(ValuesTest, ListBackWhenEmpty) {
  Value::List list;
  const Value::List& const_list = list;

  EXPECT_CHECK_DEATH(list.back());
  EXPECT_CHECK_DEATH(const_list.back());
}

TEST(ValuesTest, ListErase) {
  Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(3);

  auto next_it = list.erase(list.begin() + 1);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(list[0], Value(1));
  EXPECT_EQ(list[1], Value(3));
  EXPECT_EQ(*next_it, Value(3));
  EXPECT_EQ(next_it + 1, list.end());
}

TEST(ValuesTest, ListEraseRange) {
  Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(3);
  list.Append(4);

  auto next_it = list.erase(list.begin() + 1, list.begin() + 3);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(list[0], Value(1));
  EXPECT_EQ(list[1], Value(4));
  EXPECT_EQ(*next_it, Value(4));
  EXPECT_EQ(next_it + 1, list.end());

  next_it = list.erase(list.begin() + 1, list.begin() + 1);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(list[0], Value(1));
  EXPECT_EQ(list[1], Value(4));
  EXPECT_EQ(*next_it, Value(4));
  EXPECT_EQ(next_it + 1, list.end());

  next_it = list.erase(list.begin() + 1, list.end());
  ASSERT_EQ(1u, list.size());
  EXPECT_EQ(list[0], Value(1));
  EXPECT_EQ(next_it, list.end());

  list.clear();
  next_it = list.erase(list.begin(), list.begin());
  ASSERT_EQ(0u, list.size());
  EXPECT_EQ(next_it, list.begin());
  EXPECT_EQ(next_it, list.end());
}

TEST(ValuesTest, ListEraseValue) {
  Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(2);
  list.Append(3);

  EXPECT_EQ(2u, list.EraseValue(Value(2)));
  EXPECT_EQ(2u, list.size());
  EXPECT_EQ(1, list[0]);
  EXPECT_EQ(3, list[1]);

  EXPECT_EQ(1u, list.EraseValue(Value(1)));
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(3, list[0]);

  EXPECT_EQ(1u, list.EraseValue(Value(3)));
  EXPECT_TRUE(list.empty());

  EXPECT_EQ(0u, list.EraseValue(Value(3)));
}

TEST(ValuesTest, ListEraseIf) {
  Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(2);
  list.Append(3);

  EXPECT_EQ(3u, list.EraseIf([](const auto& val) { return val >= Value(2); }));
  EXPECT_EQ(1u, list.size());
  EXPECT_EQ(1, list[0]);

  EXPECT_EQ(1u, list.EraseIf([](const auto& val) { return true; }));
  EXPECT_TRUE(list.empty());

  EXPECT_EQ(0u, list.EraseIf([](const auto& val) { return true; }));
}

TEST(ValuesTest, ClearList) {
  Value::List list;
  list.Append(1);
  list.Append(2);
  list.Append(3);
  EXPECT_EQ(3u, list.size());
  EXPECT_FALSE(list.empty());

  list.clear();
  EXPECT_EQ(0u, list.size());
  EXPECT_TRUE(list.empty());

  // list.clear() should be idempotent.
  list.clear();
  EXPECT_EQ(0u, list.size());
  EXPECT_TRUE(list.empty());
}

TEST(ValuesTest, FindKey) {
  Value::Dict dict;
  dict.Set("foo", "bar");
  Value value(std::move(dict));
  EXPECT_NE(nullptr, value.GetDict().Find("foo"));
  EXPECT_EQ(nullptr, value.GetDict().Find("baz"));
}

TEST(ValuesTest, FindKeyChangeValue) {
  Value::Dict dict;
  dict.Set("foo", "bar");
  Value* found = dict.Find("foo");
  ASSERT_NE(nullptr, found);
  EXPECT_EQ("bar", found->GetString());

  *found = Value(123);
  EXPECT_EQ(123, dict.Find("foo")->GetInt());
}

TEST(ValuesTest, FindKeyConst) {
  Value::Dict dict;
  dict.Set("foo", "bar");
  const Value value(std::move(dict));
  EXPECT_NE(nullptr, value.GetDict().Find("foo"));
  EXPECT_EQ(nullptr, value.GetDict().Find("baz"));
}

TEST(ValuesTest, FindBoolKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(std::nullopt, dict.FindBool("null"));
  EXPECT_NE(std::nullopt, dict.FindBool("bool"));
  EXPECT_EQ(std::nullopt, dict.FindBool("int"));
  EXPECT_EQ(std::nullopt, dict.FindBool("double"));
  EXPECT_EQ(std::nullopt, dict.FindBool("string"));
  EXPECT_EQ(std::nullopt, dict.FindBool("blob"));
  EXPECT_EQ(std::nullopt, dict.FindBool("list"));
  EXPECT_EQ(std::nullopt, dict.FindBool("dict"));
}

TEST(ValuesTest, FindIntKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(std::nullopt, dict.FindInt("null"));
  EXPECT_EQ(std::nullopt, dict.FindInt("bool"));
  EXPECT_NE(std::nullopt, dict.FindInt("int"));
  EXPECT_EQ(std::nullopt, dict.FindInt("double"));
  EXPECT_EQ(std::nullopt, dict.FindInt("string"));
  EXPECT_EQ(std::nullopt, dict.FindInt("blob"));
  EXPECT_EQ(std::nullopt, dict.FindInt("list"));
  EXPECT_EQ(std::nullopt, dict.FindInt("dict"));
}

TEST(ValuesTest, FindStringKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(nullptr, dict.FindString("null"));
  EXPECT_EQ(nullptr, dict.FindString("bool"));
  EXPECT_EQ(nullptr, dict.FindString("int"));
  EXPECT_EQ(nullptr, dict.FindString("double"));
  EXPECT_NE(nullptr, dict.FindString("string"));
  EXPECT_EQ(nullptr, dict.FindString("blob"));
  EXPECT_EQ(nullptr, dict.FindString("list"));
  EXPECT_EQ(nullptr, dict.FindString("dict"));
}

TEST(ValuesTest, MutableFindStringKey) {
  Value::Dict dict;
  dict.Set("string", "foo");

  *(dict.FindString("string")) = "bar";

  Value::Dict expected_dict;
  expected_dict.Set("string", "bar");

  EXPECT_EQ(expected_dict, dict);

  Value value(std::move(dict));
  Value expected_value(std::move(expected_dict));
  EXPECT_EQ(expected_value, value);
}

TEST(ValuesTest, MutableFindBlobKey) {
  Value::BlobStorage original_blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value::Dict dict;
  dict.Set("blob", std::move(original_blob));

  Value::BlobStorage new_blob = {0x0, 0x3, 0x0};
  *(dict.FindBlob("blob")) = new_blob;

  Value::Dict expected_dict;
  expected_dict.Set("blob", std::move(new_blob));

  EXPECT_EQ(expected_dict, dict);

  Value value(std::move(dict));
  Value expected_value(std::move(expected_dict));
  EXPECT_EQ(expected_value, value);
}

TEST(ValuesTest, FindDictKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(nullptr, dict.FindDict("null"));
  EXPECT_EQ(nullptr, dict.FindDict("bool"));
  EXPECT_EQ(nullptr, dict.FindDict("int"));
  EXPECT_EQ(nullptr, dict.FindDict("double"));
  EXPECT_EQ(nullptr, dict.FindDict("string"));
  EXPECT_EQ(nullptr, dict.FindDict("blob"));
  EXPECT_EQ(nullptr, dict.FindDict("list"));
  EXPECT_NE(nullptr, dict.FindDict("dict"));
}

TEST(ValuesTest, FindListKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(nullptr, dict.FindList("null"));
  EXPECT_EQ(nullptr, dict.FindList("bool"));
  EXPECT_EQ(nullptr, dict.FindList("int"));
  EXPECT_EQ(nullptr, dict.FindList("double"));
  EXPECT_EQ(nullptr, dict.FindList("string"));
  EXPECT_EQ(nullptr, dict.FindList("blob"));
  EXPECT_NE(nullptr, dict.FindList("list"));
  EXPECT_EQ(nullptr, dict.FindList("dict"));
}

TEST(ValuesTest, FindBlob) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  EXPECT_EQ(nullptr, dict.FindBlob("null"));
  EXPECT_EQ(nullptr, dict.FindBlob("bool"));
  EXPECT_EQ(nullptr, dict.FindBlob("int"));
  EXPECT_EQ(nullptr, dict.FindBlob("double"));
  EXPECT_EQ(nullptr, dict.FindBlob("string"));
  EXPECT_NE(nullptr, dict.FindBlob("blob"));
  EXPECT_EQ(nullptr, dict.FindBlob("list"));
  EXPECT_EQ(nullptr, dict.FindBlob("dict"));
}

TEST(ValuesTest, SetKey) {
  Value::Dict dict;
  dict.Set("null", Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", std::string());
  dict.Set("blob", Value(Value::BlobStorage()));
  dict.Set("list", Value::List());
  dict.Set("dict", Value::Dict());

  Value::Dict dict2;
  dict2.Set(std::string_view("null"), Value(Value::Type::NONE));
  dict2.Set(std::string_view("bool"), Value(Value::Type::BOOLEAN));
  dict2.Set(std::string("int"), Value(Value::Type::INTEGER));
  dict2.Set(std::string("double"), Value(Value::Type::DOUBLE));
  dict2.Set(std::string("string"), Value(Value::Type::STRING));
  dict2.Set("blob", Value(Value::Type::BINARY));
  dict2.Set("list", Value(Value::Type::LIST));
  dict2.Set("dict", Value(Value::Type::DICT));

  EXPECT_EQ(dict, dict2);
  EXPECT_EQ(Value(std::move(dict)), Value(std::move(dict2)));
}

TEST(ValuesTest, SetBoolKey) {
  std::optional<bool> value;

  Value::Dict dict;
  dict.Set("true_key", true);
  dict.Set("false_key", false);

  value = dict.FindBool("true_key");
  ASSERT_TRUE(value);
  ASSERT_TRUE(*value);

  value = dict.FindBool("false_key");
  ASSERT_TRUE(value);
  ASSERT_FALSE(*value);

  value = dict.FindBool("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, SetIntKey) {
  std::optional<int> value;

  Value::Dict dict;
  dict.Set("one_key", 1);
  dict.Set("minus_one_key", -1);

  value = dict.FindInt("one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ(1, *value);

  value = dict.FindInt("minus_one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ(-1, *value);

  value = dict.FindInt("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, SetDoubleKey) {
  Value::Dict dict;
  dict.Set("one_key", 1.0);
  dict.Set("minus_one_key", -1.0);
  dict.Set("pi_key", 3.1415);

  const Value* value;

  value = dict.Find("one_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(1.0, value->GetDouble());

  value = dict.Find("minus_one_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(-1.0, value->GetDouble());

  value = dict.Find("pi_key");
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_double());
  EXPECT_EQ(3.1415, value->GetDouble());
}

TEST(ValuesTest, SetStringKey) {
  Value::Dict dict;
  dict.Set("one_key", "one");
  dict.Set("hello_key", "hello world");

  std::string movable_value("movable_value");
  dict.Set("movable_key", std::move(movable_value));
  ASSERT_TRUE(movable_value.empty());

  const std::string* value;

  value = dict.FindString("one_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("one", *value);

  value = dict.FindString("hello_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("hello world", *value);

  value = dict.FindString("movable_key");
  ASSERT_TRUE(value);
  ASSERT_EQ("movable_value", *value);

  value = dict.FindString("missing_key");
  ASSERT_FALSE(value);
}

TEST(ValuesTest, RvalueSet) {
  Value::Dict dict = Value::Dict()
                         .Set("null", Value())
                         .Set("bool", false)
                         .Set("int", 42)
                         .Set("double", 1.2)
                         .Set("string", "value")
                         .Set("u16-string", u"u16-value")
                         .Set("std-string", std::string("std-value"))
                         .Set("blob", Value::BlobStorage({1, 2}))
                         .Set("list", Value::List().Append("value in list"))
                         .Set("dict", Value::Dict().Set("key", "value"));

  Value::Dict expected;
  expected.Set("null", Value());
  expected.Set("bool", false);
  expected.Set("int", 42);
  expected.Set("double", 1.2);
  expected.Set("string", "value");
  expected.Set("u16-string", u"u16-value");
  expected.Set("std-string", std::string("std-value"));
  expected.Set("blob", Value::BlobStorage({1, 2}));
  Value::List nested_list;
  nested_list.Append("value in list");
  expected.Set("list", std::move(nested_list));
  Value::Dict nested_dict;
  nested_dict.Set("key", "value");
  expected.Set("dict", std::move(nested_dict));

  EXPECT_EQ(dict, expected);
}

TEST(ValuesTest, FindPath) {
  // Construct a dictionary path {root}.foo.bar = 123
  Value::Dict foo;
  foo.Set("bar", Value(123));

  Value::Dict root;
  root.Set("foo", std::move(foo));

  // Double key, second not found.
  Value* found = root.FindByDottedPath("foo.notfound");
  EXPECT_FALSE(found);

  // Double key, found.
  found = root.FindByDottedPath("foo.bar");
  EXPECT_TRUE(found);
  EXPECT_TRUE(found->is_int());
  EXPECT_EQ(123, found->GetInt());
}

TEST(ValuesTest, SetByDottedPath) {
  Value::Dict root;

  Value* inserted = root.SetByDottedPath("one.two", Value(123));
  Value* found = root.FindByDottedPath("one.two");
  ASSERT_TRUE(found);
  EXPECT_EQ(found->type(), Value::Type::INTEGER);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  inserted = root.SetByDottedPath("foo.bar", Value(123));
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(found->type(), Value::Type::INTEGER);
  EXPECT_EQ(inserted, found);
  EXPECT_EQ(123, found->GetInt());

  // Overwrite with a different value.
  root.SetByDottedPath("foo.bar", Value("hello"));
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(found->type(), Value::Type::STRING);
  EXPECT_EQ("hello", found->GetString());

  // Can't change existing non-dictionary keys to dictionaries.
  found = root.SetByDottedPath("foo.bar.baz", Value(123));
  EXPECT_FALSE(found);
}

TEST(ValuesTest, SetBoolPath) {
  Value::Dict root;
  Value* inserted = root.SetByDottedPath("foo.bar", true);
  Value* found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_bool());
  EXPECT_TRUE(found->GetBool());

  // Overwrite with a different value.
  root.SetByDottedPath("foo.bar", false);
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_bool());
  EXPECT_FALSE(found->GetBool());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetByDottedPath("foo.bar.zoo", true));
}

TEST(ValuesTest, SetIntPath) {
  Value::Dict root;
  Value* inserted = root.SetByDottedPath("foo.bar", 123);
  Value* found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_int());
  EXPECT_EQ(123, found->GetInt());

  // Overwrite with a different value.
  root.SetByDottedPath("foo.bar", 234);
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_int());
  EXPECT_EQ(234, found->GetInt());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetByDottedPath("foo.bar.zoo", 567));
}

TEST(ValuesTest, SetDoublePath) {
  Value::Dict root;
  Value* inserted = root.SetByDottedPath("foo.bar", 1.23);
  Value* found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_double());
  EXPECT_EQ(1.23, found->GetDouble());

  // Overwrite with a different value.
  root.SetByDottedPath("foo.bar", 2.34);
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_double());
  EXPECT_EQ(2.34, found->GetDouble());

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetByDottedPath("foo.bar.zoo", 5.67));
}

TEST(ValuesTest, SetStringPath) {
  Value::Dict root;
  Value* inserted = root.SetByDottedPath("foo.bar", "hello world");
  Value* found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  EXPECT_EQ(inserted, found);
  ASSERT_TRUE(found->is_string());
  EXPECT_EQ("hello world", found->GetString());

  // Overwrite with a different value.
  root.SetByDottedPath("foo.bar", "bonjour monde");
  found = root.FindByDottedPath("foo.bar");
  ASSERT_TRUE(found);
  ASSERT_TRUE(found->is_string());
  EXPECT_EQ("bonjour monde", found->GetString());

  ASSERT_TRUE(root.SetByDottedPath("foo.bar", std::string_view("rah rah")));
  ASSERT_TRUE(root.SetByDottedPath("foo.bar", std::string("temp string")));
  ASSERT_TRUE(root.SetByDottedPath("foo.bar", u"temp string"));

  // Can't change existing non-dictionary keys.
  ASSERT_FALSE(root.SetByDottedPath("foo.bar.zoo", "ola mundo"));
}

TEST(ValuesTest, Remove) {
  Value::Dict root;
  root.Set("one", Value(123));

  // Removal of missing key should fail.
  EXPECT_FALSE(root.Remove("two"));

  // Removal of existing key should succeed.
  EXPECT_TRUE(root.Remove("one"));

  // Second removal of previously existing key should fail.
  EXPECT_FALSE(root.Remove("one"));
}

TEST(ValuesTest, Extract) {
  Value::Dict root;
  root.Set("one", Value(123));

  // Extraction of missing key should fail.
  EXPECT_EQ(std::nullopt, root.Extract("two"));

  // Extraction of existing key should succeed.
  EXPECT_EQ(Value(123), root.Extract("one"));

  // Second extraction of previously existing key should fail.
  EXPECT_EQ(std::nullopt, root.Extract("one"));
}

TEST(ValuesTest, RemoveByDottedPath) {
  Value::Dict root;
  root.SetByDottedPath("one.two.three", Value(123));

  // Removal of missing key should fail.
  EXPECT_FALSE(root.RemoveByDottedPath("one.two.four"));

  // Removal of existing key should succeed.
  EXPECT_TRUE(root.RemoveByDottedPath("one.two.three"));

  // Second removal of previously existing key should fail.
  EXPECT_FALSE(root.RemoveByDottedPath("one.two.three"));

  // Intermediate empty dictionaries should be cleared.
  EXPECT_EQ(nullptr, root.Find("one"));

  root.SetByDottedPath("one.two.three", Value(123));
  root.SetByDottedPath("one.two.four", Value(124));

  EXPECT_TRUE(root.RemoveByDottedPath("one.two.three"));
  // Intermediate non-empty dictionaries should be kept.
  EXPECT_NE(nullptr, root.Find("one"));
  EXPECT_NE(nullptr, root.FindByDottedPath("one.two"));
  EXPECT_NE(nullptr, root.FindByDottedPath("one.two.four"));
}

TEST(ValuesTest, ExtractByDottedPath) {
  Value::Dict root;
  root.SetByDottedPath("one.two.three", Value(123));

  // Extraction of missing key should fail.
  EXPECT_EQ(std::nullopt, root.ExtractByDottedPath("one.two.four"));

  // Extraction of existing key should succeed.
  EXPECT_EQ(Value(123), root.ExtractByDottedPath("one.two.three"));

  // Second extraction of previously existing key should fail.
  EXPECT_EQ(std::nullopt, root.ExtractByDottedPath("one.two.three"));

  // Intermediate empty dictionaries should be cleared.
  EXPECT_EQ(nullptr, root.Find("one"));

  root.SetByDottedPath("one.two.three", Value(123));
  root.SetByDottedPath("one.two.four", Value(124));

  EXPECT_EQ(Value(123), root.ExtractByDottedPath("one.two.three"));
  // Intermediate non-empty dictionaries should be kept.
  EXPECT_NE(nullptr, root.Find("one"));
  EXPECT_NE(nullptr, root.FindByDottedPath("one.two"));
  EXPECT_NE(nullptr, root.FindByDottedPath("one.two.four"));
}

TEST(ValuesTest, Basic) {
  // Test basic dictionary getting/setting
  Value::Dict settings;
  ASSERT_FALSE(settings.FindByDottedPath("global.homepage"));

  ASSERT_FALSE(settings.Find("global"));
  settings.Set("global", Value(true));
  ASSERT_TRUE(settings.Find("global"));
  settings.Remove("global");
  settings.SetByDottedPath("global.homepage", Value("http://scurvy.com"));
  ASSERT_TRUE(settings.Find("global"));
  const std::string* homepage =
      settings.FindStringByDottedPath("global.homepage");
  ASSERT_TRUE(homepage);
  ASSERT_EQ(std::string("http://scurvy.com"), *homepage);

  // Test storing a dictionary in a list.
  ASSERT_FALSE(settings.FindByDottedPath("global.toolbar.bookmarks"));

  Value::List new_toolbar_bookmarks;
  settings.SetByDottedPath("global.toolbar.bookmarks",
                           std::move(new_toolbar_bookmarks));
  Value::List* toolbar_bookmarks =
      settings.FindListByDottedPath("global.toolbar.bookmarks");
  ASSERT_TRUE(toolbar_bookmarks);

  Value::Dict new_bookmark;
  new_bookmark.Set("name", Value("Froogle"));
  new_bookmark.Set("url", Value("http://froogle.com"));
  toolbar_bookmarks->Append(std::move(new_bookmark));

  Value* bookmark_list = settings.FindByDottedPath("global.toolbar.bookmarks");
  ASSERT_TRUE(bookmark_list);
  ASSERT_EQ(1U, bookmark_list->GetList().size());
  Value* bookmark = &bookmark_list->GetList()[0];
  ASSERT_TRUE(bookmark);
  ASSERT_TRUE(bookmark->is_dict());
  const std::string* bookmark_name = bookmark->GetDict().FindString("name");
  ASSERT_TRUE(bookmark_name);
  ASSERT_EQ(std::string("Froogle"), *bookmark_name);
  const std::string* bookmark_url = bookmark->GetDict().FindString("url");
  ASSERT_TRUE(bookmark_url);
  ASSERT_EQ(std::string("http://froogle.com"), *bookmark_url);
}

TEST(ValuesTest, List) {
  Value::List mixed_list;
  mixed_list.Append(true);
  mixed_list.Append(42);
  mixed_list.Append(88.8);
  mixed_list.Append("foo");

  ASSERT_EQ(4u, mixed_list.size());

  EXPECT_EQ(true, mixed_list[0]);
  EXPECT_EQ(42, mixed_list[1]);
  EXPECT_EQ(88.8, mixed_list[2]);
  EXPECT_EQ("foo", mixed_list[3]);

  // Try searching in the mixed list.
  ASSERT_TRUE(Contains(mixed_list, 42));
  ASSERT_FALSE(Contains(mixed_list, false));
}

TEST(ValuesTest, RvalueAppend) {
  Value::List list = Value::List()
                         .Append(Value())
                         .Append(false)
                         .Append(42)
                         .Append(1.2)
                         .Append("value")
                         .Append(u"u16-value")
                         .Append(std::string("std-value"))
                         .Append(Value::BlobStorage({1, 2}))
                         .Append(Value::List().Append("value in list"))
                         .Append(Value::Dict().Set("key", "value"));

  Value::List expected;
  expected.Append(Value());
  expected.Append(false);
  expected.Append(42);
  expected.Append(1.2);
  expected.Append("value");
  expected.Append(u"u16-value");
  expected.Append(std::string("std-value"));
  expected.Append(Value::BlobStorage({1, 2}));
  Value::List nested_list;
  nested_list.Append("value in list");
  expected.Append(std::move(nested_list));
  Value::Dict nested_dict;
  nested_dict.Set("key", "value");
  expected.Append(std::move(nested_dict));

  EXPECT_EQ(list, expected);
}

TEST(ValuesTest, ListWithCapacity) {
  Value::List list_with_capacity =
      Value::List::with_capacity(3).Append(true).Append(42).Append(88.8);

  ASSERT_EQ(3u, list_with_capacity.size());
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

  ASSERT_TRUE(narrow_value->is_string());
  ASSERT_EQ(std::string("narrow"), narrow_value->GetString());

  ASSERT_TRUE(utf16_value->is_string());
  ASSERT_EQ(std::string("utf16"), utf16_value->GetString());
}

TEST(ValuesTest, DictionaryDeletion) {
  std::string key = "test";
  Value::Dict dict;
  dict.Set(key, Value());
  EXPECT_FALSE(dict.empty());
  EXPECT_EQ(1U, dict.size());
  dict.clear();
  EXPECT_TRUE(dict.empty());
  EXPECT_TRUE(dict.empty());
  EXPECT_EQ(0U, dict.size());
}

TEST(ValuesTest, DictionarySetReturnsPointer) {
  {
    Value::Dict dict;
    Value* blank_ptr = dict.Set("foo.bar", Value());
    EXPECT_EQ(Value::Type::NONE, blank_ptr->type());
  }

  {
    Value::Dict dict;
    Value* blank_ptr = dict.Set("foo.bar", Value());
    EXPECT_EQ(Value::Type::NONE, blank_ptr->type());
  }

  {
    Value::Dict dict;
    Value* int_ptr = dict.Set("foo.bar", 42);
    EXPECT_EQ(Value::Type::INTEGER, int_ptr->type());
    EXPECT_EQ(42, int_ptr->GetInt());
  }

  {
    Value::Dict dict;
    Value* string_ptr = dict.Set("foo.bar", "foo");
    EXPECT_EQ(Value::Type::STRING, string_ptr->type());
    EXPECT_EQ("foo", string_ptr->GetString());
  }

  {
    Value::Dict dict;
    Value* string16_ptr = dict.Set("foo.bar", u"baz");
    EXPECT_EQ(Value::Type::STRING, string16_ptr->type());
    EXPECT_EQ("baz", string16_ptr->GetString());
  }

  {
    Value::Dict dict;
    Value* dict_ptr = dict.Set("foo.bar", Value::Dict());
    EXPECT_EQ(Value::Type::DICT, dict_ptr->type());
  }

  {
    Value::Dict dict;
    Value* list_ptr = dict.Set("foo.bar", Value::List());
    EXPECT_EQ(Value::Type::LIST, list_ptr->type());
  }
}

TEST(ValuesTest, Clone) {
  Value original_null;
  Value original_bool(true);
  Value original_int(42);
  Value original_double(3.14);
  Value original_string("hello");
  Value original_string16(u"hello16");
  Value original_binary(Value::BlobStorage(42, '!'));

  Value::List list;
  list.Append(0);
  list.Append(1);
  Value original_list(std::move(list));

  Value original_dict(Value::Dict()
                          .Set("null", original_null.Clone())
                          .Set("bool", original_bool.Clone())
                          .Set("int", original_int.Clone())
                          .Set("double", original_double.Clone())
                          .Set("string", original_string.Clone())
                          .Set("string16", original_string16.Clone())
                          .Set("binary", original_binary.Clone())
                          .Set("list", original_list.Clone()));

  Value copy_value = original_dict.Clone();
  const Value::Dict& copy_dict = copy_value.GetDict();
  EXPECT_EQ(original_dict, copy_dict);
  EXPECT_EQ(original_null, *copy_dict.Find("null"));
  EXPECT_EQ(original_bool, *copy_dict.Find("bool"));
  EXPECT_EQ(original_int, *copy_dict.Find("int"));
  EXPECT_EQ(original_double, *copy_dict.Find("double"));
  EXPECT_EQ(original_string, *copy_dict.Find("string"));
  EXPECT_EQ(original_string16, *copy_dict.Find("string16"));
  EXPECT_EQ(original_binary, *copy_dict.Find("binary"));
  EXPECT_EQ(original_list, *copy_dict.Find("list"));
}

TEST(ValuesTest, TakeString) {
  Value value("foo");
  std::string taken = std::move(value).TakeString();
  EXPECT_EQ(taken, "foo");
}

// Check that the value can still be used after `TakeString()` was called, as
// long as a new value was assigned to it.
TEST(ValuesTest, PopulateAfterTakeString) {
  Value value("foo");
  std::string taken = std::move(value).TakeString();

  value = Value(false);
  EXPECT_EQ(value, Value(false));
}

TEST(ValuesTest, TakeBlob) {
  Value::BlobStorage original_blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value value(original_blob);
  Value::BlobStorage taken = std::move(value).TakeBlob();
  EXPECT_EQ(taken, original_blob);
}

TEST(ValuesTest, PopulateAfterTakeBlob) {
  Value::BlobStorage original_blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value value(original_blob);
  Value::BlobStorage taken = std::move(value).TakeBlob();

  value = Value(false);
  EXPECT_EQ(value, Value(false));
}

TEST(ValuesTest, TakeDict) {
  Value::Dict dict;
  dict.Set("foo", 123);
  Value value(std::move(dict));
  Value clone = value.Clone();

  Value::Dict taken = std::move(value).TakeDict();
  EXPECT_EQ(taken, clone);
}

// Check that the value can still be used after `TakeDict()` was called, as long
// as a new value was assigned to it.
TEST(ValuesTest, PopulateAfterTakeDict) {
  Value::Dict dict;
  dict.Set("foo", 123);
  Value value(std::move(dict));
  Value::Dict taken = std::move(value).TakeDict();

  value = Value(false);
  EXPECT_EQ(value, Value(false));
}

TEST(ValuesTest, TakeList) {
  Value::List list;
  list.Append(true);
  list.Append(123);
  Value value(std::move(list));
  Value clone = value.Clone();

  Value::List taken = std::move(value).TakeList();
  EXPECT_EQ(taken, clone);
}

// Check that the value can still be used after `TakeList()` was called, as long
// as a new value was assigned to it.
TEST(ValuesTest, PopulateAfterTakeList) {
  Value::List list;
  list.Append("hello");
  Value value(std::move(list));
  Value::List taken = std::move(value).TakeList();

  value = Value(false);
  EXPECT_EQ(value, Value(false));
}

TEST(ValuesTest, SpecializedEquals) {
  std::vector<Value> values;
  values.emplace_back(false);
  values.emplace_back(true);
  values.emplace_back(0);
  values.emplace_back(1);
  values.emplace_back(1.0);
  values.emplace_back(2.0);
  values.emplace_back("hello");
  values.emplace_back("world");
  base::Value::Dict dict;
  dict.Set("hello", "world");
  values.emplace_back(std::move(dict));
  base::Value::Dict dict2;
  dict2.Set("world", "hello");
  values.emplace_back(std::move(dict2));
  base::Value::List list;
  list.Append("hello");
  list.Append("world");
  values.emplace_back(std::move(list));
  base::Value::List list2;
  list2.Append("world");
  list2.Append("hello");
  values.emplace_back(std::move(list2));

  for (const Value& outer_value : values) {
    for (const Value& inner_value : values) {
      SCOPED_TRACE(::testing::Message()
                   << "Outer: " << outer_value << "Inner: " << inner_value);
      const bool should_be_equal = &outer_value == &inner_value;
      if (should_be_equal) {
        EXPECT_EQ(outer_value, inner_value);
        EXPECT_EQ(inner_value, outer_value);
        EXPECT_FALSE(outer_value != inner_value);
        EXPECT_FALSE(inner_value != outer_value);
      } else {
        EXPECT_NE(outer_value, inner_value);
        EXPECT_NE(inner_value, outer_value);
        EXPECT_FALSE(outer_value == inner_value);
        EXPECT_FALSE(inner_value == outer_value);
      }
      // Also test the various overloads for operator== against concrete
      // subtypes.
      outer_value.Visit([&](const auto& outer_member) {
        using T = std::decay_t<decltype(outer_member)>;
        if constexpr (!std::is_same_v<T, absl::monostate> &&
                      !std::is_same_v<T, Value::BlobStorage>) {
          if (should_be_equal) {
            EXPECT_EQ(outer_member, inner_value);
            EXPECT_EQ(inner_value, outer_member);
            EXPECT_FALSE(outer_member != inner_value);
            EXPECT_FALSE(inner_value != outer_member);
          } else {
            EXPECT_NE(outer_member, inner_value);
            EXPECT_NE(inner_value, outer_member);
            EXPECT_FALSE(outer_member == inner_value);
            EXPECT_FALSE(inner_value == outer_member);
          }
        }
      });
    }

    // A copy of a Value should also compare equal to itself.
    Value copied_value = outer_value.Clone();
    EXPECT_EQ(outer_value, copied_value);
    EXPECT_EQ(copied_value, outer_value);
    EXPECT_FALSE(outer_value != copied_value);
    EXPECT_FALSE(copied_value != outer_value);
  }
}

// Test that a literal string comparison does not end up using the bool (!!)
// overload.
TEST(ValuesTest, LiteralStringEquals) {
  EXPECT_EQ("hello world", base::Value("hello world"));
  EXPECT_EQ(base::Value("hello world"), "hello world");
  EXPECT_NE("hello world", base::Value(true));
  EXPECT_NE(base::Value(true), "hello world");
}

TEST(ValuesTest, Equals) {
  auto null1 = std::make_unique<Value>();
  auto null2 = std::make_unique<Value>();
  EXPECT_NE(null1.get(), null2.get());
  EXPECT_EQ(*null1, *null2);

  Value boolean(false);
  EXPECT_NE(*null1, boolean);

  Value::Dict dv;
  dv.Set("a", false);
  dv.Set("b", 2);
  dv.Set("c", 2.5);
  dv.Set("d1", "string");
  dv.Set("d2", u"http://google.com");
  dv.Set("e", Value());

  Value::Dict copy = dv.Clone();
  EXPECT_EQ(dv, copy);

  Value::List list;
  list.Append(Value());
  list.Append(Value(Value::Type::DICT));
  Value::List list_copy(list.Clone());

  Value* list_weak = dv.Set("f", std::move(list));
  EXPECT_NE(dv, copy);
  copy.Set("f", std::move(list_copy));
  EXPECT_EQ(dv, copy);

  list_weak->GetList().Append(true);
  EXPECT_NE(dv, copy);

  // Check if Equals detects differences in only the keys.
  copy = dv.Clone();
  EXPECT_EQ(dv, copy);
  copy.Remove("a");
  copy.Set("aa", false);
  EXPECT_NE(dv, copy);
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
  Value::List null_list1;
  Value::List null_list2;
  EXPECT_EQ(null_list1, null_list2);
  EXPECT_FALSE(null_list1 != null_list2);
  EXPECT_FALSE(null_list1 < null_list2);
  EXPECT_FALSE(null_list1 > null_list2);
  EXPECT_LE(null_list1, null_list2);
  EXPECT_GE(null_list1, null_list2);

  // Test Non Empty List Values.
  Value::List int_list1;
  Value::List int_list2;
  int_list1.Append(1);
  int_list2.Append(2);
  EXPECT_FALSE(int_list1 == int_list2);
  EXPECT_NE(int_list1, int_list2);
  EXPECT_LT(int_list1, int_list2);
  EXPECT_FALSE(int_list1 > int_list2);
  EXPECT_LE(int_list1, int_list2);
  EXPECT_FALSE(int_list1 >= int_list2);

  // Test Empty Dict Values.
  Value::Dict null_dict1;
  Value::Dict null_dict2;
  EXPECT_EQ(null_dict1, null_dict2);
  EXPECT_FALSE(null_dict1 != null_dict2);
  EXPECT_FALSE(null_dict1 < null_dict2);
  EXPECT_FALSE(null_dict1 > null_dict2);
  EXPECT_LE(null_dict1, null_dict2);
  EXPECT_GE(null_dict1, null_dict2);

  // Test Non Empty Dict Values.
  Value::Dict int_dict1;
  Value::Dict int_dict2;
  int_dict1.Set("key", 1);
  int_dict2.Set("key", 2);
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

TEST(ValuesTest, Merge) {
  Value::Dict base;
  base.Set("base_key", "base_key_value_base");
  base.Set("collide_key", "collide_key_value_base");
  Value::Dict base_sub_dict;
  base_sub_dict.Set("sub_base_key", "sub_base_key_value_base");
  base_sub_dict.Set("sub_collide_key", "sub_collide_key_value_base");
  base.Set("sub_dict_key", std::move(base_sub_dict));

  Value::Dict merge;
  merge.Set("merge_key", "merge_key_value_merge");
  merge.Set("collide_key", "collide_key_value_merge");
  Value::Dict merge_sub_dict;
  merge_sub_dict.Set("sub_merge_key", "sub_merge_key_value_merge");
  merge_sub_dict.Set("sub_collide_key", "sub_collide_key_value_merge");
  merge.Set("sub_dict_key", std::move(merge_sub_dict));

  base.Merge(std::move(merge));

  EXPECT_EQ(4U, base.size());
  const std::string* base_key_value = base.FindString("base_key");
  ASSERT_TRUE(base_key_value);
  EXPECT_EQ("base_key_value_base", *base_key_value);  // Base value preserved.
  const std::string* collide_key_value = base.FindString("collide_key");
  ASSERT_TRUE(collide_key_value);
  EXPECT_EQ("collide_key_value_merge", *collide_key_value);  // Replaced.
  const std::string* merge_key_value = base.FindString("merge_key");
  ASSERT_TRUE(merge_key_value);
  EXPECT_EQ("merge_key_value_merge", *merge_key_value);  // Merged in.

  Value::Dict* res_sub_dict = base.FindDict("sub_dict_key");
  ASSERT_TRUE(res_sub_dict);
  EXPECT_EQ(3U, res_sub_dict->size());
  const std::string* sub_base_key_value =
      res_sub_dict->FindString("sub_base_key");
  ASSERT_TRUE(sub_base_key_value);
  EXPECT_EQ("sub_base_key_value_base", *sub_base_key_value);  // Preserved.
  const std::string* sub_collide_key_value =
      res_sub_dict->FindString("sub_collide_key");
  ASSERT_TRUE(sub_collide_key_value);
  EXPECT_EQ("sub_collide_key_value_merge",
            *sub_collide_key_value);  // Replaced.
  const std::string* sub_merge_key_value =
      res_sub_dict->FindString("sub_merge_key");
  ASSERT_TRUE(sub_merge_key_value);
  EXPECT_EQ("sub_merge_key_value_merge", *sub_merge_key_value);  // Merged in.
}

TEST(ValuesTest, DictionaryIterator) {
  Value::Dict dict;
  for (Value::Dict::iterator it = dict.begin(); it != dict.end(); ++it) {
    ADD_FAILURE();
  }

  Value value1("value1");
  dict.Set("key1", value1.Clone());
  bool seen1 = false;
  for (Value::Dict::iterator it = dict.begin(); it != dict.end(); ++it) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it->first);
    EXPECT_EQ(value1, it->second);
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  Value value2("value2");
  dict.Set("key2", value2.Clone());
  bool seen2 = seen1 = false;
  for (Value::Dict::iterator it = dict.begin(); it != dict.end(); ++it) {
    if (it->first == "key1") {
      EXPECT_FALSE(seen1);
      EXPECT_EQ(value1, it->second);
      seen1 = true;
    } else if (it->first == "key2") {
      EXPECT_FALSE(seen2);
      EXPECT_EQ(value2, it->second);
      seen2 = true;
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(seen1);
  EXPECT_TRUE(seen2);
}

TEST(ValuesTest, MutatingCopiedPairsInDictMutatesUnderlyingValues) {
  Value::Dict dict;
  dict.Set("key", Value("initial value"));

  // Because the non-const dict iterates over <const std::string&, Value&>
  // pairs, it's possible to alter iterated-over values in place even when
  // "copying" the key-value pair:
  for (auto kv : dict) {
    kv.second.GetString() = "replacement";
  }

  std::string* found = dict.FindString("key");
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, "replacement");
}

TEST(ValuesTest, StdDictionaryIterator) {
  Value::Dict dict;
  for (auto it = dict.begin(); it != dict.end(); ++it) {
    ADD_FAILURE();
  }

  Value value1("value1");
  dict.Set("key1", value1.Clone());
  bool seen1 = false;
  for (auto it : dict) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it.first);
    EXPECT_EQ(value1, it.second);
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  Value value2("value2");
  dict.Set("key2", value2.Clone());
  bool seen2 = seen1 = false;
  for (auto it : dict) {
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

TEST(ValuesTest, SelfSwap) {
  base::Value test(1);
  std::swap(test, test);
  EXPECT_EQ(1, test.GetInt());
}

TEST(ValuesTest, FromToUniquePtrValue) {
  std::unique_ptr<Value> dict = std::make_unique<Value>(Value::Type::DICT);
  dict->GetDict().Set("name", "Froogle");
  dict->GetDict().Set("url", "http://froogle.com");
  Value dict_copy = dict->Clone();

  Value dict_converted = Value::FromUniquePtrValue(std::move(dict));
  EXPECT_EQ(dict_copy, dict_converted);

  std::unique_ptr<Value> val =
      Value::ToUniquePtrValue(std::move(dict_converted));
  EXPECT_EQ(dict_copy, *val);
}

TEST(ValuesTest, MutableFindStringPath) {
  Value::Dict dict;
  dict.SetByDottedPath("foo.bar", "value");

  *(dict.FindStringByDottedPath("foo.bar")) = "new_value";

  Value::Dict expected_dict;
  expected_dict.SetByDottedPath("foo.bar", "new_value");

  EXPECT_EQ(expected_dict, dict);
}

TEST(ValuesTest, MutableGetString) {
  Value value("value");
  value.GetString() = "new_value";
  EXPECT_EQ("new_value", value.GetString());
}

TEST(ValuesTest, MutableFindBlobPath) {
  Value::BlobStorage original_blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value::Dict dict;
  dict.SetByDottedPath("foo.bar", std::move(original_blob));

  Value::BlobStorage new_blob = {0x0, 0x3, 0x0};
  *(dict.FindBlobByDottedPath("foo.bar")) = new_blob;

  Value::Dict expected_dict;
  expected_dict.SetByDottedPath("foo.bar", std::move(new_blob));

  EXPECT_EQ(expected_dict, dict);
}

TEST(ValuesTest, MutableGetBlob) {
  Value::BlobStorage original_blob = {0xF, 0x0, 0x0, 0xB, 0xA, 0x2};
  Value value(std::move(original_blob));

  Value::BlobStorage new_blob = {0x0, 0x3, 0x0};
  value.GetBlob() = new_blob;
  EXPECT_EQ(new_blob, value.GetBlob());
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
TEST(ValuesTest, TracingSupport) {
  EXPECT_EQ(perfetto::TracedValueToString(Value(false)), "false");
  EXPECT_EQ(perfetto::TracedValueToString(Value(1)), "1");
  EXPECT_EQ(perfetto::TracedValueToString(Value(1.5)), "1.5");
  EXPECT_EQ(perfetto::TracedValueToString(Value("value")), "value");
  EXPECT_EQ(perfetto::TracedValueToString(Value(Value::Type::NONE)), "<none>");
  {
    Value::List list;
    EXPECT_EQ(perfetto::TracedValueToString(list), "{}");
    list.Append(2);
    list.Append(3);
    EXPECT_EQ(perfetto::TracedValueToString(list), "[2,3]");
    EXPECT_EQ(perfetto::TracedValueToString(Value(std::move(list))), "[2,3]");
  }
  {
    Value::Dict dict;
    EXPECT_EQ(perfetto::TracedValueToString(dict), "{}");
    dict.Set("key", "value");
    EXPECT_EQ(perfetto::TracedValueToString(dict), "{key:value}");
    EXPECT_EQ(perfetto::TracedValueToString(Value(std::move(dict))),
              "{key:value}");
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

TEST(ValueViewTest, BasicConstruction) {
  {
    ValueView v = true;
    EXPECT_EQ(true, absl::get<bool>(v.data_view_for_test()));
  }
  {
    ValueView v = 25;
    EXPECT_EQ(25, absl::get<int>(v.data_view_for_test()));
  }
  {
    ValueView v = 3.14;
    EXPECT_DOUBLE_EQ(3.14, absl::get<ValueView::DoubleStorageForTest>(
                               v.data_view_for_test()));
  }
  {
    ValueView v = std::string_view("hello world");
    EXPECT_EQ("hello world",
              absl::get<std::string_view>(v.data_view_for_test()));
  }
  {
    ValueView v = "hello world";
    EXPECT_EQ("hello world",
              absl::get<std::string_view>(v.data_view_for_test()));
  }
  {
    std::string str = "hello world";
    ValueView v = str;
    EXPECT_EQ("hello world",
              absl::get<std::string_view>(v.data_view_for_test()));
  }
  {
    Value::Dict dict;
    dict.Set("hello", "world");
    ValueView v = dict;
    EXPECT_EQ(dict, absl::get<std::reference_wrapper<const Value::Dict>>(
                        v.data_view_for_test()));
  }
  {
    Value::List list;
    list.Append("hello");
    list.Append("world");
    ValueView v = list;
    EXPECT_EQ(list, absl::get<std::reference_wrapper<const Value::List>>(
                        v.data_view_for_test()));
  }
}

TEST(ValueViewTest, ValueConstruction) {
  {
    Value val(true);
    ValueView v = val;
    EXPECT_EQ(true, absl::get<bool>(v.data_view_for_test()));
  }
  {
    Value val(25);
    ValueView v = val;
    EXPECT_EQ(25, absl::get<int>(v.data_view_for_test()));
  }
  {
    Value val(3.14);
    ValueView v = val;
    EXPECT_DOUBLE_EQ(3.14, absl::get<ValueView::DoubleStorageForTest>(
                               v.data_view_for_test()));
  }
  {
    Value val("hello world");
    ValueView v = val;
    EXPECT_EQ("hello world",
              absl::get<std::string_view>(v.data_view_for_test()));
  }
  {
    Value::Dict dict;
    dict.Set("hello", "world");
    Value val(dict.Clone());
    ValueView v = val;
    EXPECT_EQ(dict, absl::get<std::reference_wrapper<const Value::Dict>>(
                        v.data_view_for_test()));
  }
  {
    Value::List list;
    list.Append("hello");
    list.Append("world");
    Value val(list.Clone());
    ValueView v = val;
    EXPECT_EQ(list, absl::get<std::reference_wrapper<const Value::List>>(
                        v.data_view_for_test()));
  }
}

TEST(ValueViewTest, ToValue) {
  {
    Value val(true);
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
  {
    Value val(25);
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
  {
    Value val(3.14);
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
  {
    Value val("hello world");
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
  {
    Value::Dict dict;
    dict.Set("hello", "world");
    Value val(dict.Clone());
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
  {
    Value::List list;
    list.Append("hello");
    list.Append("world");
    Value val(list.Clone());
    Value to_val = ValueView(val).ToValue();
    EXPECT_EQ(val, to_val);
  }
}

}  // namespace base
