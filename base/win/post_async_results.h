// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_POST_ASYNC_RESULTS_H_
#define BASE_WIN_POST_ASYNC_RESULTS_H_

#include <unknwn.h>
#include <windows.foundation.h>
#include <wrl/async.h>
#include <wrl/client.h>

#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {
namespace win {

namespace internal {

// Utility function to pretty print enum values.
constexpr const char* ToCString(AsyncStatus async_status) {
  switch (async_status) {
    case AsyncStatus::Started:
      return "AsyncStatus::Started";
    case AsyncStatus::Completed:
      return "AsyncStatus::Completed";
    case AsyncStatus::Canceled:
      return "AsyncStatus::Canceled";
    case AsyncStatus::Error:
      return "AsyncStatus::Error";
  }

  NOTREACHED();
  return "";
}

template <typename T>
using AsyncAbiT = typename ABI::Windows::Foundation::Internal::GetAbiType<
    typename ABI::Windows::Foundation::IAsyncOperation<T>::TResult_complex>::
    type;

// Compile time switch to decide what container to use for the async results for
// |T|. Depends on whether the underlying Abi type is a pointer to IUnknown or
// not. It queries the internals of Windows::Foundation to obtain this
// information.
template <typename T>
using AsyncResultsT = std::conditional_t<
    std::is_convertible<AsyncAbiT<T>, IUnknown*>::value,
    Microsoft::WRL::ComPtr<std::remove_pointer_t<AsyncAbiT<T>>>,
    AsyncAbiT<T>>;

// Obtains the results of the provided async operation.
template <typename T>
AsyncResultsT<T> GetAsyncResults(
    ABI::Windows::Foundation::IAsyncOperation<T>* async_op) {
  AsyncResultsT<T> results;
  HRESULT hr = async_op->GetResults(&results);
  if (FAILED(hr)) {
    VLOG(2) << "GetAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    return AsyncResultsT<T>{};
  }

  return results;
}

}  // namespace internal

// This method registers a completion handler for |async_op| and will post the
// results to |callback|. The |callback| will be run on the same thread that
// invoked this method. Callers need to ensure that this method is invoked in
// the correct COM apartment, i.e. the one that created |async_op|. While a WRL
// Callback can be constructed from callable types such as a lambda or
// std::function objects, it cannot be directly constructed from a
// base::OnceCallback. Thus the callback is moved into a capturing lambda, which
// then posts the callback once it is run. Posting the results to the TaskRunner
// is required, since the completion callback might be invoked on an arbitrary
// thread. Lastly, the lambda takes ownership of |async_op|, as this needs to be
// kept alive until GetAsyncResults can be invoked.
template <typename T>
HRESULT PostAsyncResults(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<T>>
        async_op,
    base::OnceCallback<void(internal::AsyncResultsT<T>)> callback) {
  auto completed_cb = base::BindOnce(
      [](Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<T>>
             async_op,
         base::OnceCallback<void(internal::AsyncResultsT<T>)> callback) {
        std::move(callback).Run(internal::GetAsyncResults(async_op.Get()));
      },
      async_op, std::move(callback));

  auto completed_lambda = [task_runner(base::ThreadTaskRunnerHandle::Get()),
                           completed_cb(std::move(completed_cb))](
                              auto&&, AsyncStatus async_status) mutable {
    if (async_status != AsyncStatus::Completed) {
      VLOG(2) << "Got unexpected AsyncStatus: "
              << internal::ToCString(async_status);
    }

    // Note: We are ignoring the passed in pointer to |async_op|, as |callback|
    // has access to the initially provided |async_op|. Since the code within
    // the lambda could be executed on any thread, it is vital that the
    // callback gets posted to the original |task_runner|, as this is
    // guaranteed to be in the correct COM apartment.
    task_runner->PostTask(FROM_HERE, std::move(completed_cb));
    return S_OK;
  };

  using CompletedHandler = Microsoft::WRL::Implements<
      Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
      ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T>,
      Microsoft::WRL::FtmBase>;

  return async_op->put_Completed(
      Microsoft::WRL::Callback<CompletedHandler>(std::move(completed_lambda))
          .Get());
}

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_POST_ASYNC_RESULTS_H_