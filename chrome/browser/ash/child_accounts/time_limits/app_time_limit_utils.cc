// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"

#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace ash {
namespace app_time {

enterprise_management::App::AppType AppTypeForReporting(apps::AppType type) {
  switch (type) {
    case apps::AppType::kArc:
      return enterprise_management::App::ARC;
    case apps::AppType::kBuiltIn:
      return enterprise_management::App::BUILT_IN;
    case apps::AppType::kCrostini:
      return enterprise_management::App::CROSTINI;
    case apps::AppType::kChromeApp:
      return enterprise_management::App::EXTENSION;
    case apps::AppType::kPluginVm:
      return enterprise_management::App::PLUGIN_VM;
    case apps::AppType::kWeb:
      return enterprise_management::App::WEB;
    default:
      return enterprise_management::App::UNKNOWN;
  }
}

AppId GetChromeAppId() {
  return AppId(apps::AppType::kChromeApp, app_constants::kChromeAppId);
}

bool IsWebAppOrExtension(const AppId& app_id) {
  return app_id.app_type() == apps::AppType::kWeb ||
         app_id.app_type() == apps::AppType::kChromeApp;
}

// Returns true if the application shares chrome's time limit.
bool ContributesToWebTimeLimit(const AppId& app_id, AppState state) {
  if (state == AppState::kAlwaysAvailable)
    return false;

  return IsWebAppOrExtension(app_id);
}

bool IsValidExtensionUrl(const GURL& app_url) {
  return !app_url.is_empty() && !app_url.inner_url() &&
         app_url.SchemeIs(extensions::kExtensionScheme);
}

}  // namespace app_time
}  // namespace ash
