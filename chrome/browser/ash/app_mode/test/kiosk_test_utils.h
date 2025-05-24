// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/auto_reset.h"
#include "base/functional/function_ref.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"

namespace ash::kiosk::test {

// Returns the auto launch Kiosk app configured in the system. Checks if there
// is none.
[[nodiscard]] KioskApp AutoLaunchKioskApp();

// Returns the Kiosk Chrome app configured in the system. Checks if there is not
// exactly one Chrome app.
[[nodiscard]] KioskApp TheKioskChromeApp();

// Returns the Kiosk web app configured in the system. Checks if there is not
// exactly one web app.
[[nodiscard]] KioskApp TheKioskWebApp();

// Returns the Kiosk app configured in the system. Checks if there is not
// exactly one app.
[[nodiscard]] KioskApp TheKioskApp();

// Returns the `KioskApp` known by the system given its corresponding
// `account_id` configured in policies.
[[nodiscard]] std::optional<KioskApp> GetAppByAccountId(
    std::string_view account_id);

// Launches the given `app`, simulating a manual launch from the login screen.
// Returns true if the launch started.
[[nodiscard]] bool LaunchAppManually(const KioskApp& app);

// Launches the app identified by the given `account_id`, simulating a manual
// launch from the login screen. Returns true if the launch started.
//
// `account_id` must have been previously configured in policies.
[[nodiscard]] bool LaunchAppManually(std::string_view account_id);

// Waits until a Kiosk session launched. Returns true if the launch was
// successful.
[[nodiscard]] bool WaitKioskLaunched();

// Tells `KioskLaunchController` to block kiosk launch until the `AutoReset` is
// destroyed.
//
// This returns an optional as a convenience, so tests can call `.reset()` to
// allow the launch to proceed. The optional is never `nullopt`.
[[nodiscard]] std::optional<base::AutoReset<bool>> BlockKioskLaunch();

// Returns true if the Chrome `app` is installed in the given `profile`.
[[nodiscard]] bool IsChromeAppInstalled(Profile& profile, const KioskApp& app);
[[nodiscard]] bool IsChromeAppInstalled(Profile& profile,
                                        std::string_view app_id);

// Returns true if the Web `app` is installed in the given `profile`.
[[nodiscard]] bool IsWebAppInstalled(Profile& profile, const KioskApp& app);
[[nodiscard]] bool IsWebAppInstalled(Profile& profile, const GURL& install_url);

// Returns true if the Kiosk `app` is installed in the given `profile`.
[[nodiscard]] bool IsAppInstalled(Profile& profile, const KioskApp& app);

// Returns the version string of the Chrome `app` installed in the given
// `profile`. CHECKs when `app` is not a Chrome app or not installed.
[[nodiscard]] std::string InstalledChromeAppVersion(Profile& profile,
                                                    const KioskApp& app);
[[nodiscard]] std::string InstalledChromeAppVersion(Profile& profile,
                                                    std::string_view app_id);

// Returns the version string of the Chrome `app` in the device local account
// external cache. CHECKs when the app is not cached.
[[nodiscard]] std::string CachedChromeAppVersion(const KioskApp& app);
[[nodiscard]] std::string CachedChromeAppVersion(std::string_view app_id);

// Returns the current profile. Makes sense to be called after Kiosk launch.
[[nodiscard]] Profile& CurrentProfile();

// Waits for the Kiosk splash screen to appear.
void WaitSplashScreen();

// Waits for the Kiosk network dialog in the splash screen to appear.
void WaitNetworkScreen();

// Presses the accelerator to display the network dialog in the splash screen.
// Returns true if the accelerator was processed.
[[nodiscard]] bool PressNetworkAccelerator();

// Presses the accelerator to cancel (bailout) Kiosk launch. Returns true if the
// accelerator was processed.
[[nodiscard]] bool PressBailoutAccelerator();

// Opens accessibility settings, waits to make sure the `KioskSystemSession`
// does not close it, and returns the corresponding `Browser`.
//
// Checks if `KioskSystemSession` closes the browser, or if it is null.
Browser* OpenA11ySettings(Profile& profile);

// Waits for the next new browser window to be created and returns true if
// `KioskSystemSession` decides to close it.
[[nodiscard]] bool DidKioskCloseNewWindow();

// Closes the window of the given `app`.
void CloseAppWindow(const KioskApp& app);

// Caches the resulting device local account policy built by `setup` for the
// given `account_id`.
//
// Must be called early, before Chrome loads policies from session manager
// during startup.
void CachePolicy(const std::string& account_id,
                 base::FunctionRef<void(policy::UserPolicyBuilder&)> setup);

// The account ID as configured in policies is different from the `AccountId`
// that identify users in Chrome. This function converts the policy `account_id`
// of a Kiosk app of the given `type` to a Chrome `AccountId`.
AccountId CreateDeviceLocalAccountId(std::string_view account_id,
                                     policy::DeviceLocalAccountType type);

// Opens a new browser window including navigation to a test url.
Browser& CreateRegularBrowser(Profile& profile);

// opens a new popup browser window belonging to the provided `app_name`.
Browser& CreatePopupBrowser(Profile& profile, const std::string& app_name);

}  // namespace ash::kiosk::test

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_
