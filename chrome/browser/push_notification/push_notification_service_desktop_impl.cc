// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace push_notification {

PushNotificationServiceDesktopImpl::PushNotificationServiceDesktopImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}
PushNotificationServiceDesktopImpl::~PushNotificationServiceDesktopImpl() =
    default;

void PushNotificationServiceDesktopImpl::Shutdown() {
  // TODO(b/306398998): Once fetching GCM token is implemented, reset the token
  // here.
  client_manager_.reset();
}

}  // namespace push_notification
