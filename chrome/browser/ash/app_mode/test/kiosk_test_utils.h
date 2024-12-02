// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_

#include <string_view>

#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/profiles/profile.h"

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

// Returns true if the Chrome `app` is installed in the given `profile`.
[[nodiscard]] bool IsChromeAppInstalled(Profile& profile, const KioskApp& app);
[[nodiscard]] bool IsChromeAppInstalled(Profile& profile,
                                        std::string_view app_id);

// Returns true if the Web `app` is installed in the given `profile`.
[[nodiscard]] bool IsWebAppInstalled(Profile& profile, const KioskApp& app);
[[nodiscard]] bool IsWebAppInstalled(Profile& profile, const GURL& install_url);

// Returns true if the Kiosk `app` is installed in the given `profile`.
[[nodiscard]] bool IsAppInstalled(Profile& profile, const KioskApp& app);

// Returns the current profile. Makes sense to be called after Kiosk launch.
[[nodiscard]] Profile& CurrentProfile();

// Closes the window of the given `app`.
void CloseAppWindow(const KioskApp& app);

}  // namespace ash::kiosk::test

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_TEST_UTILS_H_
