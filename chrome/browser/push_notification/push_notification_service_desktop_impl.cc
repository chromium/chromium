// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"
#include "base/check.h"

namespace push_notification {

PushNotificationServiceDesktopImpl::PushNotificationServiceDesktopImpl() =
    default;
PushNotificationServiceDesktopImpl::~PushNotificationServiceDesktopImpl() =
    default;

void PushNotificationServiceDesktopImpl::Shutdown() {}

}  // namespace push_notification
