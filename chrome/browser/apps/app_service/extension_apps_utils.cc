// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps_utils.h"

#include "base/feature_list.h"
#include "build//build_config.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool g_enable_hosted_apps_in_lacros_for_testing = false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A kill switch for lacros hosted apps.
const base::Feature kLacrosDisableHostedApps{"LacrosDisableHostedApps",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif
}  // namespace

namespace apps {

bool ShouldHostedAppsRunInLacros() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(kLacrosDisableHostedApps))
    return false;

  // Lacros hosted apps and chrome apps (i.e. platform apps) are both extension
  // based apps and belong to the same AppType: kStandaloneBrowserChromeApp.
  // They are handled by the same AppPublisher and AppController classes
  // (StandaloneBrowserExtensionApps, LacrosExtensionAppsPublisher,
  // LacrosExtensionAppController) in Ash and Lacros, which are made available
  // if crosapi::browser_util::IsLacrosChromeAppsEnabled().
  if (!crosapi::browser_util::IsLacrosChromeAppsEnabled())
    return false;

  // Laros hosted apps use BrowserAppInstanceRegistry to track its running
  // instances in browser windows and tabs, which is made available if
  // web_app::IsWebAppsCrosapiEnabled().
  if (!web_app::IsWebAppsCrosapiEnabled())
    return false;

  return true;
#else  // IS_CHROMEOS_LACROS
  if (g_enable_hosted_apps_in_lacros_for_testing)
    return true;

  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service && lacros_service->init_params()->publish_hosted_apps;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void EnableHostedAppsInLacrosForTesting() {
  g_enable_hosted_apps_in_lacros_for_testing = true;
}
#endif

const char kExtensionAppMuxedIdDelimiter[] = "###";
}  // namespace apps
