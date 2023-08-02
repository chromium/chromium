// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns true if hosted apps should run in Lacros.
bool ShouldHostedAppsRunInLacros();

// Enables hosted apps in Lacros for testing.
void EnableHostedAppsInLacrosForTesting();
#endif  // IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the escaped app_id to be passed to chrome::ShowAppManagementPage().
std::string GetEscapedAppId(const std::string& app_id, AppType app_type);
#endif

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
