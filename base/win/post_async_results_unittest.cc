// Copyright 2020 The Chromium Authors. All rights reserved.
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

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncResults_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetDefaultValue_AsyncResultsT();
  ASSERT_EQ(PostAsyncResults(
                async_op, base::BindLambdaForTesting(
                              [&](internal::AsyncResultsT<TypeParam> result) {
                                value_received = result;
                                std::move(quit_closure).Run();
                              })),
            S_OK);

  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(templated_values.GetTestValue_T()));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetTestValue_AsyncResultsT(), value_received);
}

TYPED_TEST_P(PostAsyncResultsTest, PostAsyncResults_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<TypeParam>>();
  ComPtr<IAsyncOperation<TypeParam>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  base::test::AsyncResultsTestValues<TypeParam> templated_values;
  auto value_received = templated_values.GetTestValue_AsyncResultsT();
  ASSERT_EQ(PostAsyncResults(
                async_op, base::BindLambdaForTesting(
                              [&](internal::AsyncResultsT<TypeParam> result) {
                                value_received = result;
                                std::move(quit_closure).Run();
                              })),
            S_OK);

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(templated_values.GetDefaultValue_AsyncResultsT(), value_received);
}

REGISTER_TYPED_TEST_SUITE_P(PostAsyncResultsTest,
                            PostAsyncResults_Success,
                            PostAsyncResults_Failure);

INSTANTIATE_TYPED_TEST_SUITE_P(Win,
                               PostAsyncResultsTest,
                               base::test::AsyncResultsTestValuesTypes);

}  // namespace win
}  // namespace base
