// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_METADATA_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_METADATA_H_

#include <CoreFoundation/CFBase.h>

#include <string>

#include "base/containers/span.h"

namespace enterprise_auth {

extern const char kMicrosoftIdentityProvider[];
extern const CFStringRef kMicrosoftSsoExtensionId;
extern const CFStringRef kMicrosoftSsoTeamId;

extern const char kOktaIdentityProvider[];
extern const CFStringRef kOktaSsoExtensionId;
extern const CFStringRef kOktaSsoTeamId;

// This struct abstracts metadata about Apple's Extensible Enterprise SSO
// extensions.
struct SsoExtensionMetadata {
  // The name of the identity provider. This must match with the
  // ExtensibleEnterpriseSSOBlocklist policy definition.
  std::string idp_name;

  // The team identifier entry from the MDM payload configuring the extension.
  // Each IdP posts this information in their public documentation.
  CFStringRef team_id;

  // The extension identifier entry from the MDM payload configuring the
  // extension. Each IdP posts this information in their public documentation.
  CFStringRef extension_id;
};

// Returns the metadata of all identity providers supported by Chrome's
// Extensible Enterprise SSO integration, ordered by `idp_name`.
// The backing storage is static so the caller must not worry about the lifetime
// of the span.
base::span<const SsoExtensionMetadata> GetSupportedIdentityProviders();

// Returns the entry of `GetSupportedIdentityProviders()` whose `team_id` and
// `extension_id` both match the given values, or nullptr if there is none.
// `team_id` and `extension_id` must not be null.
const SsoExtensionMetadata* FindSsoExtensionMetadata(CFStringRef team_id,
                                                     CFStringRef extension_id);

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_METADATA_H_
