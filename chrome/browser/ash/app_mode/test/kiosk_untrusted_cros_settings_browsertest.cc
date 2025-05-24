// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;

// Verifies Kiosk does not launch with `PERMANENTLY_UNTRUSTED` cros settings.
class KioskUntrustedCrosSettingsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskUntrustedCrosSettingsTest() = default;
  KioskUntrustedCrosSettingsTest(const KioskUntrustedCrosSettingsTest&) =
      delete;
  KioskUntrustedCrosSettingsTest& operator=(
      const KioskUntrustedCrosSettingsTest&) = delete;

  ~KioskUntrustedCrosSettingsTest() override = default;

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskUntrustedCrosSettingsTest, DoesNotLaunch) {
  ScopedCrosSettingsTestHelper settings{/*create_settings_service=*/false};
  settings.ReplaceDeviceSettingsProviderWithStub();
  settings.SetTrustedStatus(CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  // Kiosk does not launch when settings are `PERMANENTLY_UNTRUSTED`.
  EXPECT_FALSE(KioskController::Get().IsSessionStarting());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskUntrustedCrosSettingsTest,
    testing::Values(KioskMixin::Config{/*name=*/"WebApp",
                                       /*auto_launch_account_id=*/{},
                                       {KioskMixin::SimpleWebAppOption()}},
                    KioskMixin::Config{/*name=*/"ChromeApp",
                                       /*auto_launch_account_id=*/{},
                                       {KioskMixin::SimpleChromeAppOption()}}),
    KioskMixin::ConfigName);

}  // namespace ash
