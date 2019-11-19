// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(ChromeOsAppsNavigationThrottleTest, TestGetDestinationPlatform) {
  const std::string app_launch_name = "fake_package";
  const std::string chrome_launch_name =
      arc::ArcIntentHelperBridge::kArcIntentHelperPackageName;

  // When the PickerAction is either ERROR or DIALOG_DEACTIVATED we MUST stay in
  // Chrome not taking into account the selected_app_package.
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          chrome_launch_name,
          apps::AppsNavigationThrottle::PickerAction::ERROR_BEFORE_PICKER));
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          app_launch_name,
          apps::AppsNavigationThrottle::PickerAction::ERROR_BEFORE_PICKER));
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          chrome_launch_name,
          apps::AppsNavigationThrottle::PickerAction::ERROR_AFTER_PICKER));
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          app_launch_name,
          apps::AppsNavigationThrottle::PickerAction::ERROR_AFTER_PICKER));
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          chrome_launch_name,
          apps::AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));
  EXPECT_EQ(
      apps::AppsNavigationThrottle::Platform::CHROME,
      ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
          app_launch_name,
          apps::AppsNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));

  // When the PickerAction is PWA_APP_PRESSED, always expect the platform to be
  // PWA.
  EXPECT_EQ(apps::AppsNavigationThrottle::Platform::PWA,
            ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
                chrome_launch_name,
                apps::AppsNavigationThrottle::PickerAction::PWA_APP_PRESSED));
  EXPECT_EQ(apps::AppsNavigationThrottle::Platform::PWA,
            ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
                app_launch_name,
                apps::AppsNavigationThrottle::PickerAction::PWA_APP_PRESSED));

  // Under any other PickerAction, stay in Chrome only if the package is Chrome.
  // Otherwise redirect to ARC.
  EXPECT_EQ(apps::AppsNavigationThrottle::Platform::CHROME,
            ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
                chrome_launch_name, apps::AppsNavigationThrottle::PickerAction::
                                        PREFERRED_ACTIVITY_FOUND));
  EXPECT_EQ(apps::AppsNavigationThrottle::Platform::ARC,
            ChromeOsAppsNavigationThrottle::GetDestinationPlatform(
                app_launch_name, apps::AppsNavigationThrottle::PickerAction::
                                     PREFERRED_ACTIVITY_FOUND));
}

}  // namespace chromeos
