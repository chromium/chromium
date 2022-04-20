// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps_utils.h"

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

const char kExtensionAppMuxedIdDelimiter[] = "###";
}  // namespace apps
