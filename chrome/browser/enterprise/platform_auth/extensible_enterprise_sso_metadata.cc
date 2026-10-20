// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "base/values.h"

namespace enterprise_auth {

constexpr char kMicrosoftIdentityProvider[] = "microsoft";
constexpr char kMicrosoftSsoExtensionId[] =
    "com.microsoft.CompanyPortalMac.ssoextension";
constexpr char kMicrosoftSsoTeamId[] = "UBF8T346G9";

constexpr char kOktaIdentityProvider[] = "okta";
constexpr char kOktaSsoExtensionId[] = "com.okta.mobile.auth-service-extension";
constexpr char kOktaSsoTeamId[] = "B7F62B65BN";

constexpr char kPingIdentityProvider[] = "ping";
constexpr char kPingSsoExtensionId[] =
    "com.pingidentity.pingid.desktop.pingssoe";
constexpr char kPingSsoTeamId[] = "6U3RF4C84N";

constexpr auto kSupportedIdPs = std::to_array<SsoExtensionMetadata>({
    {.idp_name = kMicrosoftIdentityProvider,
     .team_id = kMicrosoftSsoTeamId,
     .extension_id = kMicrosoftSsoExtensionId},
    {.idp_name = kOktaIdentityProvider,
     .team_id = kOktaSsoTeamId,
     .extension_id = kOktaSsoExtensionId},
    {.idp_name = kPingIdentityProvider,
     .team_id = kPingSsoTeamId,
     .extension_id = kPingSsoExtensionId},
});

base::span<const SsoExtensionMetadata> GetSupportedIdentityProviders() {
  return kSupportedIdPs;
}

const SsoExtensionMetadata* FindSsoExtensionMetadata(
    std::string_view team_id,
    std::string_view extension_id) {
  for (const SsoExtensionMetadata& extension :
       GetSupportedIdentityProviders()) {
    if (team_id == extension.team_id &&
        extension_id == extension.extension_id) {
      return &extension;
    }
  }

  return nullptr;
}

}  // namespace enterprise_auth
