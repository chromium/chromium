// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_threadsafe.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

constinit thread_local const ObserverListThreadSafeBase::NotificationDataBase*
    current_notification = nullptr;

// static
const ObserverListThreadSafeBase::NotificationDataBase*&
ObserverListThreadSafeBase::GetCurrentNotification() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(
      &current_notification,
      sizeof(const ObserverListThreadSafeBase::NotificationDataBase*));

  return current_notification;
}

}  // namespace internal
}  // namespace base
