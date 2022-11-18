// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for "policy_ids" which represent a way
// to specify apps in policy definitions on ChromeOS.
// ChromeOS assigns each app a unique 32-digit identifier that is usually not
// known by admins. The utility functions below help to bridge this gap and
// convert policy ids into internal apps ids and back at runtime.
// Supported app types are:
//    * Web Apps
//    * Arc Apps
//    * Chrome Apps
//    * System Web Apps
//    * Preinstalled Web Apps

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_POLICY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_POLICY_UTIL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Profile;

namespace apps_util {

// Checks whether |policy_id| specifies an app of supported type.
bool IsSupportedAppTypePolicyId(base::StringPiece policy_id);

// Checks whether |policy_id| specifies a Chrome App.
bool IsChromeAppPolicyId(base::StringPiece policy_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Checks whether |policy_id| specifies an Arc App.
bool IsArcAppPolicyId(base::StringPiece policy_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Checks whether |policy_id| specifies a Web App.
bool IsWebAppPolicyId(base::StringPiece policy_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Checks whether |policy_id| specifies a System Web App.
bool IsSystemWebAppPolicyId(base::StringPiece policy_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Checks whether |policy_id| specifies a Preinstalled Web App.
bool IsPreinstalledWebAppPolicyId(base::StringPiece policy_id);

// Transforms the provided |raw_policy_id| if necessary.
// For Web Apps, converts it to GURL and returns the spec().
// Does nothing for other app types.
std::string TransformRawPolicyId(const std::string& raw_policy_id);

// Returns |app_id| of the app that has a matching |policy_id| among
// |policy_ids| or absl::nullopt if none matches.
absl::optional<std::string> GetAppIdFromPolicyId(Profile*,
                                                 const std::string& policy_id);

// Returns the |policy_ids| field of the app with id equal to |app_id| or
// absl::nullopt if there's no such app.
//
// Web App Example:
// Admin installs a Web App using "https://foo.example" as the install URL.
// Chrome generates an app id based on the URL e.g. "abc123". Calling
// GetPolicyIdsFromAppId() with "abc123" will return {"https://foo.example"}.
//
// Arc++ Example:
// Admin installs an Android App with package name "com.example.foo". Chrome
// generates an app id based on the package e.g. "123abc". Calling
// GetPolicyIdsFromAppId() with "123abc" will return {"com.example.foo"}.
//
// Chrome App Example:
// Admin installs a Chrome App with "aaa111" as its app id. Calling
// GetPolicyIdsFromAppId() with "aaa111" will return {"aaa111"}.
//
// System Web App Example:
// Chrome generates apps ids for all System Web Apps -- let's say the id of the
// Camera app is "hfhhnacclhffhdffklopdkcgdhifgngh". Calling
// GetPolicyIdsFromAppId() with "hfhhnacclhffhdffklopdkcgdhifgngh" will return
// {"camera"}.
absl::optional<std::vector<std::string>> GetPolicyIdsFromAppId(
    Profile*,
    const std::string& app_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Maps SystemWebAppType to a policy id.
// Returns absl::nullopt for apps not included in official builds.
absl::optional<base::StringPiece> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Returns the policy ID for a given preinstalled web app ID. Note that not all
// preinstalled web apps are supposed to have a policy ID (currently we only
// support EDU apps) - in all other cases this will return absl::nullopt.
absl::optional<base::StringPiece> GetPolicyIdForPreinstalledWebApp(
    base::StringPiece preinstalled_web_app_id);

void SetPreinstalledWebAppsMappingForTesting(
    absl::optional<base::flat_map<base::StringPiece, base::StringPiece>>
        preinstalled_web_apps_mapping_for_testing);

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_POLICY_UTIL_H_
