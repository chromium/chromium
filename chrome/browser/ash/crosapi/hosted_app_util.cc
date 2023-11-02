// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/hosted_app_util.h"

#include "base/feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"

namespace crosapi {

namespace {
// A kill switch for lacros hosted apps.
BASE_FEATURE(kStandaloneBrowserDisableHostedApps,
             "StandaloneBrowserDisableHostedApps",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

bool IsStandaloneBrowserHostedAppsEnabled() {
  if (base::FeatureList::IsEnabled(kStandaloneBrowserDisableHostedApps))
    return false;

  // Lacros hosted apps and chrome apps (i.e. platform apps) are both extension
  // based apps and belong to the same AppType: kStandaloneBrowserChromeApp.
  // They are handled by the same AppPublisher and AppController classes
  // (StandaloneBrowserExtensionApps, LacrosExtensionAppsPublisher,
  // LacrosExtensionAppController) in Ash and Lacros, which are made available
  // if crosapi::browser_util::IsLacrosChromeAppsEnabled().
  if (!crosapi::browser_util::IsLacrosChromeAppsEnabled())
    return false;

  // Lacros hosted apps use BrowserAppInstanceRegistry to track its running
  // instances in browser windows and tabs, which is made available if
  // web_app::IsWebAppsCrosapiEnabled().
  if (!web_app::IsWebAppsCrosapiEnabled())
    return false;

  return true;
}

}  // namespace crosapi
