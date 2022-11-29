// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

class SuggestedContentPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(SuggestedContentPolicyTest, SuggestedContentEnabled) {
  // Verify Suggested Content pref behavior before policy.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(
      prefs->IsManagedPreference(ash::prefs::kSuggestedContentEnabled));
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));
  prefs->SetBoolean(ash::prefs::kSuggestedContentEnabled, false);
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));

  // Verify that Suggested Content can be force disabled.
  PolicyMap policies;
  policies.Set(key::kSuggestedContentEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kSuggestedContentEnabled));
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));
  prefs->SetBoolean(ash::prefs::kSuggestedContentEnabled, true);
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));

  // Verify that Suggested Content can be force enabled.
  policies.Set(key::kSuggestedContentEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kSuggestedContentEnabled));
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));
  prefs->SetBoolean(ash::prefs::kSuggestedContentEnabled, false);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kSuggestedContentEnabled));
}

}  // namespace policy
