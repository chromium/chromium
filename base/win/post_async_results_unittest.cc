// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/post_async_results.h"

#include "base/test/bind_test_util.h"
#include "base/test/fake_iasync_operation_win.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperation_impl;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler_impl;
using Microsoft::WRL::ComPtr;

namespace {
class TestClassImplementingIUnknown
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRtClassicComMix |
              Microsoft::WRL::InhibitRoOriginateError>,
          IUnknown> {};
}  // namespace

namespace ABI {
namespace Windows {
namespace Foundation {

template <>
struct __declspec(uuid("3895C200-8F26-4F5A-B29D-2B5D72E68F99"))
    IAsyncOperation<IUnknown*> : IAsyncOperation_impl<IUnknown*> {};

template <>
struct __declspec(uuid("CD99A253-6473-4810-AF0D-763DAB79AC42"))
    IAsyncOperationCompletedHandler<IUnknown*>
    : IAsyncOperationCompletedHandler_impl<IUnknown*> {};

template <>
struct __declspec(uuid("CB52D855-8121-4AC8-A164-084A27FB377E"))
    IAsyncOperation<int*> : IAsyncOperation_impl<int*> {};

template <>
struct __declspec(uuid("EA868415-A724-40BC-950A-C7DB6B1723C6"))
    IAsyncOperationCompletedHandler<int*>
    : IAsyncOperationCompletedHandler_impl<int*> {};

// These specialization templates were included in windows.foundation.h, but
// removed in 10.0.19041.0 SDK, so are included here conditionally
#ifdef NTDDI_WIN10_VB  // Windows 10.0.19041
template <>
struct __declspec(uuid("968b9665-06ed-5774-8f53-8edeabd5f7b5"))
    IAsyncOperation<int> : IAsyncOperation_impl<int> {};

template <>
struct __declspec(uuid("d60cae9d-88cb-59f1-8576-3fba44796be8"))
    IAsyncOperationCompletedHandler<int>
    : IAsyncOperationCompletedHandler_impl<int> {};
#endif

}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace base {
namespace win {
namespace {

template <typename T>
class TemplatedValues {};

template <>
class TemplatedValues<int> {
 public:
  int GetDefaultValue_T() { return 0; }
  int GetDefaultValue_AsyncResultsT() { return 0; }

  int GetTestValue_T() { return 4; }
  int GetTestValue_AsyncResultsT() { return 4; }
};

template <>
class TemplatedValues<int*> {
 public:
  int* GetDefaultValue_T() { return nullptr; }
  int* GetDefaultValue_AsyncResultsT() { return nullptr; }

  int* GetTestValue_T() { return &test_value_; }
  int* GetTestValue_AsyncResultsT() { return &test_value_; }

 private:
  int test_value_ = 4;
};

template <>
class TemplatedValues<IUnknown*> {
 public:
  TemplatedValues() {
    auto class_instance = Microsoft::WRL::Make<TestClassImplementingIUnknown>();
    class_instance.As(&test_value_);
  }

  IUnknown* GetDefaultValue_T() { return nullptr; }
  ComPtr<IUnknown> GetDefaultValue_AsyncResultsT() {
    ComPtr<IUnknown> value{};
    return value;
  }

  IUnknown* GetTestValue_T() { return test_value_.Get(); }
  ComPtr<IUnknown> GetTestValue_AsyncResultsT() { return test_value_; }

 private:
  ComPtr<IUnknown> test_value_;
};
}  // namespace

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
  TemplatedValues<TypeParam> templated_values;
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
  TemplatedValues<TypeParam> templated_values;
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

using ResultTypes = ::testing::Types<int, int*, IUnknown*>;
INSTANTIATE_TYPED_TEST_SUITE_P(Win, PostAsyncResultsTest, ResultTypes);

}  // namespace win
}  // namespace base
