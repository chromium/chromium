// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_util.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/invalidation/invalidation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace policy {

BASE_FEATURE(kDevicePolicyInvalidationWithDirectMessagesEnabled,
             "DevicePolicyInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDeviceLocalAccountPolicyInvalidationWithDirectMessagesEnabled,
             "DeviceLocalAccountPolicyInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kCbcmPolicyInvalidationWithDirectMessagesEnabled,
             "CbcmPolicyInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUserPolicyInvalidationWithDirectMessagesEnabled,
             "UserPolicyInvalidationWithDirectMessagesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// GCP number to be used for policy invalidations. Policy update is not
// considered critical to receive invalidation.
constexpr std::string_view kPolicyInvalidationProjectNumber =
    invalidation::kNonCriticalInvalidationsProjectNumber;

bool IsDirectInvalidationEnabledForScope(PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return base::FeatureList::IsEnabled(
          kUserPolicyInvalidationWithDirectMessagesEnabled);
    case PolicyInvalidationScope::kDevice:
      return base::FeatureList::IsEnabled(
          kDevicePolicyInvalidationWithDirectMessagesEnabled);
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return base::FeatureList::IsEnabled(
          kDeviceLocalAccountPolicyInvalidationWithDirectMessagesEnabled);
    case PolicyInvalidationScope::kCBCM:
      return base::FeatureList::IsEnabled(
          kCbcmPolicyInvalidationWithDirectMessagesEnabled);
  }
}

}  // namespace

bool IsOriginInAllowlist(const GURL& url,
                         const PrefService* prefs,
                         const char* allowlist_pref_name,
                         const char* always_allow_pref_name) {
  DCHECK(prefs);

  if (always_allow_pref_name && prefs->GetBoolean(always_allow_pref_name)) {
    return true;
  }

  const base::Value::List& allowlisted_urls =
      prefs->GetList(allowlist_pref_name);

  if (allowlisted_urls.empty()) {
    return false;
  }

  for (auto const& value : allowlisted_urls) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid()) {
      continue;
    }

    // Despite |url| being a GURL, the path is ignored when matching.
    if (pattern.Matches(url)) {
      return true;
    }
  }

  return false;
}

std::string_view GetPolicyInvalidationProjectNumber(
    PolicyInvalidationScope scope) {
  if (IsDirectInvalidationEnabledForScope(scope)) {
    return kPolicyInvalidationProjectNumber;
  }
  return kPolicyFCMInvalidationSenderID;
}

}  // namespace policy
