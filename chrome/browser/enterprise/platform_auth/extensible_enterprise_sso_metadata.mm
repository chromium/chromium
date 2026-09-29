// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"

#include <CoreFoundation/CoreFoundation.h>

#include <array>
#include <functional>
#include <string>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/values.h"

namespace enterprise_auth {

constexpr char kMicrosoftIdentityProvider[] = "microsoft";
const CFStringRef kMicrosoftSsoExtensionId(
    CFSTR("com.microsoft.CompanyPortalMac.ssoextension"));
const CFStringRef kMicrosoftSsoTeamId(CFSTR("UBF8T346G9"));

constexpr char kOktaIdentityProvider[] = "okta";
const CFStringRef kOktaSsoExtensionId(
    CFSTR("com.okta.mobile.auth-service-extension"));
const CFStringRef kOktaSsoTeamId(CFSTR("B7F62B65BN"));

base::span<const SsoExtensionMetadata> GetSupportedIdentityProviders() {
  // Kept sorted by `idp_name`.
  static const base::NoDestructor kSupportedIdPs(
      std::to_array<SsoExtensionMetadata>({
          {.idp_name = kMicrosoftIdentityProvider,
           .team_id = kMicrosoftSsoTeamId,
           .extension_id = kMicrosoftSsoExtensionId},
          {.idp_name = kOktaIdentityProvider,
           .team_id = kOktaSsoTeamId,
           .extension_id = kOktaSsoExtensionId},
      }));
  return *kSupportedIdPs;
}

const SsoExtensionMetadata* FindSsoExtensionMetadata(CFStringRef team_id,
                                                     CFStringRef extension_id) {
  CHECK(team_id && extension_id);
  for (const SsoExtensionMetadata& extension :
       GetSupportedIdentityProviders()) {
    if (CFEqual(team_id, extension.team_id) &&
        CFEqual(extension_id, extension.extension_id)) {
      return &extension;
    }
  }

  return nullptr;
}

}  // namespace enterprise_auth
