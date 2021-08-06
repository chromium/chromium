// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/fake_iasync_operation_win.h"

#include <asyncinfo.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/test/async_results_test_values_win.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using Microsoft::WRL::Callback;
using Microsoft::WRL::Make;

namespace base {
namespace win {
namespace {
constexpr HRESULT kTestError = 0x87654321;
}

template <typename T>
class FakeIAsyncOperationTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(FakeIAsyncOperationTest);

TYPED_TEST_P(FakeIAsyncOperationTest, MultipleCompletedHandlers) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();
  auto handler = Callback<IAsyncOperationCompletedHandler<TypeParam>>(
      [](auto async_operation, AsyncStatus async_status) { return S_OK; });
  ASSERT_HRESULT_SUCCEEDED(operation->put_Completed(handler.Get()));
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_SUCCEEDED(operation->put_Completed(handler.Get()));
      , "put_Completed");
}

TYPED_TEST_P(FakeIAsyncOperationTest, MultipleCompletions) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();
  base::test::AsyncResultsTestValues<TypeParam> test_values;
  ASSERT_NO_FATAL_FAILURE(
      operation->CompleteWithResults(test_values.GetTestValue_T()));
  // EXPECT_FATAL_FAILURE() can only reference globals and statics.
  // https://github.com/google/googletest/blob/master/docs/advanced.md#catching-failures
  static auto test_value_t = test_values.GetTestValue_T();
  static auto& static_operation = operation;
  EXPECT_FATAL_FAILURE(static_operation->CompleteWithResults(test_value_t),
                       "already completed");

  operation = Make<FakeIAsyncOperation<TypeParam>>();
  static_operation = operation;
  ASSERT_NO_FATAL_FAILURE(operation->CompleteWithError(E_FAIL));
  EXPECT_FATAL_FAILURE(static_operation->CompleteWithError(E_FAIL),
                       "already completed");

  operation = Make<FakeIAsyncOperation<TypeParam>>();
  static_operation = operation;
  ASSERT_NO_FATAL_FAILURE(operation->CompleteWithError(E_FAIL));
  EXPECT_FATAL_FAILURE(static_operation->CompleteWithResults(test_value_t),
                       "already completed");

  operation = Make<FakeIAsyncOperation<TypeParam>>();
  static_operation = operation;
  ASSERT_NO_FATAL_FAILURE(
      operation->CompleteWithResults(test_values.GetTestValue_T()));
  EXPECT_FATAL_FAILURE(static_operation->CompleteWithError(E_FAIL),
                       "already completed");
}

TYPED_TEST_P(FakeIAsyncOperationTest, CompleteWithResults_WithHandler) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();

  bool completed_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(operation->put_Completed(
      Callback<IAsyncOperationCompletedHandler<TypeParam>>(
          [operation, &completed_handler_called](auto async_operation,
                                                 AsyncStatus async_status) {
            completed_handler_called = true;
            EXPECT_EQ(operation.Get(), async_operation);
            EXPECT_EQ(async_status, AsyncStatus::Completed);
            return S_OK;
          })
          .Get()));
  ASSERT_FALSE(completed_handler_called);
  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Started);
  HRESULT error_code;
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  base::test::AsyncResultsTestValues<TypeParam> test_values;
  auto results = test_values.GetDefaultValue_AsyncResultsT();
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(operation->GetResults(&results)), "GetResults");

  operation->CompleteWithResults(test_values.GetTestValue_AsyncResultsT());
  ASSERT_TRUE(completed_handler_called);
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Completed);
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  ASSERT_HRESULT_SUCCEEDED(operation->GetResults(&results));
  ASSERT_EQ(results, test_values.GetTestValue_AsyncResultsT());
}

TYPED_TEST_P(FakeIAsyncOperationTest, CompleteWithResults_WithoutHandler) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();

  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Started);
  HRESULT error_code;
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  base::test::AsyncResultsTestValues<TypeParam> test_values;
  auto results = test_values.GetDefaultValue_AsyncResultsT();
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(operation->GetResults(&results)), "GetResults");

  operation->CompleteWithResults(test_values.GetTestValue_AsyncResultsT());
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Completed);
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  ASSERT_HRESULT_SUCCEEDED(operation->GetResults(&results));
  ASSERT_EQ(results, test_values.GetTestValue_AsyncResultsT());
}

TYPED_TEST_P(FakeIAsyncOperationTest, CompleteWithError_WithHandler) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();

  bool completed_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(operation->put_Completed(
      Callback<IAsyncOperationCompletedHandler<TypeParam>>(
          [operation, &completed_handler_called](auto async_operation,
                                                 AsyncStatus async_status) {
            completed_handler_called = true;
            EXPECT_EQ(operation.Get(), async_operation);
            EXPECT_EQ(async_status, AsyncStatus::Error);
            return S_OK;
          })
          .Get()));
  ASSERT_FALSE(completed_handler_called);
  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Started);
  HRESULT error_code;
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  base::test::AsyncResultsTestValues<TypeParam> test_values;
  auto results = test_values.GetDefaultValue_AsyncResultsT();
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(operation->GetResults(&results)), "GetResults");

  operation->CompleteWithError(kTestError);
  ASSERT_TRUE(completed_handler_called);
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Error);
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, kTestError);
  ASSERT_HRESULT_FAILED(operation->GetResults(&results));
}

TYPED_TEST_P(FakeIAsyncOperationTest, CompleteWithError_WithoutHandler) {
  auto operation = Make<FakeIAsyncOperation<TypeParam>>();

  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Started);
  HRESULT error_code;
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, S_OK);
  base::test::AsyncResultsTestValues<TypeParam> test_values;
  auto results = test_values.GetDefaultValue_AsyncResultsT();
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(operation->GetResults(&results)), "GetResults");

  operation->CompleteWithError(kTestError);
  ASSERT_HRESULT_SUCCEEDED(operation->get_Status(&async_status));
  ASSERT_EQ(async_status, AsyncStatus::Error);
  ASSERT_HRESULT_SUCCEEDED(operation->get_ErrorCode(&error_code));
  ASSERT_EQ(error_code, kTestError);
  ASSERT_HRESULT_FAILED(operation->GetResults(&results));
}

REGISTER_TYPED_TEST_SUITE_P(FakeIAsyncOperationTest,
                            MultipleCompletedHandlers,
                            MultipleCompletions,
                            CompleteWithResults_WithHandler,
                            CompleteWithResults_WithoutHandler,
                            CompleteWithError_WithHandler,
                            CompleteWithError_WithoutHandler);

INSTANTIATE_TYPED_TEST_SUITE_P(Win,
                               FakeIAsyncOperationTest,
                               base::test::AsyncResultsTestValuesTypes);

}  // namespace win
}  // namespace base
