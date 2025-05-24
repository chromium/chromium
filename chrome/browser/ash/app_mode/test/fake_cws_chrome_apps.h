// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_FAKE_CWS_CHROME_APPS_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_FAKE_CWS_CHROME_APPS_H_

#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"

// This file has helper methods to retrieve a `CwsChromeAppOption` for the
// `FakeCWS` apps implemented in
//   //chrome/test/data/chromeos/app_mode/apps_and_extensions
//
// Tests can use the `CwsChromeAppOption` to configure `KioskMixin`.

namespace ash::kiosk::test {

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     offline_enabled_kiosk_app/v1
[[nodiscard]] KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV1();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     offline_enabled_kiosk_app/v2
[[nodiscard]] KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     offline_enabled_kiosk_app/v2_permission_change
[[nodiscard]] KioskMixin::CwsChromeAppOption
OfflineEnabledChromeAppV2WithPermissionChange();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     offline_enabled_kiosk_app/v2_required_platform_version_added
//
// This version sets `required_platform_version` to 1234.0.0 in the manifest.
[[nodiscard]] KioskMixin::CwsChromeAppOption
OfflineEnabledChromeAppV2RequiresVersion1234();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     local_fs/v1_write_data
//
// Version 1 of this app writes some data to the file system, while version 2
// reads from it.
[[nodiscard]] KioskMixin::CwsChromeAppOption LocalFsChromeAppV1();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//     local_fs/v2_read_and_verify_data
//
// Version 1 of this app writes some data to the file system, while version 2
// reads from it.
[[nodiscard]] KioskMixin::CwsChromeAppOption LocalFsChromeAppV2();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//   minimum_chrome_version/v1
//
// Version 1 of this app has no `minimum_chrome_version` requirements.
[[nodiscard]] KioskMixin::CwsChromeAppOption MinimumChromeVersionAppV1();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//   minimum_chrome_version/v2
//
// Version 2 of this app sets `minimum_chrome_version` to M100.
[[nodiscard]] KioskMixin::CwsChromeAppOption
MinimumChromeVersionAppV2WithMinimumVersion100();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//   minimum_chrome_version/v3
//
// Version 3 of this app sets `minimum_chrome_version` to M999.
[[nodiscard]] KioskMixin::CwsChromeAppOption
MinimumChromeVersionAppV3WithMinimumVersion999();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//   app_with_secondary_app/v1
//
// This app sets the "minimum_chrome_version" app as a secondary app in the
// manifest. See `MinimumChromeVersionAppV1`.
[[nodiscard]] KioskMixin::CwsChromeAppOption AppWithSecondaryAppV1();

// Corresponds to the Chrome app under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/
//   enterprise_kiosk_test_app/src
//
// This app declares the "identity" permission in the manifest.
[[nodiscard]] KioskMixin::CwsChromeAppOption EnterpriseKioskAppV1();

}  // namespace ash::kiosk::test

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_FAKE_CWS_CHROME_APPS_H_
