// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/async_operation.h"

#include <utility>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WRL = Microsoft::WRL;

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;

// In order to exercise the interface logic of AsyncOperation we define an empty
// dummy interface, its implementation, and the necessary boilerplate to hook it
// up with IAsyncOperation and IAsyncOperationCompletedHandler.
namespace {

// Chosen by fair `uuidgen` invocation. Also applies to the UUIDs below.
MIDL_INTERFACE("756358C7-8083-4D78-9D27-9278B76096d4")
IFooBar : public IInspectable{};

class FooBar
    : public WRL::RuntimeClass<
          WRL::RuntimeClassFlags<WRL::WinRt | WRL::InhibitRoOriginateError>,
          IFooBar> {};

}  // namespace

namespace ABI {
namespace Windows {
namespace Foundation {

// Provide the required template specializations to register
// IAsyncOperation<Foobar*> as an AggregateType. This is similar to how it is
// done for UWP classes.
template <>
struct DECLSPEC_UUID("124858e4-f97e-409c-86ae-418c4781144c")
    IAsyncOperation<FooBar*>
    : IAsyncOperation_impl<Internal::AggregateType<FooBar*, IFooBar*>> {
  static const wchar_t* z_get_rc_name_impl() {
    return L"Windows.Foundation.IAsyncOperation<FooBar>";
  }
};

template <>
struct DECLSPEC_UUID("9e49373c-200c-4715-abd7-4214ba669c81")
    IAsyncOperationCompletedHandler<FooBar*>
    : IAsyncOperationCompletedHandler_impl<
          Internal::AggregateType<FooBar*, IFooBar*>> {
  static const wchar_t* z_get_rc_name_impl() {
    return L"Windows.Foundation.AsyncOperationCompletedHandler<FooBar>";
  }
};

#ifdef NTDDI_WIN10_VB  // Windows 10.0.19041
// Specialization templates that used to be in windows.foundation.h, removed in
// the 10.0.19041.0 SDK, so placed here instead.
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

// Utility method to add a completion callback to |async_op|. |*called_cb| will
// be set to true once the callback is invoked.
template <typename T>
void PutCallback(AsyncOperation<T>* async_op, bool* called_cb) {
  async_op->put_Completed(
      WRL::Callback<IAsyncOperationCompletedHandler<T>>(
          [=](IAsyncOperation<T>* iasync_op, AsyncStatus status) {
            EXPECT_EQ(async_op, iasync_op);
            *called_cb = true;
            return S_OK;
          })
          .Get());
}

}  // namespace

TEST(AsyncOperationTest, TestInt) {
  bool called_cb = false;

  auto int_op = WRL::Make<AsyncOperation<int>>();
  PutCallback(int_op.Get(), &called_cb);

  int results;
  EXPECT_TRUE(FAILED(int_op->GetResults(&results)));
  EXPECT_FALSE(called_cb);
  int_op->callback().Run(123);

  EXPECT_TRUE(called_cb);
  EXPECT_TRUE(SUCCEEDED(int_op->GetResults(&results)));
  EXPECT_EQ(123, results);

  // GetResults should be idempotent.
  EXPECT_TRUE(SUCCEEDED(int_op->GetResults(&results)));
  EXPECT_EQ(123, results);
}

TEST(AsyncOperationTest, TestBool) {
  bool called_cb = false;

  auto bool_op = WRL::Make<AsyncOperation<bool>>();
  PutCallback(bool_op.Get(), &called_cb);

  // AsyncOperation<bool> is an aggregate of bool and boolean, and requires a
  // pointer to the latter to get the results.
  boolean results;
  EXPECT_TRUE(FAILED(bool_op->GetResults(&results)));
  EXPECT_FALSE(called_cb);
  bool_op->callback().Run(true);

  EXPECT_TRUE(called_cb);
  EXPECT_TRUE(SUCCEEDED(bool_op->GetResults(&results)));
  EXPECT_TRUE(results);
}

TEST(AsyncOperationTest, TestInterface) {
  bool called_cb = false;

  auto foobar_op = WRL::Make<AsyncOperation<FooBar*>>();
  PutCallback(foobar_op.Get(), &called_cb);

  // AsyncOperation<FooBar*> is an aggregate of FooBar* and IFooBar*.
  WRL::ComPtr<IFooBar> results;
  EXPECT_TRUE(FAILED(foobar_op->GetResults(&results)));
  EXPECT_FALSE(called_cb);

  auto foobar = WRL::Make<FooBar>();
  IFooBar* foobar_ptr = foobar.Get();
  foobar_op->callback().Run(std::move(foobar));

  EXPECT_TRUE(called_cb);
  EXPECT_TRUE(SUCCEEDED(foobar_op->GetResults(&results)));
  EXPECT_EQ(foobar_ptr, results.Get());
}

TEST(AsyncOperationTest, TestIdempotence) {
  bool called_cb = false;

  auto int_op = WRL::Make<AsyncOperation<int>>();
  PutCallback(int_op.Get(), &called_cb);

  int results;
  EXPECT_TRUE(FAILED(int_op->GetResults(&results)));
  EXPECT_FALSE(called_cb);
  // Calling GetResults twice shouldn't change the result.
  EXPECT_TRUE(FAILED(int_op->GetResults(&results)));
  EXPECT_FALSE(called_cb);

  int_op->callback().Run(42);

  EXPECT_TRUE(called_cb);
  EXPECT_TRUE(SUCCEEDED(int_op->GetResults(&results)));
  EXPECT_EQ(42, results);
  // Calling GetResults twice shouldn't change the result.
  EXPECT_TRUE(SUCCEEDED(int_op->GetResults(&results)));
  EXPECT_EQ(42, results);
}

TEST(AsyncOperationTest, DoubleCallbackFails) {
  auto int_op = WRL::Make<AsyncOperation<int>>();
  auto cb = int_op->callback();

  // Obtaining another callback should result in a DCHECK failure.
  EXPECT_DCHECK_DEATH(int_op->callback());
}

}  // namespace win
}  // namespace base
