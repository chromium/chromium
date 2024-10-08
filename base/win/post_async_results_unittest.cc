// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/post_async_results.h"

#include "base/test/async_results_test_values_win.h"
#include "base/test/bind.h"
#include "base/test/fake_iasync_operation_win.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;

namespace base {
namespace win {

template <typename T>
class PostAsyncResultsTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(PostAsyncResultsTest);

TYPED_TEST_P(PostAsyncResultsTest, GetAsyncResultsT_Success) {
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));

  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op->get_Status(&async_status));

  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  ASSERT_HRESULT_SUCCEEDED(internal::GetAsyncResultsT(
      async_op.Get(), async_status, &value_received));
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest, GetAsyncResultsT_Failure) {
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  HRESULT test_error = (HRESULT)0x87654321L;
  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(test_error));

  AsyncStatus async_status;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op->get_Status(&async_status));

  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetTestValue_AsyncResultsT();
  ASSERT_EQ(
      internal::GetAsyncResultsT(async_op.Get(), async_status, &value_received),
      test_error);
  ASSERT_EQ(templated_values.GetDefaultValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncOperationCompletedHandler_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  IAsyncOperation<TypeParam>* async_op_received;
  AsyncStatus async_status_received = AsyncStatus::Started;
  internal::IAsyncOperationCompletedHandlerT<TypeParam> completed_handler =
      base::BindLambdaForTesting(
          [&](IAsyncOperation<TypeParam>* async_operation,
              AsyncStatus async_status) {
            async_op_received = async_operation;
            async_status_received = async_status;
            std::move(quit_closure).Run();
          });
  ASSERT_HRESULT_SUCCEEDED(internal::PostAsyncOperationCompletedHandler(
      async_op.Get(), std::move(completed_handler)));

  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(async_op.Get(), async_op_received);
  ASSERT_EQ(AsyncStatus::Completed, async_status_received);
}

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncOperationCompletedHandler_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  IAsyncOperation<TypeParam>* async_op_received;
  AsyncStatus async_status_received = AsyncStatus::Started;
  internal::IAsyncOperationCompletedHandlerT<TypeParam> completed_handler =
      base::BindLambdaForTesting(
          [&](IAsyncOperation<TypeParam>* async_operation,
              AsyncStatus async_status) {
            async_op_received = async_operation;
            async_status_received = async_status;
            std::move(quit_closure).Run();
          });
  ASSERT_HRESULT_SUCCEEDED(internal::PostAsyncOperationCompletedHandler(
      async_op.Get(), std::move(completed_handler)));

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(async_op.Get(), async_op_received);
  ASSERT_EQ(AsyncStatus::Error, async_status_received);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_OnlySuccessHandler_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  ASSERT_HRESULT_SUCCEEDED(PostAsyncHandlers(
      async_op.Get(), base::BindLambdaForTesting(
                          [&](internal::AsyncResultsT<TypeParam> result) {
                            value_received = result;
                            std::move(quit_closure).Run();
                          })));

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_OnlySuccessHandler_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  bool success_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(PostAsyncHandlers(
      async_op.Get(), base::BindLambdaForTesting(
                          [&](internal::AsyncResultsT<TypeParam> result) {
                            success_handler_called = true;
                          })));

  HRESULT test_error = (HRESULT)0x87654321L;
  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(test_error));
  run_loop.RunUntilIdle();
  ASSERT_FALSE(success_handler_called);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_NoArgsFailureHandler_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  bool failure_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(
      PostAsyncHandlers(async_op.Get(),
                        base::BindLambdaForTesting(
                            [&](internal::AsyncResultsT<TypeParam> result) {
                              value_received = result;
                              std::move(quit_closure).Run();
                            }),
                        base::BindLambdaForTesting([&] {
                          failure_handler_called = true;
                          std::move(quit_closure).Run();
                        })));

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
  ASSERT_FALSE(failure_handler_called);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_NoArgsFailureHandler_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  bool failure_handler_called = false;
  bool success_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(
      PostAsyncHandlers(async_op.Get(),
                        base::BindLambdaForTesting(
                            [&](internal::AsyncResultsT<TypeParam> result) {
                              success_handler_called = true;
                              std::move(quit_closure).Run();
                            }),
                        base::BindLambdaForTesting([&] {
                          failure_handler_called = true;
                          std::move(quit_closure).Run();
                        })));

  HRESULT test_error = (HRESULT)0x87654321L;
  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(test_error));
  run_loop.Run();
  ASSERT_FALSE(success_handler_called);
  ASSERT_TRUE(failure_handler_called);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_HRESULTFailureHandler_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  bool failure_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(
      PostAsyncHandlers(async_op.Get(),
                        base::BindLambdaForTesting(
                            [&](internal::AsyncResultsT<TypeParam> result) {
                              value_received = result;
                              std::move(quit_closure).Run();
                            }),
                        base::BindLambdaForTesting([&](HRESULT hr) {
                          failure_handler_called = true;
                          std::move(quit_closure).Run();
                        })));

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
  ASSERT_FALSE(failure_handler_called);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_HRESULTFailureHandler_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  bool success_handler_called = false;
  HRESULT hr_received = S_OK;
  ASSERT_HRESULT_SUCCEEDED(
      PostAsyncHandlers(async_op.Get(),
                        base::BindLambdaForTesting(
                            [&](internal::AsyncResultsT<TypeParam> result) {
                              success_handler_called = true;
                              std::move(quit_closure).Run();
                            }),
                        base::BindLambdaForTesting([&](HRESULT hr) {
                          hr_received = hr;
                          std::move(quit_closure).Run();
                        })));

  HRESULT test_error = (HRESULT)0x87654321L;
  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(test_error));
  run_loop.Run();
  ASSERT_FALSE(success_handler_called);
  ASSERT_EQ(test_error, hr_received);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_HRESULTAndResultFailureHandler_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  bool failure_handler_called = false;
  ASSERT_HRESULT_SUCCEEDED(PostAsyncHandlers(
      async_op.Get(),
      base::BindLambdaForTesting(
          [&](internal::AsyncResultsT<TypeParam> result) {
            value_received = result;
            std::move(quit_closure).Run();
          }),
      base::BindLambdaForTesting(
          [&](HRESULT hr, internal::AsyncResultsT<TypeParam> result) {
            failure_handler_called = true;
            std::move(quit_closure).Run();
          })));

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
  ASSERT_FALSE(failure_handler_called);
}

TYPED_TEST_P(PostAsyncResultsTest,
             PostAsyncHandlers_HRESULTAndResultFailureHandler_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  bool success_handler_called = false;
  HRESULT hr_received = S_OK;
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  ASSERT_HRESULT_SUCCEEDED(PostAsyncHandlers(
      async_op.Get(),
      base::BindLambdaForTesting(
          [&](internal::AsyncResultsT<TypeParam> result) {
            success_handler_called = true;
            std::move(quit_closure).Run();
          }),
      base::BindLambdaForTesting(
          [&](HRESULT hr, internal::AsyncResultsT<TypeParam> result) {
            hr_received = hr;
            value_received = result;
            std::move(quit_closure).Run();
          })));

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithErrorResult(
      templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_FALSE(success_handler_called);
  ASSERT_HRESULT_SUCCEEDED(hr_received);
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncResults_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  ASSERT_HRESULT_SUCCEEDED(PostAsyncResults(
      async_op, base::BindLambdaForTesting(
                    [&](internal::AsyncResultsT<TypeParam> result) {
                      value_received = result;
                      std::move(quit_closure).Run();
                    })));

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncResults_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_HRESULT_SUCCEEDED(fake_iasync_op.As(&async_op));

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetTestValue_AsyncResultsT();
  ASSERT_HRESULT_SUCCEEDED(PostAsyncResults(
      async_op, base::BindLambdaForTesting(
                    [&](internal::AsyncResultsT<TypeParam> result) {
                      value_received = result;
                      std::move(quit_closure).Run();
                    })));

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetDefaultValue_AsyncResultsT(), value_received);
}

REGISTER_TYPED_TEST_SUITE_P(
    PostAsyncResultsTest,
    GetAsyncResultsT_Success,
    GetAsyncResultsT_Failure,
    PostAsyncOperationCompletedHandler_Success,
    PostAsyncOperationCompletedHandler_Failure,
    PostAsyncHandlers_OnlySuccessHandler_Success,
    PostAsyncHandlers_OnlySuccessHandler_Failure,
    PostAsyncHandlers_NoArgsFailureHandler_Success,
    PostAsyncHandlers_NoArgsFailureHandler_Failure,
    PostAsyncHandlers_HRESULTFailureHandler_Success,
    PostAsyncHandlers_HRESULTFailureHandler_Failure,
    PostAsyncHandlers_HRESULTAndResultFailureHandler_Success,
    PostAsyncHandlers_HRESULTAndResultFailureHandler_Failure,
    PostAsyncResults_Success,
    PostAsyncResults_Failure);

INSTANTIATE_TYPED_TEST_SUITE_P(Win,
                               PostAsyncResultsTest,
                               base::test::AsyncResultsTestValuesTypes);

}  // namespace win
}  // namespace base
