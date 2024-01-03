// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_client_manager_desktop_impl.h"

namespace push_notification {
PushNotificationClientManagerDesktopImpl::
    PushNotificationClientManagerDesktopImpl() = default;
PushNotificationClientManagerDesktopImpl::
    ~PushNotificationClientManagerDesktopImpl() = default;

void PushNotificationClientManagerDesktopImpl::
    NotifyPushNotificationClientOfMessage(PushNotificationMessage message) {
  // TODO(b/287340843): Parse the message and
  // extract the `PushNotificationClientId` to pass the message to the correct
  // `PushNotificationClient`.
}

}  // namespace push_notification
