// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/wifi_toggle_notification_controller.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

class WifiToggleNotificationControllerTest : public AshTestBase {
 public:
  WifiToggleNotificationControllerTest() = default;
  ~WifiToggleNotificationControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // NOTE: This is necessary to give the TrayNetworkStateModel a chance to
    // sync its list of network devices.
    base::RunLoop().RunUntilIdle();
  }

 private:
  chromeos::network_config::CrosNetworkConfigTestHelper network_config_helper_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(WifiToggleNotificationControllerTest);
};

// Verifies that toggling Wi-Fi (usually via keyboard) shows a notification.
TEST_F(WifiToggleNotificationControllerTest, ToggleWifi) {
  // No notifications at startup.
  ASSERT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Simulate a user action to toggle Wi-Fi.
  Shell::Get()->system_tray_notifier()->NotifyRequestToggleWifi();

  // Notification was shown.
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  EXPECT_TRUE(MessageCenter::Get()->HasPopupNotifications());
  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById("wifi-toggle"));
}

}  // namespace ash
