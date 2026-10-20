// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"

#include <algorithm>
#include <functional>
#include <string>

#include "base/containers/flat_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_auth {

namespace {

constexpr char kUnknownId[] = "UNKNOWN_ID";

}  // namespace

// Test that every supported IdP can be found by its own team and extension IDs.
TEST(ExtensibleEnterpriseSsoMetadataTest, FindsEverySupportedIdp) {
  for (const SsoExtensionMetadata& expected : GetSupportedIdentityProviders()) {
    SCOPED_TRACE(expected.idp_name);
    const SsoExtensionMetadata* found =
        FindSsoExtensionMetadata(expected.team_id, expected.extension_id);
    ASSERT_TRUE(found);
    EXPECT_EQ(expected.idp_name, found->idp_name);
  }
}

// Test that the Okta IDs resolve to the Okta IdP.
TEST(ExtensibleEnterpriseSsoMetadataTest, FindsOkta) {
  const SsoExtensionMetadata* found =
      FindSsoExtensionMetadata(kOktaSsoTeamId, kOktaSsoExtensionId);
  ASSERT_TRUE(found);
  EXPECT_EQ(kOktaIdentityProvider, found->idp_name);
}

// Test that the Microsoft IDs resolve to the Microsoft IdP.
TEST(ExtensibleEnterpriseSsoMetadataTest, FindsMicrosoft) {
  const SsoExtensionMetadata* found =
      FindSsoExtensionMetadata(kMicrosoftSsoTeamId, kMicrosoftSsoExtensionId);
  ASSERT_TRUE(found);
  EXPECT_EQ(kMicrosoftIdentityProvider, found->idp_name);
}

// Tests that the Ping IDs resolve to the Ping IdP.
TEST(ExtensibleEnterpriseSsoMetadataTest, FindsPing) {
  const SsoExtensionMetadata* found =
      FindSsoExtensionMetadata(kPingSsoTeamId, kPingSsoExtensionId);
  ASSERT_TRUE(found);
  EXPECT_EQ(kPingIdentityProvider, found->idp_name);
}

// Test that team and extension IDs belonging to different IdPs don't match.
TEST(ExtensibleEnterpriseSsoMetadataTest, MismatchedIdsReturnNull) {
  EXPECT_FALSE(
      FindSsoExtensionMetadata(kOktaSsoTeamId, kMicrosoftSsoExtensionId));
  EXPECT_FALSE(
      FindSsoExtensionMetadata(kMicrosoftSsoTeamId, kOktaSsoExtensionId));
  EXPECT_FALSE(FindSsoExtensionMetadata(kPingSsoTeamId, kOktaSsoExtensionId));
  EXPECT_FALSE(FindSsoExtensionMetadata(kOktaSsoTeamId, kPingSsoExtensionId));
  EXPECT_FALSE(
      FindSsoExtensionMetadata(kPingSsoTeamId, kMicrosoftSsoExtensionId));
  EXPECT_FALSE(
      FindSsoExtensionMetadata(kMicrosoftSsoTeamId, kPingSsoExtensionId));
}

// Test that unknown IDs don't match any IdP.
TEST(ExtensibleEnterpriseSsoMetadataTest, UnknownIdsReturnNull) {
  EXPECT_FALSE(FindSsoExtensionMetadata(kUnknownId, kOktaSsoExtensionId));
  EXPECT_FALSE(FindSsoExtensionMetadata(kOktaSsoTeamId, kUnknownId));
  EXPECT_FALSE(FindSsoExtensionMetadata(kUnknownId, kUnknownId));
}

// Test that exactly the expected IdPs are supported.
TEST(ExtensibleEnterpriseSsoMetadataTest, SupportedIdps) {
  const base::flat_set<std::string> expected_names = {
      kMicrosoftIdentityProvider, kOktaIdentityProvider, kPingIdentityProvider};
  const base::flat_set<std::string> actual_names =
      base::MakeFlatSet<std::string>(GetSupportedIdentityProviders(),
                                     std::less<>(),
                                     [](const SsoExtensionMetadata& data) {
                                       return std::string(data.idp_name);
                                     });
  EXPECT_EQ(expected_names, actual_names);
  EXPECT_EQ(expected_names.size(), GetSupportedIdentityProviders().size());
}

// Test that the supported IdPs are strictly ordered by `idp_name`, which also
// guarantees that there are no duplicates.
TEST(ExtensibleEnterpriseSsoMetadataTest, SupportedIdpsAreSortedAndUnique) {
  EXPECT_TRUE(std::ranges::adjacent_find(GetSupportedIdentityProviders(),
                                         std::ranges::greater_equal(),
                                         &SsoExtensionMetadata::idp_name) ==
              GetSupportedIdentityProviders().end());
}

}  // namespace enterprise_auth
