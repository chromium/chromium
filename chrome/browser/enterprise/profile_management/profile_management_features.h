// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace profile_management::features {

// Controls whether third-party profile management is enabled.
BASE_DECLARE_FEATURE(kThirdPartyProfileManagement);

// Controls whether token-based profile management is enabled.
BASE_DECLARE_FEATURE(kEnableProfileTokenManagement);

// Controls whether OIDC-response profile management is enabled.
BASE_DECLARE_FEATURE(kOidcAuthProfileManagement);

// Controls whether OIDC profile enrollment can be started from navigation
// response in addition to redirections.
BASE_DECLARE_FEATURE(kOidcAuthResponseInterception);

// Controls whether OIDC enrollment process can time out (and after how long).
BASE_DECLARE_FEATURE(kOidcEnrollmentTimeout);

// Controls whether the generic OIDC-response profile management is enabled.
BASE_DECLARE_FEATURE(kEnableGenericOidcAuthProfileManagement);

// Controls whether to add a list of hosts that are eligible for OIDC profile
// enrollments.
BASE_DECLARE_FEATURE(kOidcEnrollmentAuthSource);

// Controls whether OIDC interception from navigation auth header instead of the
// usual URL params is permitted. This flag only works on Chrome Canary or Dev.
BASE_DECLARE_FEATURE(kOidcAuthHeaderInterception);

// Controls whether remote commands is enabled for OIDC profiles.
BASE_DECLARE_FEATURE(kEnableOidcProfileRemoteCommands);

// Oidc authentication related feature params.
extern const base::FeatureParam<std::string> kOidcAuthStubDmToken;
extern const base::FeatureParam<std::string> kOidcAuthStubProfileId;
extern const base::FeatureParam<std::string> kOidcAuthStubClientId;
extern const base::FeatureParam<std::string> kOidcAuthStubUserName;
extern const base::FeatureParam<std::string> kOidcAuthStubUserEmail;
extern const base::FeatureParam<bool> kOidcAuthIsDasherBased;
extern const base::FeatureParam<int> kOidcAuthForceErrorUi;
extern const base::FeatureParam<bool> kOidcAuthForceTimeoutUi;
extern const base::FeatureParam<base::TimeDelta> kOidcEnrollRegistrationTimeout;

// List of hosts to be added as eligible to `kOidcEnrollmentAuthSource`,
// takes the form of a comma-separated string.
extern const base::FeatureParam<std::string> kOidcAuthAdditionalHosts;

// List of URLs to be added as eligible to `kOidcAuthHeaderInterception`,
// takes the form of a comma-separated string.
extern const base::FeatureParam<std::string> kOidcAuthAdditionalUrls;

}  // namespace profile_management::features

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_
