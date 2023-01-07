// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_tracker_internal.h"

namespace sync_file_system {
namespace drive_backend {

// A helper class to ensure one-shot callback to be called exactly once.
//
// Usage:
//   class Foo {
//    private:
//     CallbackTracker callback_tracker_;
//   };
//
//   void DoSomethingAsync(const SomeCallbackType& callback) {
//     base::OnceClosure abort_case_handler =
//         base::BindOnce(callback, ABORT_ERROR);
//
//     SomeCallbackType wrapped_callback =
//         callback_tracker_.Register(
//             abort_case_handler, callback);
//
//     // The body of the operation goes here.
//   }
class CallbackTracker {
 public:
  using AbortClosureByHelper =
      std::map<internal::AbortHelper*, base::OnceClosure>;

  CallbackTracker();

  CallbackTracker(const CallbackTracker&) = delete;
  CallbackTracker& operator=(const CallbackTracker&) = delete;

  ~CallbackTracker();

  // Returns a wrapped callback.
  // Upon AbortAll() call, CallbackTracker invokes |abort_closure| and voids all
  // wrapped callbacks returned by Register().
  // Invocation of the wrapped callback unregisters |callback| from
  // CallbackTracker.
  template <typename T>
  base::OnceCallback<T> Register(base::OnceClosure abort_closure,
                                 base::OnceCallback<T> callback) {
    internal::AbortHelper* helper = new internal::AbortHelper(this);
    helpers_[helper] = std::move(abort_closure);
    return base::BindOnce(&internal::InvokeAndInvalidateHelper<T>::Run,
                          helper->AsWeakPtr(), std::move(callback));
  }

  void AbortAll();

 private:
  friend class internal::AbortHelper;

  std::unique_ptr<internal::AbortHelper> PassAbortHelper(
      internal::AbortHelper* helper);

  AbortClosureByHelper helpers_;  // Owns AbortHelpers.
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_H_
