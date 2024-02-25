// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_features.h"

#include "build/build_config.h"

namespace profile_management::features {

BASE_FEATURE(kThirdPartyProfileManagement,
             "ThirdPartyProfileManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableProfileTokenManagement,
             "EnableProfileTokenManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOidcAuthProfileManagement,
             "OidcAuthProfileManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow Oidc Enrollment flow to use a stubbed DM token rather than fetching a
// real one from DM server, if one is supplied.
constexpr base::FeatureParam<std::string> kOidcAuthStubDmToken{
    &kOidcAuthProfileManagement, "dm_token", ""};

// Allow Oidc Enrollment flow to use a stubbed profile id rather than generating
// one using regular workflow, if one is supplied.
constexpr base::FeatureParam<std::string> kOidcAuthStubProfileId{
    &kOidcAuthProfileManagement, "profile_id", ""};

// Allow Oidc Enrollment flow to use a stubbed client id rather than generating
// one using regular workflow, if one is supplied.
constexpr base::FeatureParam<std::string> kOidcAuthStubClientId{
    &kOidcAuthProfileManagement, "client_id", ""};

// Allow Oidc Enrollment flow to use a stubbed user display name instead of
// retrieving it from DM server.
constexpr base::FeatureParam<std::string> kOidcAuthStubUserName{
    &kOidcAuthProfileManagement, "user_name", ""};

// Allow Oidc Enrollment flow to use a stubbed user display email instead of
// retrieving it from DM server.
constexpr base::FeatureParam<std::string> kOidcAuthStubUserEmail{
    &kOidcAuthProfileManagement, "user_email", ""};

// Controls whether Oidc Enrollment flow follows dasherless flow or dasher-based
// flow. This param can only convert a dasher based flow to a dasherless one,
// and does not work the other way around.
constexpr base::FeatureParam<bool> kOidcAuthIsDasherBased{
    &kOidcAuthProfileManagement, "is_dasher_based", true};

}  // namespace profile_management::features
