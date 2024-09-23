// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/types/expected_macros.h"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

expected<void, std::string> ReturnOk() {
  return ok();
}

expected<int, std::string> ReturnValue(int v) {
  return v;
}

expected<int, std::string> ReturnError(std::string_view msg) {
  return unexpected(std::string(msg));
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

TEST(ReturnIfError, Works) {
  const auto func = []() -> std::string {
    RETURN_IF_ERROR(ReturnOk());
    RETURN_IF_ERROR(ReturnOk());
    RETURN_IF_ERROR(ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
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

TEST(ReturnIfError, WorksWithLambda) {
  const auto func = []() -> std::string {
    RETURN_IF_ERROR([] { return ReturnOk(); }());
    RETURN_IF_ERROR([] { return ReturnError("EXPECTED"); }());
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
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

TEST(ReturnIfError, WorksWithAdaptorFunc) {
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

  EXPECT_EQ("EXPECTED A EXPECTED B", func());
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

TEST(ReturnIfError, WorksWithVoidReturnAdaptor) {
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

TEST(AssignOrReturn, Works) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(int value1, ReturnValue(1));
    EXPECT_EQ(1, value1);
    ASSIGN_OR_RETURN(const int value2, ReturnValue(2));
    EXPECT_EQ(2, value2);
    ASSIGN_OR_RETURN(const int& value3, ReturnValue(3));
    EXPECT_EQ(3, value3);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value4,
                     ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
}

TEST(AssignOrReturn, WorksWithExpectedReturn) {
  const auto func = []() -> expected<void, std::string> {
    ASSIGN_OR_RETURN(int value1, ReturnValue(1));
    EXPECT_EQ(1, value1);
    ASSIGN_OR_RETURN(const int value2, ReturnValue(2));
    EXPECT_EQ(2, value2);
    ASSIGN_OR_RETURN(const int& value3, ReturnValue(3));
    EXPECT_EQ(3, value3);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value4,
                     ReturnError("EXPECTED"));
    return ok();
  };

  EXPECT_THAT(func(), test::ErrorIs(::testing::Eq("EXPECTED")));
}

TEST(AssignOrReturn, WorksWithLambda) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(const int value1, [] { return ReturnValue(1); }());
    EXPECT_EQ(1, value1);
    ASSIGN_OR_RETURN([[maybe_unused]] const int value2,
                     [] { return ReturnError("EXPECTED"); }());
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
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
    EXPECT_EQ((std::tuple(1, 1)), t1);
    ASSIGN_OR_RETURN((const std::tuple<int, std::tuple<int, int>, int> t2),
                     ReturnTupleValue(1, std::tuple(1, 1), 1));
    EXPECT_EQ((std::tuple(1, std::tuple(1, 1), 1)), t2);
    ASSIGN_OR_RETURN(
        ([[maybe_unused]] const std::tuple<int, std::tuple<int, int>, int> t3),
        (ReturnTupleError<int, std::tuple<int, int>, int>("EXPECTED")));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
}

TEST(AssignOrReturn, WorksWithStructuredBindings) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN((const auto& [t1, t2, t3, t4, t5]),
                     ReturnTupleValue(std::tuple(1, 1), 1, 2, 3, 4));
    EXPECT_EQ((std::tuple(1, 1)), t1);
    EXPECT_EQ(1, t2);
    EXPECT_EQ(2, t3);
    EXPECT_EQ(3, t4);
    EXPECT_EQ(4, t5);
    ASSIGN_OR_RETURN([[maybe_unused]] int t6, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
}

TEST(AssignOrReturn, WorksWithParenthesesAndDereference) {
  const auto func = []() -> std::string {
    int integer;
    int* pointer_to_integer = &integer;
    ASSIGN_OR_RETURN((*pointer_to_integer), ReturnValue(1));
    EXPECT_EQ(1, integer);
    ASSIGN_OR_RETURN(*pointer_to_integer, ReturnValue(2));
    EXPECT_EQ(2, integer);
    // Test where the order of dereference matters.
    pointer_to_integer--;
    int* const* const pointer_to_pointer_to_integer = &pointer_to_integer;
    ASSIGN_OR_RETURN((*pointer_to_pointer_to_integer)[1], ReturnValue(3));
    EXPECT_EQ(3, integer);
    ASSIGN_OR_RETURN([[maybe_unused]] const int t1, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
}

TEST(AssignOrReturn, WorksWithAdaptorFunc) {
  const auto fail_test_if_called = [](std::string error) {
    ADD_FAILURE();
    return error;
  };
  const auto adaptor = [](std::string error) { return error + " EXPECTED B"; };
  const auto func = [&]() -> std::string {
    ASSIGN_OR_RETURN([[maybe_unused]] int value, ReturnValue(1),
                     fail_test_if_called);
    ASSIGN_OR_RETURN(value, ReturnError("EXPECTED A"), adaptor);
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED A EXPECTED B", func());
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

TEST(AssignOrReturn, WorksWithVoidReturnAdaptor) {
  int code = 0;
  int phase = 0;
  const auto adaptor = [&](std::string error) { ++code; };
  const auto func = [&]() -> void {
    ASSIGN_OR_RETURN(phase, ReturnValue(1), adaptor);
    phase = 2;
    ASSIGN_OR_RETURN(phase, ReturnError("EXPECTED A"), adaptor);
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

  EXPECT_EQ("EXPECTED A EXPECTED B", func());
}

TEST(AssignOrReturn, WorksWithAppendIncludingLocals) {
  const auto func = [&](const std::string& str) -> std::string {
    ASSIGN_OR_RETURN([[maybe_unused]] const int value,
                     ReturnError("EXPECTED A"),
                     [&](std::string e) { return e + str; });
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED A EXPECTED B", func(" EXPECTED B"));
}

TEST(AssignOrReturn, WorksForExistingVariable) {
  const auto func = []() -> std::string {
    int value = 1;
    ASSIGN_OR_RETURN(value, ReturnValue(2));
    EXPECT_EQ(2, value);
    ASSIGN_OR_RETURN(value, ReturnValue(3));
    EXPECT_EQ(3, value);
    ASSIGN_OR_RETURN(value, ReturnError("EXPECTED"));
    return "ERROR";
  };

  EXPECT_EQ("EXPECTED", func());
}

TEST(AssignOrReturn, UniquePtrWorks) {
  const auto func = []() -> std::string {
    ASSIGN_OR_RETURN(const std::unique_ptr<int> ptr, ReturnPtrValue(1));
    EXPECT_EQ(*ptr, 1);
    return "EXPECTED";
  };

  EXPECT_EQ("EXPECTED", func());
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

  EXPECT_EQ("EXPECTED", func());
}

}  // namespace
}  // namespace base
