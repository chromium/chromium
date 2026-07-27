// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/management/management_util.h"

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC));

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

policy::ManagementAuthorityTrustworthiness
GetHigherManagementAuthorityTrustworthiness(Profile* profile) {
  policy::ManagementAuthorityTrustworthiness platform_trustworthiness =
      policy::ManagementServiceFactory::GetForPlatform()
          ->GetManagementAuthorityTrustworthiness();
  if (profile->IsGuestSession() || profile->IsSystemProfile()) {
    // Guest and System profiles cannot have user-level management policies.
    // We only return the platform-level trustworthiness and avoid triggering
    // the creation of the profile-specific management service.
    return platform_trustworthiness;
  }
  policy::ManagementAuthorityTrustworthiness browser_trustworthiness =
      policy::ManagementServiceFactory::GetForProfile(profile)
          ->GetManagementAuthorityTrustworthiness();
  return std::max(platform_trustworthiness, browser_trustworthiness);
}

}  // namespace extensions
