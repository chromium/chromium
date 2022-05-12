// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps_utils.h"

#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool g_enable_hosted_apps_in_lacros_for_testing = false;
#endif
}  // namespace

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ShouldHostedAppsRunInLacros() {
  if (g_enable_hosted_apps_in_lacros_for_testing)
    return true;

  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service && lacros_service->init_params()->publish_hosted_apps;
}

void EnableHostedAppsInLacrosForTesting() {
  g_enable_hosted_apps_in_lacros_for_testing = true;
}
#endif  // IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS)
std::string MuxId(const Profile* profile, const std::string& extension_id) {
  DCHECK(profile);
  return profile->GetBaseName().value() + kExtensionAppMuxedIdDelimiter +
         extension_id;
}

std::vector<std::string> DemuxId(const std::string& muxed_id) {
  return base::SplitStringUsingSubstr(
      muxed_id, apps::kExtensionAppMuxedIdDelimiter, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_ALL);
}
std::string GetStandaloneBrowserExtensionAppId(const std::string& app_id) {
  std::vector<std::string> splits = DemuxId(app_id);
  return (splits.size() == 2) ? splits[1] : app_id;
}

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
