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
struct __declspec(uuid("968b9665-06ed-5744-8f53-8edeabd5f7b5"))
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

TEST(PostAsyncResultsTest, ValueType_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<int>>();
  ComPtr<IAsyncOperation<int>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  int value_received = 1;
  ASSERT_EQ(
      PostAsyncResults(async_op, base::BindLambdaForTesting([&](int result) {
                         value_received = result;
                         std::move(quit_closure).Run();
                       })),
      S_OK);

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithResults(7));
  run_loop.Run();
  ASSERT_EQ(7, value_received);
}

TEST(PostAsyncResultsTest, ValueType_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<int>>();
  ComPtr<IAsyncOperation<int>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  int value_received = 1;
  ASSERT_EQ(
      PostAsyncResults(async_op, base::BindLambdaForTesting([&](int result) {
                         value_received = result;
                         std::move(quit_closure).Run();
                       })),
      S_OK);

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(value_received, 0);
}

TEST(PostAsyncResultsTest, PointerType_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<int*>>();
  ComPtr<IAsyncOperation<int*>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  int* value_received = nullptr;
  ASSERT_EQ(
      PostAsyncResults(async_op, base::BindLambdaForTesting([&](int* result) {
                         value_received = result;
                         std::move(quit_closure).Run();
                       })),
      S_OK);

  int test_value = 4;
  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithResults(&test_value));
  run_loop.Run();
  ASSERT_EQ(&test_value, value_received);
}

TEST(PostAsyncResultsTest, PointerType_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<int*>>();
  ComPtr<IAsyncOperation<int*>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  int test_value = 2;
  int* value_received = &test_value;
  ASSERT_EQ(
      PostAsyncResults(async_op, base::BindLambdaForTesting([&](int* result) {
                         value_received = result;
                         std::move(quit_closure).Run();
                       })),
      S_OK);

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(nullptr, value_received);
}

TEST(PostAsyncResultsTest, IUnknownType_Success) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<IUnknown*>>();
  ComPtr<IAsyncOperation<IUnknown*>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  ComPtr<IUnknown> value_received = nullptr;
  ASSERT_EQ(PostAsyncResults(async_op, base::BindLambdaForTesting(
                                           [&](ComPtr<IUnknown> result) {
                                             value_received = result;
                                             std::move(quit_closure).Run();
                                           })),
            S_OK);

  auto test_value = Microsoft::WRL::Make<TestClassImplementingIUnknown>();
  ComPtr<IUnknown> value_to_send;
  ASSERT_EQ(test_value.As(&value_to_send), S_OK);
  ASSERT_NO_FATAL_FAILURE(
      fake_iasync_op->CompleteWithResults(value_to_send.Get()));
  run_loop.Run();
  ASSERT_EQ(value_to_send.Get(), value_received.Get());
}

TEST(PostAsyncResultsTest, IUnknownType_Failure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto fake_iasync_op = Microsoft::WRL::Make<FakeIAsyncOperation<IUnknown*>>();
  ComPtr<IAsyncOperation<IUnknown*>> async_op;
  ASSERT_EQ(fake_iasync_op.As(&async_op), S_OK);

  RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auto test_value = Microsoft::WRL::Make<TestClassImplementingIUnknown>();
  ComPtr<IUnknown> value_received;
  ASSERT_EQ(test_value.As(&value_received), S_OK);
  ASSERT_EQ(PostAsyncResults(async_op, base::BindLambdaForTesting(
                                           [&](ComPtr<IUnknown> result) {
                                             value_received = result;
                                             std::move(quit_closure).Run();
                                           })),
            S_OK);

  ASSERT_NO_FATAL_FAILURE(fake_iasync_op->CompleteWithError(E_FAIL));
  run_loop.Run();
  ASSERT_EQ(nullptr, value_received.Get());
}

}  // namespace win
}  // namespace base
