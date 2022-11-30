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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Kill switch in case the feature causes problems.
BASE_FEATURE(kStopMuxingLacrosExtensionAppIds,
             "StopMuxingLacrosExtensionAppIds",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ShouldHostedAppsRunInLacros() {
  if (g_enable_hosted_apps_in_lacros_for_testing)
    return true;

  return chromeos::BrowserParamsProxy::Get()->PublishHostedApps();
}

void EnableHostedAppsInLacrosForTesting() {
  g_enable_hosted_apps_in_lacros_for_testing = true;
}
#endif  // IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS)
bool ShouldMuxExtensionIds() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros uses the value passed from ash.
  return !chromeos::BrowserParamsProxy::Get()->DoNotMuxExtensionAppIds();
#else
  bool keeplist_enabled =
      crosapi::browser_util::IsLacrosPrimaryBrowser() &&
      base::FeatureList::IsEnabled(
          ash::features::kEnforceAshExtensionKeeplist) &&
      base::FeatureList::IsEnabled(kStopMuxingLacrosExtensionAppIds);
  // Muxing is only necessary if the keeplist is disabled.
  return !keeplist_enabled;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

std::string MuxId(const Profile* profile, const std::string& extension_id) {
  DCHECK(profile);
  if (ShouldMuxExtensionIds()) {
    return profile->GetBaseName().value() + kExtensionAppMuxedIdDelimiter +
           extension_id;
  } else {
    return extension_id;
  }
}

std::vector<std::string> DemuxId(const std::string& muxed_id) {
  if (ShouldMuxExtensionIds()) {
    return base::SplitStringUsingSubstr(
        muxed_id, apps::kExtensionAppMuxedIdDelimiter, base::KEEP_WHITESPACE,
        base::SPLIT_WANT_ALL);
  } else {
    return std::vector<std::string>{"", muxed_id};
  }
}
std::string GetStandaloneBrowserExtensionAppId(const std::string& app_id) {
  std::vector<std::string> splits = DemuxId(app_id);
  return (splits.size() == 2) ? splits[1] : app_id;
}

// TODO(https://crbug.com/1225848): This logic can be removed once ash is past
// M105.
const char kExtensionAppMuxedIdDelimiter[] = "###";
#endif  // IS_CHROMEOS

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
