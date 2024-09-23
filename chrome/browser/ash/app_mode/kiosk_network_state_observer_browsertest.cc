// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_network_state_observer.h"

#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class KioskNetworkStateObserverTest : public WebKioskBaseTest {
 public:
  KioskNetworkStateObserverTest() = default;

  KioskNetworkStateObserverTest(const KioskNetworkStateObserverTest&) = delete;
  KioskNetworkStateObserverTest& operator=(
      const KioskNetworkStateObserverTest&) = delete;

  void UpdateActiveWiFiCredentialsScopeChangePolicy(bool enable) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled, enable);
  }

  KioskNetworkStateObserver& network_state_observer() const {
    return kiosk_system_session()->network_state_observer_for_testing();
  }
};

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, DefaultDisabled) {
  InitializeRegularOnlineKiosk();
  EXPECT_FALSE(network_state_observer().IsPolicyEnabled());
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, NoActiveWiFi) {
  InitializeRegularOnlineKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(network_state_observer().IsPolicyEnabled());
}

// TODO(b/365528789): add more tests.

}  // namespace ash
