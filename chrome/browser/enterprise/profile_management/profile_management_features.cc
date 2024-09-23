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
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOidcAuthResponseInterception,
             "OidcAuthResponseInterception",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOidcEnrollmentTimeout,
             "kOidcEnrollmentTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableGenericOidcAuthProfileManagement,
             "EnableGenericOidcAuthProfileManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOidcEnrollmentAuthSource,
             "OidcEnrollmentAuthSource",
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

// If set to `true`, OIDC flow will always fail its registration and trigger the
// Error dialog.
constexpr base::FeatureParam<bool> kOidcAuthForceErrorUi{
    &kOidcAuthProfileManagement, "force_error_ui", false};

// If set to `true`, OIDC flow will always fail its policy fetch and trigger the
// Timeout dialog.
constexpr base::FeatureParam<bool> kOidcAuthForceTimeoutUi{
    &kOidcAuthProfileManagement, "force_timeout_ui", false};

// Controls the timeout duration of client registration during OIDC enrollment
// flow, in seconds.
constexpr base::FeatureParam<base::TimeDelta> kOidcEnrollRegistrationTimeout{
    &kOidcEnrollmentTimeout, "registration_timeout", base::Seconds(30)};

// Allow Oidc Enrollment flow to consider more hosts as eligible authentication
// sources.
constexpr base::FeatureParam<std::string> kOidcAuthAdditionalHosts{
    &kOidcEnrollmentAuthSource, "hosts", ""};

}  // namespace profile_management::features
