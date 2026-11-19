// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_switch_notice.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_provider_change_relaunch.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

constexpr char kPolicyUrl[] = "https://business.gemini.google/home/cid/abc123";

std::vector<base::test::FeatureRef> GetEnabledFeatures(
    bool enable_relaunch_prompt) {
  std::vector<base::test::FeatureRef> features =
      GetDefaultEnabledGlicTestFeatures();
  features.push_back(features::kGeic);
  features.push_back(features::kGeicProviderSwitchNotice);
  if (enable_relaunch_prompt) {
    features.push_back(features::kGeicProviderChangePrompt);
  }
  return features;
}

// Starts with no FRE completed. By default, the `PRE_` step starts on Gemini,
// and the user-scoped `GeminiEnterpriseUrl` policy is added before the main
// step's startup, so it latches to GEiC.
class GlicProviderSwitchNoticeBrowserTest : public InProcessBrowserTest {
 public:
  struct Options {
    bool enable_relaunch_prompt = false;
    policy::PolicyScope policy_scope = policy::POLICY_SCOPE_USER;
    // Whether the policy is also set at the `PRE_` step's startup.
    bool set_policy_in_pre_test = false;
  };

  GlicProviderSwitchNoticeBrowserTest()
      : GlicProviderSwitchNoticeBrowserTest(Options()) {}

  explicit GlicProviderSwitchNoticeBrowserTest(Options options)
      : options_(options),
        glic_test_env_(GlicTestEnvironmentConfig{.fre_status = std::nullopt},
                       GetEnabledFeatures(options.enable_relaunch_prompt)) {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    if (!content::IsPreTest() || options_.set_policy_in_pre_test) {
      SetPolicy();
    }
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  void SetPolicy() {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, options_.policy_scope,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kPolicyUrl), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  // Onboards the user to the latched provider, as its FRE does.
  void CompleteOnboarding() {
    GetGlicProviderSpec(profile()).GetFreStore().CompleteOnboarding(profile());
  }

  // Returns the known provider, or std::nullopt if none is known.
  std::optional<GlicProviderKey> KnownProviderKey() {
    const PrefService& prefs = *profile()->GetPrefs();
    if (!prefs.HasPrefPath(prefs::kGlicProviderSwitchNoticeKnownProviderKey)) {
      return std::nullopt;
    }
    return GlicProviderKey::FromPrefString(
        prefs.GetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  }

  // Returns the latched provider's key.
  GlicProviderKey LatchedProviderKey() {
    return GetGlicProviderSpec(profile()).GetProviderKey();
  }

 private:
  const Options options_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeBrowserTest,
                       PRE_GeminiToGeicShowsNotice) {
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::Gemini());
  ASSERT_EQ(KnownProviderKey(), std::nullopt);

  CompleteOnboarding();

  // Onboarding records Gemini as the known provider.
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile()), GlicProviderChange::kNone);
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeBrowserTest,
                       GeminiToGeicShowsNotice) {
  ASSERT_NE(LatchedProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile()),
            GlicProviderChange::kGeminiToGeic);
}

// Relaunching from the relaunch prompt applies the same switch, but the prompt
// already told the user about it. The PRE_ step brings in the policy at
// runtime so the relaunch button sees a pending change.
class GlicProviderSwitchNoticeWithPromptBrowserTest
    : public GlicProviderSwitchNoticeBrowserTest {
 public:
  GlicProviderSwitchNoticeWithPromptBrowserTest()
      : GlicProviderSwitchNoticeBrowserTest({.enable_relaunch_prompt = true}) {}
};

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWithPromptBrowserTest,
                       PRE_RelaunchFromPromptShowsNoNotice) {
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::Gemini());
  CompleteOnboarding();
  ASSERT_TRUE(KnownProviderKey().has_value());
  SetPolicy();
  ASSERT_EQ(GlicEnabling::GetPendingProviderChange(profile()),
            GlicProviderChange::kGeminiToGeic);

  bool restarted = false;
  auto restart_override = SetRestartCallbackForTesting(
      base::BindLambdaForTesting([&] { restarted = true; }));
  MaybeRelaunchForProviderChange(profile());
  EXPECT_TRUE(restarted);
  EXPECT_EQ(KnownProviderKey(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWithPromptBrowserTest,
                       RelaunchFromPromptShowsNoNotice) {
  ASSERT_NE(LatchedProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile()), GlicProviderChange::kNone);
}

// With a machine-scoped policy, the user can opt out to consumer Gemini. Doing
// so switches provider at the next startup, but the user made that switch, so
// there's no notice.
class GlicProviderSwitchNoticeUserOptOutBrowserTest
    : public GlicProviderSwitchNoticeBrowserTest {
 public:
  GlicProviderSwitchNoticeUserOptOutBrowserTest()
      : GlicProviderSwitchNoticeBrowserTest(
            {.policy_scope = policy::POLICY_SCOPE_MACHINE,
             .set_policy_in_pre_test = true}) {}
};

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeUserOptOutBrowserTest,
                       PRE_UserOptOutShowsNoNotice) {
  ASSERT_NE(LatchedProviderKey(), GlicProviderKey::Gemini());
  CompleteOnboarding();
  ASSERT_TRUE(KnownProviderKey().has_value());
  ASSERT_NE(KnownProviderKey(), GlicProviderKey::Gemini());

  SwitchAssistantSelection(*profile(), prefs::AssistantSelection::kConsumer);
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeUserOptOutBrowserTest,
                       UserOptOutShowsNoNotice) {
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile()), GlicProviderChange::kNone);
}

}  // namespace
}  // namespace glic
