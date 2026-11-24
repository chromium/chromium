// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_switch_notice.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/common/glic_relaunch.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_assistant_switcher.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/glic_overlay.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

constexpr char kPolicyUrl[] = "https://business.gemini.google/home/cid/abc123";

// Counts the client load errors reported by a host.
class ClientLoadErrorCounter : public Host::Observer {
 public:
  explicit ClientLoadErrorCounter(Host& host) { observation_.Observe(&host); }

  void ClientLoadErrorOccurred(ClientLoadErrorReason reason) override {
    ++count_;
  }

  int count() const { return count_; }

 private:
  int count_ = 0;
  base::ScopedObservation<Host, Host::Observer> observation_{this};
};

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

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Simulate a default/stable browser check so `CompleteOnboarding()`
    // asynchronously enables background mode across all build configurations
    // without querying the host OS default browser state.
    GlicLauncherConfiguration::SetCheckDefaultBrowserCallbackForTesting(
        base::BindRepeating([] {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &GlicLauncherConfiguration::OnCheckIsDefaultBrowserFinished,
                  version_info::Channel::STABLE,
                  shell_integration::DefaultWebClientState::IS_DEFAULT));
        }));
  }

  void TearDownOnMainThread() override {
    GlicLauncherConfiguration::SetCheckDefaultBrowserCallbackForTesting({});
    InProcessBrowserTest::TearDownOnMainThread();
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
    if (GlicEnabling::IsOsEntryPointEnabledForProfile(profile())) {
      ASSERT_TRUE(base::test::RunUntil([] {
        return GlicBackgroundModeManager::GetInstance()
            ->IsInBackgroundModeForTesting();
      }));
    }
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
    return GetGlicProviderSpec(profile()).GetProviderKeyForTesting();
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

// Opens Glic with a notice due. The `PRE_` step latches to GEiC from the
// user-scoped `GeminiEnterpriseUrl` policy and onboards to it. The policy is
// gone at the main step's startup, so it latches to Gemini with a GEiC to
// Gemini notice.
class GlicProviderSwitchNoticePanelBrowserTest : public GlicBrowserTest {
 public:
  explicit GlicProviderSwitchNoticePanelBrowserTest(bool no_webview = true) {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kGeic, true},
         {features::kGeicProviderSwitchNotice, true},
         // For RelaunchPanelTakesPriority.
         {features::kGeicProviderChangePrompt, true},
         {features::kGlicNoWebview, no_webview}});
  }

  void SetUpInProcessBrowserTestFixture() override {
    GlicBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    if (content::IsPreTest()) {
      SetPolicy();
    }
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  void SetPolicy() {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kPolicyUrl), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  // The `PRE_` step's body.
  void OnboardToGeic() {
    ASSERT_NE(GetGlicProviderSpec(GetProfile()).GetProviderKeyForTesting(),
              GlicProviderKey::Gemini());
    GetGlicProviderSpec(GetProfile())
        .GetFreStore()
        .CompleteOnboarding(GetProfile());
    ASSERT_TRUE(KnownProviderKey().has_value());
    ASSERT_NE(KnownProviderKey(), GlicProviderKey::Gemini());
  }

  // Returns the known provider, or std::nullopt if none is known.
  std::optional<GlicProviderKey> KnownProviderKey() {
    const PrefService& prefs = *GetProfile()->GetPrefs();
    if (!prefs.HasPrefPath(prefs::kGlicProviderSwitchNoticeKnownProviderKey)) {
      return std::nullopt;
    }
    return GlicProviderKey::FromPrefString(
        prefs.GetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  }

  GlicNoWebviewContentsManager* GetContentsManager(GlicInstanceImpl* instance) {
    return static_cast<GlicNoWebviewContentsManager*>(
        instance->host().contents_manager());
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       PRE_ContinueLoadsClient) {
  OnboardToGeic();
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       ContinueLoadsClient) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, instance));

  GetContentsManager(instance)
      ->GetOverlayPageHandlerForTesting()
      ->OnProviderSwitchNoticeContinueClicked();

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(instance->host().provider_switch_notice(),
            GlicProviderChange::kNone);
  EXPECT_OK(WaitForErrorPanelType(std::nullopt, instance));
  EXPECT_OK(WaitUntilGuestIsShowing(instance));
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       PRE_NoticePersistsAcrossReadyStateUpdates) {
  OnboardToGeic();
}

// Ready state updates while the notice is showing (e.g. from an unrelated pref
// change) don't clear it or report it again.
IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       NoticePersistsAcrossReadyStateUpdates) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, instance));
  ClientLoadErrorCounter errors(instance->host());

  // Toggling GeminiSettings notifies ready state observers twice, ending in
  // kReady.
  GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kReady);
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, instance));
  EXPECT_EQ(errors.count(), 0);
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       PRE_ContinueClearsNoticeInOtherInstance) {
  OnboardToGeic();
}

// Continuing in one instance drops the notice in other open instances.
IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       ContinueClearsNoticeInOtherInstance) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * first, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, first));
  // A second window keeps `first` visible, so it can't hibernate.
  BrowserWindowInterface* window = CreateAdditionalBrowserWindow();
  ASSERT_OK_AND_ASSIGN(
      GlicInstanceImpl * second,
      OpenGlicForTab(TabListInterface::From(window)->GetActiveTab()));
  ASSERT_NE(first, second);
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, second));

  GetContentsManager(second)
      ->GetOverlayPageHandlerForTesting()
      ->OnProviderSwitchNoticeContinueClicked();

  EXPECT_EQ(first->host().provider_switch_notice(), GlicProviderChange::kNone);
  EXPECT_OK(WaitForErrorPanelType(std::nullopt, first));
  EXPECT_OK(WaitForGlicClient(first));
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       PRE_ReopeningKeepsNotice) {
  OnboardToGeic();
}

// Closing the panel without continuing doesn't acknowledge the notice.
IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       ReopeningKeepsNotice) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, instance));
  PreventDeletionOnClose(instance);

  ASSERT_OK(CloseGlicForTabAndWait(tab));
  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kProviderSwitchNoticeGeicToGemini, instance));
  EXPECT_NE(KnownProviderKey(), GlicProviderKey::Gemini());
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       PRE_RelaunchPanelTakesPriority) {
  OnboardToGeic();
}

// An instance created while a provider change is pending shows the relaunch
// panel instead of the notice.
IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticePanelBrowserTest,
                       RelaunchPanelTakesPriority) {
  ASSERT_EQ(GetProviderSwitchNotice(*GetProfile()),
            GlicProviderChange::kGeicToGemini);
  SetPolicy();
  ASSERT_EQ(GlicEnabling::GetPendingProviderChange(GetProfile()),
            GlicProviderChange::kGeminiToGeic);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));
  EXPECT_EQ(instance->host().provider_switch_notice(),
            GlicProviderChange::kNone);
}

class GlicProviderSwitchNoticeWebviewBrowserTest
    : public GlicProviderSwitchNoticePanelBrowserTest {
 public:
  GlicProviderSwitchNoticeWebviewBrowserTest()
      : GlicProviderSwitchNoticePanelBrowserTest(/*no_webview=*/false) {}
};

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWebviewBrowserTest,
                       PRE_ContinueLoadsClient) {
  OnboardToGeic();
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWebviewBrowserTest,
                       ContinueLoadsClient) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(
      WaitForWebUiState(mojom::WebUiState::kProviderSwitchNoticeGeicToGemini));

  ASSERT_TRUE(
      content::ExecJs(instance->host().webui_contents(),
                      "document.getElementById('continueButton').click()"));

  EXPECT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWebviewBrowserTest,
                       PRE_ContinueClearsNoticeInOtherInstance) {
  OnboardToGeic();
}

IN_PROC_BROWSER_TEST_F(GlicProviderSwitchNoticeWebviewBrowserTest,
                       ContinueClearsNoticeInOtherInstance) {
  // `WaitForWebUiState()` needs a single instance, so poll each host.
  auto shows_notice = [](GlicInstanceImpl* instance) {
    return instance->host().GetPrimaryWebUiState() ==
           mojom::WebUiState::kProviderSwitchNoticeGeicToGemini;
  };
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * first, OpenGlicForActiveTab());
  ASSERT_TRUE(base::test::RunUntil([&] { return shows_notice(first); }));
  // A second window keeps `first` visible while it loads the client.
  BrowserWindowInterface* window = CreateAdditionalBrowserWindow();
  ASSERT_OK_AND_ASSIGN(
      GlicInstanceImpl * second,
      OpenGlicForTab(TabListInterface::From(window)->GetActiveTab()));
  ASSERT_NE(first, second);
  ASSERT_TRUE(base::test::RunUntil([&] { return shows_notice(second); }));

  ASSERT_TRUE(content::ExecJs(
      second->host().webui_contents(),
      "document.getElementById('continueButton').click()"));

  EXPECT_OK(WaitForGlicClient(first));
  EXPECT_EQ(first->host().provider_switch_notice(), GlicProviderChange::kNone);
}

}  // namespace
}  // namespace glic
