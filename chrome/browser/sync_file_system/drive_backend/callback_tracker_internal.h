// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_INTERNAL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_INTERNAL_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace sync_file_system {
namespace drive_backend {

class CallbackTracker;

namespace internal {

class AbortHelper {
 public:
  explicit AbortHelper(CallbackTracker* tracker);
  ~AbortHelper();
  base::WeakPtr<AbortHelper> AsWeakPtr();

  static std::unique_ptr<AbortHelper> TakeOwnership(
      const base::WeakPtr<AbortHelper>& abort_helper);

 private:
  CallbackTracker* tracker_;  // Not owned.
  base::WeakPtrFactory<AbortHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AbortHelper);
};

template <typename>
struct InvokeAndInvalidateHelper;

template <typename... Args>
struct InvokeAndInvalidateHelper<void(Args...)> {
  static void Run(const base::WeakPtr<AbortHelper>& abort_helper,
                  const base::Callback<void(Args...)>& callback,
                  Args... args) {
    std::unique_ptr<AbortHelper> deleter =
        AbortHelper::TakeOwnership(abort_helper);
    if (deleter) {
      callback.Run(std::forward<Args>(args)...);
    }
  }
};

}  // namespace internal
}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_TRACKER_INTERNAL_H_
