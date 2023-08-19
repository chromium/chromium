// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(PolicyTest, UrlKeyedAnonymizedDataCollection) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Disable by policy.
  PolicyMap policies;
  policies.Set(key::kUrlKeyedAnonymizedDataCollectionEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enable by policy.
  policies.Set(key::kUrlKeyedAnonymizedDataCollectionEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(true), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, UrlKeyedMetricsAllowed) {
  PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();
  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Disable by policy.
  PolicyMap policies;
  policies.Set(key::kUrlKeyedMetricsAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Enable by policy.
  policies.Set(key::kUrlKeyedMetricsAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(PolicyMap());

  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

}  // namespace policy
