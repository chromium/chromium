// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(PolicyTest, NetworkPrediction) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  // Enabled by default.
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  // Disabled by policy.
  PolicyMap policies;
  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   prefetch::NetworkPredictionOptions::kDisabled)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  // Enabled by policy.
  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   prefetch::NetworkPredictionOptions::kStandard)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   prefetch::NetworkPredictionOptions::kWifiOnlyDeprecated)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   prefetch::NetworkPredictionOptions::kExtended)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);
}

}  // namespace policy
