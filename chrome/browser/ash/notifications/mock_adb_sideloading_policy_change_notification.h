// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_MOCK_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_MOCK_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_

#include "chrome/browser/ash/notifications/adb_sideloading_policy_change_notification.h"

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace ash {

class MockAdbSideloadingPolicyChangeNotification
    : public AdbSideloadingPolicyChangeNotification {
 public:
  using NotificationType = AdbSideloadingPolicyChangeNotification::Type;

  // `browser_policy_connector_ash` must be non-null and must outlive `this`.
  explicit MockAdbSideloadingPolicyChangeNotification(
      const policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash);
  ~MockAdbSideloadingPolicyChangeNotification() override;

  void Show(NotificationType type) override;

  NotificationType last_shown_notification = NotificationType::kNone;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_MOCK_ADB_SIDELOADING_POLICY_CHANGE_NOTIFICATION_H_
