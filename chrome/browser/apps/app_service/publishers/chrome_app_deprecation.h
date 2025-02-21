// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_

#include <string_view>

class Profile;

// This namespace collects all the methods that control when to enforce the
// multiple stages of the Chrome Apps deprecation for user installed apps, Kiosk
// sessions and managed users.
namespace apps::chrome_app_deprecation {

enum class DeprecationStatus { kLaunchAllowed, kLaunchBlocked };

// Executes all the checks and tasks related to the Chrome Apps deprecation.
DeprecationStatus HandleDeprecation(std::string_view app_id, Profile* profile);

void AddAppToAllowlistForTesting(std::string_view app_id);

}  // namespace apps::chrome_app_deprecation

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_CHROME_APP_DEPRECATION_H_
