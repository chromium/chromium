// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_threadsafe.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace internal {

ABSL_CONST_INIT thread_local const ObserverListThreadSafeBase::
    NotificationDataBase* current_notification = nullptr;

// static
const ObserverListThreadSafeBase::NotificationDataBase*&
ObserverListThreadSafeBase::GetCurrentNotification() {
  return current_notification;
}

}  // namespace internal
}  // namespace base
