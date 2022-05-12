// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace apps {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns true if hosted apps should run in Lacros.
bool ShouldHostedAppsRunInLacros();

// Enables hosted apps in Lacros for testing.
void EnableHostedAppsInLacrosForTesting();
#endif  // IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS)
// Returns a muxed id that consists of the profile base name joined to the
// extension id.
std::string MuxId(const Profile* profile, const std::string& extension_id);

// Takes |muxed_id| and extracts the corresponding profile name and extension id
// into the return value. E.g. for Chrome app id
// "Default###plfjlfohfjjpmmifkbcmalnmcebkklkh", returns
// ["Default", "plfjlfohfjjpmmifkbcmalnmcebkklkh"]. For Chrome app id
// "plfjlfohfjjpmmifkbcmalnmcebkklkh", returns
// ["plfjlfohfjjpmmifkbcmalnmcebkklkh"].
std::vector<std::string> DemuxId(const std::string& muxed_id);

// Returns the real app id for Chrome apps or extensions. E.g. for Chrome app id
// "Default###plfjlfohfjjpmmifkbcmalnmcebkklkh", returns
// "plfjlfohfjjpmmifkbcmalnmcebkklkh".
std::string GetStandaloneBrowserExtensionAppId(const std::string& app_id);

// The delimiter separating the profile basename from the extension id
// in the muxed app id of standalone browser extension apps.
extern const char kExtensionAppMuxedIdDelimiter[];
#endif  // IS_CHROMEOS

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the escaped app_id to be passed to chrome::ShowAppManagementPage().
std::string GetEscapedAppId(const std::string& app_id, AppType app_type);
#endif

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_EXTENSION_APPS_UTILS_H_
