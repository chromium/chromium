// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/mock_adb_sideloading_policy_change_notification.h"

namespace ash {

MockAdbSideloadingPolicyChangeNotification::
    MockAdbSideloadingPolicyChangeNotification() = default;
MockAdbSideloadingPolicyChangeNotification::
    ~MockAdbSideloadingPolicyChangeNotification() = default;

void MockAdbSideloadingPolicyChangeNotification::Show(NotificationType type) {
  last_shown_notification = type;
}

}  // namespace ash
