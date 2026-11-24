// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/expected_macros.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/compiler_specific.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

expected<void, std::string> ReturnOk() {
  return ok();
}

expected<int, std::string> ReturnExpectedValue(int v) {
  return v;
}

std::optional<int> ReturnOptionalValue(int v) {
  return v;
}

expected<int, std::string> ReturnError(std::string_view msg) {
  return unexpected(std::string(msg));
}

std::optional<int> ReturnNullopt() {
  return std::nullopt;
}

template <class... Args>
expected<std::tuple<Args...>, std::string> ReturnTupleValue(Args&&... v) {
  return std::tuple<Args...>(std::forward<Args>(v)...);
}

template <class... Args>
expected<std::tuple<Args...>, std::string> ReturnTupleError(
    std::string_view msg) {
  return unexpected(std::string(msg));
}

expected<std::unique_ptr<int>, std::string> ReturnPtrValue(int v) {
  return std::make_unique<int>(v);
}

expected<std::string, std::unique_ptr<int>> ReturnPtrError(int v) {
  return unexpected(std::make_unique<int>(v));
}

TEST(ReturnIfError, ExpectedWorks) {
  const auto func = []() -> std::string {
    RETURN_IF_ERROR(ReturnOk());
    expected<int, std::string> e_int1 = 1;
    RETURN_IF_ERROR(e_int1);
    const expected<int, std::string> e_int2 = 2;
    RETURN_IF_ERROR(e_int2);
    RETURN_IF_ERROR(ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(ReturnIfError, OptionalWorks) {
  int phase = 0;
  const auto func = [&]() -> std::optional<int> {
    phase = 1;
    RETURN_IF_ERROR(ReturnOptionalValue(0));
    phase = 2;
    std::optional<int> o_int1 = 1;
    RETURN_IF_ERROR(o_int1);
    phase = 3;
    const std::optional<int> o_int2 = 2;
    RETURN_IF_ERROR(o_int2);
    phase = 4;
    RETURN_IF_ERROR(ReturnNullopt());
    phase = 5;
    return 3;
  };

  EXPECT_EQ(func(), std::nullopt);
  EXPECT_EQ(phase, 4);
}

TEST(ReturnIfError, WorksWithExpectedReturn) {
  const auto func = []() -> expected<void, std::string> {
    RETURN_IF_ERROR(ReturnOk());
    RETURN_IF_ERROR(ReturnOk());
    RETURN_IF_ERROR(ReturnError("EXPECTED"));
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Eq("EXPECTED")));
}

TEST(ReturnIfError, ExpectedWorksWithLambda) {
  const auto func = []() -> std::string {
    RETURN_IF_ERROR([] { return ReturnOk(); }());
    RETURN_IF_ERROR([] { return ReturnError("EXPECTED"); }());
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(ReturnIfError, OptionalWorksWithLambda) {
  int phase = 0;
  const auto func = [&]() -> std::optional<int> {
    phase = 1;
    RETURN_IF_ERROR([] { return ReturnOptionalValue(1); }());
    phase = 2;
    RETURN_IF_ERROR([] { return ReturnNullopt(); }());
    phase = 3;
    return 2;
  };

  EXPECT_EQ(func(), std::nullopt);
  EXPECT_EQ(phase, 2);
}

TEST(ReturnIfError, WorksWithMoveOnlyType) {
  const auto func = []() -> std::unique_ptr<int> {
    RETURN_IF_ERROR([] { return ReturnPtrError(1); }());
    return nullptr;
  };

  EXPECT_THAT(func(), ::testing::Pointee(::testing::Eq(1)));
}

TEST(ReturnIfError, WorksWithMoveOnlyTypeAndExpectedReturn) {
  const auto func = []() -> expected<void, std::unique_ptr<int>> {
    RETURN_IF_ERROR([] { return ReturnPtrError(1); }());
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Pointee(::testing::Eq(1))));
}

TEST(ReturnIfError, ExpectedWorksWithAdaptorFunc) {
  const auto fail_test_if_called = [](std::string error) {
    ADD_FAILURE();
    return error;
  };
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> std::string {
    RETURN_IF_ERROR(ReturnOk(), fail_test_if_called);
    RETURN_IF_ERROR(ReturnError("EXPECTED A"), adaptor);
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED A EXPECTED B");
}

TEST(ReturnIfError, OptionalWorksWithAdaptorFunc) {
  const auto func = [&]() -> const char* {
    RETURN_IF_ERROR(ReturnOptionalValue(1), []() { return "ERROR 1"; });
    RETURN_IF_ERROR(ReturnNullopt(), []() { return "EXPECTED"; });
    return "ERROR 2";
  };

  EXPECT_STREQ(func(), "EXPECTED");
}

TEST(ReturnIfError, ExpectedWorksWithNonCopyableAdaptorFunc) {
  struct Adaptor {
    Adaptor() = default;
    Adaptor(const Adaptor&) = delete;
    Adaptor& operator=(const Adaptor&) = delete;

    std::string operator()(std::string error) const {
      return error + " EXPECTED B";
    }
  };
  const Adaptor adaptor;
  const auto func = [&]() -> std::string {
    RETURN_IF_ERROR(ReturnError("EXPECTED A"), adaptor);
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED A EXPECTED B");
}

TEST(ReturnIfError, ExpectedWorksWithMemberFunctionAdaptor) {
  struct Adaptor {
    std::string Adapt(std::string error) const { return error + " EXPECTED B"; }
  };
  const Adaptor adaptor;
  const auto func = [&]() -> std::string {
    RETURN_IF_ERROR(ReturnError("EXPECTED A"), &Adaptor::Adapt, &adaptor);
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED A EXPECTED B");
}

TEST(ReturnIfError, WorksWithAdaptorFuncAndExpectedReturn) {
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> expected<void, std::string> {
    RETURN_IF_ERROR(ReturnError("EXPECTED A"), adaptor);
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Eq("EXPECTED A EXPECTED B")));
}

TEST(ReturnIfError, WorksWithAdaptorFuncAndMoveOnlyType) {
  const auto adaptor = [](std::unique_ptr<int> error) {
    return std::make_unique<int>(2);
  };
  const auto func = [&]() -> std::unique_ptr<int> {
    RETURN_IF_ERROR(ReturnPtrError(1), adaptor);
    return nullptr;
  };

  EXPECT_THAT(func(), ::testing::Pointee(::testing::Eq(2)));
}

TEST(ReturnIfError, WorksWithAdaptorFuncAndMoveOnlyTypeAndExpectedReturn) {
  const auto adaptor = [](std::unique_ptr<int> error) {
    return std::make_unique<int>(2);
  };
  const auto func = [&]() -> expected<void, std::unique_ptr<int>> {
    RETURN_IF_ERROR(ReturnPtrError(1), adaptor);
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Pointee(::testing::Eq(2))));
}

TEST(ReturnIfError, WorksWithAdaptorReturningImmovableType) {
  struct Immovable {
    explicit Immovable(int v) : value(v) {}
    ~Immovable() = default;
    Immovable(const Immovable&) = delete;
    Immovable& operator=(const Immovable&) = delete;

    const int value;
  };

  const auto optional_func = []() -> std::optional<Immovable> {
    RETURN_IF_ERROR(ReturnError("ERROR"), [](auto) -> std::optional<Immovable> {
      return std::nullopt;
    });
    return std::optional<Immovable>(std::in_place, 1);
  };
  EXPECT_EQ(optional_func(), std::nullopt);

  const auto direct_func = []() -> Immovable {
    RETURN_IF_ERROR(ReturnNullopt(), [] { return Immovable(2); });
    return Immovable(1);
  };
  EXPECT_EQ(direct_func().value, 2);
}

TEST(ReturnIfError, ExpectedWorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  const auto adaptor = [&](std::string error) { ++code; };
  const auto func = [&]() -> void {
    phase = 1;
    RETURN_IF_ERROR(ReturnOk(), adaptor);
    phase = 2;
    RETURN_IF_ERROR(ReturnError("EXPECTED A"), adaptor);
    phase = 3;
  };

  func();
  EXPECT_EQ(phase, 2);
  EXPECT_EQ(code, 1);
}

TEST(ReturnIfError, OptionalWorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  const auto adaptor = [&]() { ++code; };
  const auto func = [&]() -> void {
    phase = 1;
    RETURN_IF_ERROR(ReturnOptionalValue(1), adaptor);
    phase = 2;
    RETURN_IF_ERROR(ReturnNullopt(), adaptor);
    phase = 3;
  };

  func();
  EXPECT_EQ(phase, 2);
  EXPECT_EQ(code, 1);
}

TEST(ReturnIfError, ExpectedWorksWithValue) {
  const auto func = []() -> int {
    RETURN_IF_ERROR(ReturnOk(), 1);
    RETURN_IF_ERROR(ReturnError("ERROR"), 2);
    return 3;
  };

  EXPECT_EQ(func(), 2);
}

TEST(ReturnIfError, OptionalWorksWithValue) {
  const auto func = []() -> int {
    RETURN_IF_ERROR(ReturnOptionalValue(0), 1);
    RETURN_IF_ERROR(ReturnNullopt(), 2);
    return 3;
  };

  EXPECT_EQ(func(), 2);
}

TEST(ReturnIfError, WorksWithUnexpectedValue) {
  const auto func = []() -> expected<void, int> {
    RETURN_IF_ERROR(ReturnOk(), unexpected(1));
    RETURN_IF_ERROR(ReturnError("ERROR"), unexpected(2));
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(2));
}

// An lvalue is not copied, so a function that returns a reference or a view can
// return it like in a return statement.
TEST(ReturnIfError, DoesNotCopyLvalueValue) {
  const std::string value = "VALUE";
  const std::string ok_value = "OK";
  const auto func = [&]() -> const std::string& {
    RETURN_IF_ERROR(ReturnError("ERROR"), value);
    return ok_value;
  };

  EXPECT_EQ(&func(), &value);

  const auto optional_func = [&]() -> const std::string& {
    RETURN_IF_ERROR(ReturnNullopt(), value);
    return ok_value;
  };

  EXPECT_EQ(&optional_func(), &value);
}

// A value converts to the function's return type like in a return statement.
TEST(ReturnIfError, ConvertsValueToReturnType) {
  const auto string_func = []() -> std::string {
    RETURN_IF_ERROR(ReturnError("ERROR"), "VALUE");
    return "OK";
  };
  EXPECT_EQ(string_func(), "VALUE");

  const auto ptr_func = []() -> std::unique_ptr<const int> {
    RETURN_IF_ERROR(ReturnNullopt(), std::make_unique<int>(1));
    return nullptr;
  };
  EXPECT_THAT(ptr_func(), ::testing::Pointee(1));

  const auto optional_func = []() -> std::optional<std::string> {
    RETURN_IF_ERROR(ReturnError("ERROR"), "VALUE");
    return "OK";
  };
  EXPECT_THAT(optional_func(), ::testing::Optional(std::string("VALUE")));

  const auto nested_optional_func = []() -> std::optional<std::optional<int>> {
    RETURN_IF_ERROR(ReturnError("ERROR"), std::nullopt);
    return std::optional<int>(1);
  };
  EXPECT_EQ(nested_optional_func(), std::nullopt);

  // Temporaries in the value expression live until after conversion to the
  // return type.
  constexpr auto temporary_in_value = []() -> std::string {
    RETURN_IF_ERROR((expected<int, int>(unexpect, 1)),
                    std::string("LONG_VALUE_EXCEEDING_SSO_BUFFER").c_str());
    return "OK";
  };
  static_assert(temporary_in_value() == "LONG_VALUE_EXCEEDING_SSO_BUFFER");
  EXPECT_EQ(temporary_in_value(), "LONG_VALUE_EXCEEDING_SSO_BUFFER");
}

TEST(ReturnIfError, WorksWithMoveOnlyValue) {
  const auto func = []() -> std::unique_ptr<int> {
    RETURN_IF_ERROR(ReturnError("ERROR"), std::make_unique<int>(1));
    return nullptr;
  };

  EXPECT_THAT(func(), ::testing::Pointee(1));
}

TEST(ReturnIfError, OnlyEvaluatesValueOnError) {
  int evaluations = 0;
  const auto func = [&]() -> int {
    RETURN_IF_ERROR(ReturnOk(), ++evaluations);
    RETURN_IF_ERROR(ReturnOptionalValue(0), ++evaluations);
    RETURN_IF_ERROR(ReturnError("ERROR"), ++evaluations);
    return 0;
  };

  EXPECT_EQ(func(), 1);
  EXPECT_EQ(evaluations, 1);
}

TEST(AssignOrReturn, ExpectedWorks) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(int value1, ReturnExpectedValue(1));
    EXPECT_EQ(value1, 1);
    ASSIGN_OR_RETURN(const int value2, ReturnExpectedValue(2));
    EXPECT_EQ(value2, 2);
    ASSIGN_OR_RETURN(const int& value3, ReturnExpectedValue(3));
    EXPECT_EQ(value3, 3);
    expected<int, std::string> e_int4 = 4;
    ASSIGN_OR_RETURN(int value4, e_int4);
    EXPECT_EQ(value4, 4);
    const expected<int, std::string> e_int5 = 5;
    ASSIGN_OR_RETURN(int value5, e_int5);
    EXPECT_EQ(value5, 5);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value6,
                     ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, OptionalWorks) {
  int phase = 0;
  const auto func = [&]() -> std::optional<int> {
    phase = 1;
    ASSIGN_OR_RETURN(int value1, ReturnOptionalValue(1));
    EXPECT_EQ(value1, 1);
    phase = 2;
    ASSIGN_OR_RETURN(const int value2, ReturnOptionalValue(2));
    EXPECT_EQ(value2, 2);
    phase = 3;
    ASSIGN_OR_RETURN(const int& value3, ReturnOptionalValue(3));
    EXPECT_EQ(value3, 3);
    phase = 4;
    std::optional<int> o_int4 = 4;
    ASSIGN_OR_RETURN(int value4, o_int4);
    EXPECT_EQ(value4, 4);
    phase = 5;
    const std::optional<int> o_int5 = 5;
    ASSIGN_OR_RETURN(int value5, o_int5);
    EXPECT_EQ(value5, 5);
    phase = 6;
    ASSIGN_OR_RETURN([[maybe_unused]] const int value6, ReturnNullopt());
    phase = 7;
    return 6;
  };

  EXPECT_EQ(func(), std::nullopt);
  EXPECT_EQ(phase, 6);
}

TEST(AssignOrReturn, WorksWithExpectedReturn) {
  const auto func = []() -> expected<void, std::string> {
    ASSIGN_OR_RETURN(int value1, ReturnExpectedValue(1));
    EXPECT_EQ(value1, 1);
    ASSIGN_OR_RETURN(const int value2, ReturnExpectedValue(2));
    EXPECT_EQ(value2, 2);
    ASSIGN_OR_RETURN(const int& value3, ReturnExpectedValue(3));
    EXPECT_EQ(value3, 3);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value4,
                     ReturnError("EXPECTED"));
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Eq("EXPECTED")));
}

TEST(AssignOrReturn, ExpectedWorksWithLambda) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(const int value1, [] { return ReturnExpectedValue(1); }());
    EXPECT_EQ(value1, 1);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value2,
                     [] { return ReturnError("EXPECTED"); }());
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, OptionalWorksWithLambda) {
  int phase = 0;
  const auto func = [&]() -> std::optional<int> {
    phase = 1;
    ASSIGN_OR_RETURN(const int value1, [] { return ReturnOptionalValue(1); }());
    EXPECT_EQ(value1, 1);
    phase = 2;
    ASSIGN_OR_RETURN([[maybe_unused]] const int value2,
                     [] { return ReturnNullopt(); }());
    phase = 3;
    return 2;
  };

  EXPECT_EQ(func(), std::nullopt);
  EXPECT_EQ(phase, 2);
}

TEST(AssignOrReturn, OptionalWorksWithDifferentTypes) {
  int phase = 0;
  const auto func = [&]() -> std::optional<std::string> {
    phase = 1;
    ASSIGN_OR_RETURN([[maybe_unused]] int value1, ReturnNullopt());
    phase = 2;
    return "";
  };

  EXPECT_EQ(func(), std::nullopt);
  EXPECT_EQ(phase, 1);
}

TEST(AssignOrReturn, UniquePtrWorksWithNullptrAdapter) {
  int phase = 0;
  const auto func = [&]() -> std::unique_ptr<int> {
    phase = 1;
    ASSIGN_OR_RETURN(int value, ReturnError("ERROR"),
                     [](auto&&) { return nullptr; });
    phase = 2;
    return std::make_unique<int>(value);
  };

  EXPECT_EQ(func(), nullptr);
  EXPECT_EQ(phase, 1);
}

TEST(AssignOrReturn, UniquePtrWorksWithNullptr) {
  int phase = 0;
  const auto func = [&]() -> std::unique_ptr<int> {
    phase = 1;
    ASSIGN_OR_RETURN(int value, ReturnError("ERROR"), nullptr);
    phase = 2;
    return std::make_unique<int>(value);
  };

  EXPECT_EQ(func(), nullptr);
  EXPECT_EQ(phase, 1);
}

TEST(AssignOrReturn, ExpectedWorksWithValue) {
  const auto func = []() -> bool {
    ASSIGN_OR_RETURN(const int value1, ReturnExpectedValue(1), false);
    EXPECT_EQ(value1, 1);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value2, ReturnError("ERROR"),
                     false);
    return true;
  };

  EXPECT_FALSE(func());
}

TEST(AssignOrReturn, OptionalWorksWithValue) {
  const auto func = []() -> bool {
    ASSIGN_OR_RETURN(const int value1, ReturnOptionalValue(1), false);
    EXPECT_EQ(value1, 1);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value2, ReturnNullopt(), false);
    return true;
  };

  EXPECT_FALSE(func());
}

TEST(AssignOrReturn, WorksWithMoveOnlyType) {
  const auto func = []() -> std::unique_ptr<int> {
    ASSIGN_OR_RETURN([[maybe_unused]] const std::string s,
                     [] { return ReturnPtrError(1); }());
    return nullptr;
  };

  EXPECT_THAT(func(), ::testing::Pointee(::testing::Eq(1)));
}

TEST(AssignOrReturn, WorksWithMoveOnlyTypeAndExpectedReturn) {
  const auto func = []() -> expected<void, std::unique_ptr<int>> {
    ASSIGN_OR_RETURN([[maybe_unused]] const std::string s,
                     [] { return ReturnPtrError(1); }());
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Pointee(::testing::Eq(1))));
}

TEST(AssignOrReturn, WorksWithCommasInType) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN((const std::tuple<int, int> t1), ReturnTupleValue(1, 1));
    EXPECT_EQ(t1, (std::tuple(1, 1)));
    ASSIGN_OR_RETURN((const std::tuple<int, std::tuple<int, int>, int> t2),
                     ReturnTupleValue(1, std::tuple(1, 1), 1));
    EXPECT_EQ(t2, (std::tuple(1, std::tuple(1, 1), 1)));
    ASSIGN_OR_RETURN(
        ([[maybe_unused]] const std::tuple<int, std::tuple<int, int>, int> t3),
        (ReturnTupleError<int, std::tuple<int, int>, int>("EXPECTED")));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, WorksWithStructuredBindings) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN((const auto& [t1, t2, t3, t4, t5]),
                     ReturnTupleValue(std::tuple(1, 1), 1, 2, 3, 4));
    EXPECT_EQ(t1, (std::tuple(1, 1)));
    EXPECT_EQ(t2, 1);
    EXPECT_EQ(t3, 2);
    EXPECT_EQ(t4, 3);
    EXPECT_EQ(t5, 4);
    ASSIGN_OR_RETURN([[maybe_unused]] int t6, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, WorksWithParenthesesAndDereference) {
  const auto func = []() -> std::string {
    int integer;
    int* pointer_to_integer = &integer;
    ASSIGN_OR_RETURN((*pointer_to_integer), ReturnExpectedValue(1));
    EXPECT_EQ(integer, 1);
    ASSIGN_OR_RETURN(*pointer_to_integer, ReturnExpectedValue(2));
    EXPECT_EQ(integer, 2);
    // Test where the order of dereference matters.
    UNSAFE_TODO(--pointer_to_integer);
    int* const* const pointer_to_pointer_to_integer = &pointer_to_integer;
    UNSAFE_TODO(ASSIGN_OR_RETURN((*pointer_to_pointer_to_integer)[1],
                                 ReturnExpectedValue(3)));
    EXPECT_EQ(integer, 3);
    ASSIGN_OR_RETURN([[maybe_unused]] const int t1, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, ExpectedWorksWithAdaptorFunc) {
  const auto fail_test_if_called = [](std::string error) {
    ADD_FAILURE();
    return error;
  };
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> std::string {
    ASSIGN_OR_RETURN(int value, ReturnExpectedValue(1), fail_test_if_called);
    EXPECT_EQ(value, 1);
    ASSIGN_OR_RETURN(value, ReturnError("EXPECTED A"), adaptor);
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED A EXPECTED B");
}

TEST(AssignOrReturn, OptionalWorksWithAdaptorFunc) {
  const auto func = [&]() -> const char* {
    ASSIGN_OR_RETURN(int value, ReturnOptionalValue(1),
                     []() { return "ERROR 1"; });
    EXPECT_EQ(value, 1);
    ASSIGN_OR_RETURN(value, ReturnNullopt(), []() { return "EXPECTED"; });
    return "ERROR 2";
  };

  EXPECT_STREQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, WorksWithAdaptorFuncAndExpectedReturn) {
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> expected<void, std::string> {
    ASSIGN_OR_RETURN([[maybe_unused]] const int value,
                     ReturnError("EXPECTED A"), adaptor);
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Eq("EXPECTED A EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithAdaptorFuncAndMoveOnlyType) {
  const auto adaptor = [](std::unique_ptr<int> error) {
    return std::make_unique<int>(2);
  };
  const auto func = [&]() -> std::unique_ptr<int> {
    ASSIGN_OR_RETURN([[maybe_unused]] const std::string s, ReturnPtrError(1),
                     adaptor);
    return nullptr;
  };

  EXPECT_THAT(func(), ::testing::Pointee(::testing::Eq(2)));
}

TEST(AssignOrReturn, WorksWithAdaptorFuncAndMoveOnlyTypeAndExpectedReturn) {
  const auto adaptor = [](std::unique_ptr<int> error) {
    return std::make_unique<int>(2);
  };
  const auto func = [&]() -> expected<void, std::unique_ptr<int>> {
    ASSIGN_OR_RETURN([[maybe_unused]] const std::string s, ReturnPtrError(1),
                     adaptor);
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Pointee(::testing::Eq(2))));
}

TEST(AssignOrReturn, ExpectedWorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  const auto adaptor = [&](std::string error) { ++code; };
  const auto func = [&]() -> void {
    ASSIGN_OR_RETURN(phase, ReturnExpectedValue(1), adaptor);
    phase = 2;
    ASSIGN_OR_RETURN(phase, ReturnError("EXPECTED A"), adaptor);
    phase = 3;
  };

  func();
  EXPECT_EQ(phase, 2);
  EXPECT_EQ(code, 1);
}

TEST(AssignOrReturn, OptionalWorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  const auto adaptor = [&]() { ++code; };
  const auto func = [&]() -> void {
    ASSIGN_OR_RETURN(phase, ReturnOptionalValue(1), adaptor);
    phase = 2;
    ASSIGN_OR_RETURN(phase, ReturnNullopt(), adaptor);
    phase = 3;
  };

  func();
  EXPECT_EQ(phase, 2);
  EXPECT_EQ(code, 1);
}

TEST(AssignOrReturn, WorksWithThirdArgumentAndCommas) {
  const auto fail_test_if_called = [](std::string error) {
    ADD_FAILURE();
    return error;
  };
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> std::string {
    ASSIGN_OR_RETURN((const auto& [t1, t2, t3]), ReturnTupleValue(1, 2, 3),
                     fail_test_if_called);
    EXPECT_EQ(t1, 1);
    EXPECT_EQ(t2, 2);
    EXPECT_EQ(t3, 3);
    ASSIGN_OR_RETURN(([[maybe_unused]] const auto& [t4, t5, t6]),
                     (ReturnTupleError<int, int, int>("EXPECTED A")), adaptor);
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED A EXPECTED B");
}

TEST(AssignOrReturn, WorksWithAppendIncludingLocals) {
  const auto func = [&](const std::string& str) -> std::string {
    ASSIGN_OR_RETURN([[maybe_unused]] const int value,
                     ReturnError("EXPECTED A"),
                     [&](std::string e) { return e + str; });
    return "ERROR";
  };

  EXPECT_EQ(func(" EXPECTED B"), "EXPECTED A EXPECTED B");
}

TEST(AssignOrReturn, WorksForExistingVariable) {
  const auto func = []() -> std::string {
    int value = 1;
    ASSIGN_OR_RETURN(value, ReturnExpectedValue(2));
    EXPECT_EQ(value, 2);
    ASSIGN_OR_RETURN(value, ReturnExpectedValue(3));
    EXPECT_EQ(value, 3);
    ASSIGN_OR_RETURN(value, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, UniquePtrWorks) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(const std::unique_ptr<int> ptr, ReturnPtrValue(1));
    EXPECT_EQ(*ptr, 1);
    return "EXPECTED";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

TEST(AssignOrReturn, UniquePtrWorksForExistingVariable) {
  const auto func = []() -> std::string {
    std::unique_ptr<int> ptr;
    ASSIGN_OR_RETURN(ptr, ReturnPtrValue(1));
    EXPECT_EQ(*ptr, 1);

    ASSIGN_OR_RETURN(ptr, ReturnPtrValue(2));
    EXPECT_EQ(*ptr, 2);
    return "EXPECTED";
  };

  EXPECT_EQ(func(), "EXPECTED");
}

}  // namespace
}  // namespace base
