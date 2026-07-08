// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace dictation {

namespace {

// VoiceTypingSettings policy values
constexpr int kPolicyValueEnabledWithLogging = 0;
constexpr int kPolicyValueEnabledWithoutLogging = 1;
constexpr int kPolicyValueDisabled = 2;

// GenAiDefaultSettings policy values
constexpr int kDefaultSettingsValueAllowed = 0;
constexpr int kDefaultSettingsValueAllowedWithoutLogging = 1;
constexpr int kDefaultSettingsValueDisabled = 2;

class DictationKeyedServicePolicyTest : public policy::PolicyTest {
 public:
  DictationKeyedServicePolicyTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {}
  ~DictationKeyedServicePolicyTest() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();
    profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                      true);
  }

  void SetPolicyCombination(std::optional<int> voice_typing_policy,
                            std::optional<int> gen_ai_policy) {
    policy::PolicyMap policies;
    if (voice_typing_policy.has_value()) {
      policies.Set(policy::key::kVoiceTypingSettings,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(voice_typing_policy.value()), nullptr);
    }
    if (gen_ai_policy.has_value()) {
      policies.Set(policy::key::kGenAiDefaultSettings,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(gen_ai_policy.value()), nullptr);
    }
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyEnabledWithLogging) {
  SetPolicyCombination(kPolicyValueEnabledWithLogging, std::nullopt);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyEnabledWithoutLogging) {
  SetPolicyCombination(kPolicyValueEnabledWithoutLogging, std::nullopt);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest, PolicyDisabled) {
  SetPolicyCombination(kPolicyValueDisabled, std::nullopt);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyDefaultSettingsDisabled) {
  SetPolicyCombination(std::nullopt, kDefaultSettingsValueDisabled);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyDefaultSettingsAllowed) {
  SetPolicyCombination(std::nullopt, kDefaultSettingsValueAllowed);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyDefaultSettingsAllowedWithoutLogging) {
  SetPolicyCombination(std::nullopt,
                       kDefaultSettingsValueAllowedWithoutLogging);

  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->ShouldShowContextMenuItem());
}

// Verifies that the specific VoiceTypingSettings policy overrides the fallback
// GenAiDefaultSettings policy when they are configured with conflicting values.
IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       SpecificPolicyOverridesDefaultSettings) {
  {
    SetPolicyCombination(kPolicyValueEnabledWithLogging,
                         kDefaultSettingsValueDisabled);

    DictationKeyedService* service = DictationKeyedService::Get(profile());
    ASSERT_NE(service, nullptr);
    EXPECT_TRUE(service->ShouldShowContextMenuItem());
  }

  {
    SetPolicyCombination(kPolicyValueDisabled, kDefaultSettingsValueAllowed);

    DictationKeyedService* service = DictationKeyedService::Get(profile());
    ASSERT_NE(service, nullptr);
    EXPECT_FALSE(service->ShouldShowContextMenuItem());
  }
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       PolicyChangeEndsActiveSession) {
  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);

  service->StartSession(
      *GetBrowserWindowInterface(),
      DefaultInPageTargetId(chrome_test_utils::GetActiveWebContents(this)));
  ASSERT_NE(service->session_controller(), nullptr);

  // Set policy to disabled (value = 2) in real-time.
  SetPolicyCombination(kPolicyValueDisabled, std::nullopt);

  EXPECT_EQ(service->session_controller(), nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServicePolicyTest,
                       ContextMenuDisabledByPolicy) {
  DictationKeyedService* service = DictationKeyedService::Get(profile());
  ASSERT_NE(service, nullptr);

  // 1. With policy enabled, the menu item should be present and enabled.
  SetPolicyCombination(kPolicyValueEnabledWithLogging, std::nullopt);
  content::ContextMenuParams params;
  params.is_editable = true;
  auto menu = std::make_unique<TestRenderViewContextMenu>(
      *chrome_test_utils::GetActiveWebContents(this)->GetPrimaryMainFrame(),
      params);
  menu->Init();
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION));
  EXPECT_TRUE(menu->IsItemEnabled(IDC_CONTENT_CONTEXT_DICTATION));

  // 2. If we disable the policy, executing the command should be a safe no-op
  // and not start a session.
  SetPolicyCombination(kPolicyValueDisabled, std::nullopt);
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_DICTATION, 0);
  EXPECT_EQ(service->session_controller(), nullptr);
}

}  // namespace

}  // namespace dictation
