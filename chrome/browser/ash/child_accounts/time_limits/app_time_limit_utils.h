// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_UTILS_H_

#include "components/policy/proto/device_management_backend.pb.h"

class GURL;

namespace apps {
enum class AppType;
}  // namespace apps

namespace ash {
namespace app_time {

class AppId;
enum class AppState;

enterprise_management::App::AppType AppTypeForReporting(apps::AppType type);

AppId GetChromeAppId();

bool IsWebAppOrExtension(const AppId& app_id);

// Returns true if the application shares chrome's time limit.
bool ContributesToWebTimeLimit(const AppId& app_id, AppState app_state);

// Returns whether the given |app_url| is a valid extension url.
bool IsValidExtensionUrl(const GURL& app_url);

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_UTILS_H_
