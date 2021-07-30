// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// Implementers of this class are expected to be a minimal layer of glue code
// that performs API call on the Java side to Google Mobile for each operation.
// Any logic beyond data conversion should either live in
// `PasswordStoreAndroidBackend` or a component that is used by the java-side of
// this bridge.
class PasswordStoreAndroidBackendBridge {
 public:
  using TaskId = base::StrongAlias<struct TaskIdTag, int>;

  // Each bridge is created with a consumer that will be called when a task is
  // completed. In order to identify which request the response belongs to, the
  // bridge needs to pass a `TaskId` back that was returned by the initial
  // request and is unique per bridge.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called with the `task_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendBridge`, and with the
    // requested `passwords`.
    // Used in response to `GetAllLogins`.
    virtual void OnCompleteWithLogins(TaskId task_id,
                                      std::vector<PasswordForm> passwords) = 0;
  };

  virtual ~PasswordStoreAndroidBackendBridge() = default;

  // Sets the `consumer` that is notified on task completion.
  virtual void SetConsumer(Consumer* consumer) = 0;

  // Triggers an asynchronous request to retrieve all stored passwords. The
  // registered `Consumer` is notified with `OnCompleteWithLogins` when the
  // task with the returned TaskId succeeds.
  virtual TaskId GetAllLogins() WARN_UNUSED_RESULT = 0;

  // Factory function for creating the bridge. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  static std::unique_ptr<PasswordStoreAndroidBackendBridge> Create();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
