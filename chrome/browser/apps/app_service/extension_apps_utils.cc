// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps_utils.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool g_enable_hosted_apps_in_lacros_for_testing = false;
#endif
}  // namespace

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ShouldHostedAppsRunInLacros() {
  if (g_enable_hosted_apps_in_lacros_for_testing) {
    return true;
  }

  return chromeos::BrowserParamsProxy::Get()->PublishHostedApps();
}

void EnableHostedAppsInLacrosForTesting() {
  g_enable_hosted_apps_in_lacros_for_testing = true;
}
#endif  // IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string GetEscapedAppId(const std::string& app_id, AppType app_type) {
  // Normally app ids would only contain alphanumerics, but standalone
  // browser extension app uses '#' as a delimiter, which needs to be escaped.
  return app_type == apps::AppType::kStandaloneBrowserChromeApp
             ? base::EscapeAllExceptUnreserved(app_id)
             : app_id;
}
#endif

}  // namespace apps
