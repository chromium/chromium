// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

class AssistantPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(AssistantPolicyTest, AssistantContextEnabled) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantContextEnabled));
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantContextEnabled, true);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));

  // Verifies that the Assistant context can be forced to always disabled.
  PolicyMap policies;
  policies.Set(key::kVoiceInteractionContextEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantContextEnabled));
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantContextEnabled, true);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));

  // Verifies that the Assistant context can be forced to always enabled.
  policies.Set(key::kVoiceInteractionContextEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantContextEnabled));
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantContextEnabled, false);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(AssistantPolicyTest, AssistantHotwordEnabled) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled, true);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));

  // Verifies that the Assistant hotword can be forced to always disabled.
  PolicyMap policies;
  policies.Set(key::kVoiceInteractionHotwordEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled, true);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));

  // Verifies that the Assistant hotword can be forced to always enabled.
  policies.Set(key::kVoiceInteractionHotwordEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(
      ash::assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));
  prefs->SetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled, false);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::assistant::prefs::kAssistantHotwordEnabled));
}

}  // namespace policy
