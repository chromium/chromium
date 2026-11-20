// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_provider_change_relaunch.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include <optional>

#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class GlicGeicApiBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicApiBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {
    feature_list_.InitAndEnableFeature(features::kGeic);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(geic::kGeicGuestURLSwitch,
                                    GetGuestURL().spec());
    // Allows the non-allowlisted test server guest URL.
    command_line->AppendSwitch(geic::kGeicAllowAnyGuestURLSwitch);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicSignInTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicAuthTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// crbug.com/582296507: GE auth flows that used `window.open()` never completed
// in the side panel, since the guest gets no handle to the opened tab:
// `MaybeHandleGlicWindowCreation()` opens it in a new tab or popup instead.
// Pins that contract alongside the `openAuthTab()` replacement.
IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest,
                       testGeicWindowOpenReturnsNullButAuthTabIsTracked) {
  ASSERT_OK(OpenGlicForActiveTab());
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  // `window.open()` returns null to the guest, but still opens a tab.
  ExecuteJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 2; }));
  tabs::TabInterface* window_open_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(window_open_tab);
  EXPECT_EQ(window_open_tab->GetContents()->GetVisibleURL(),
            GURL("https://www.chromium.org"));
  const tabs::TabHandle window_open_handle = window_open_tab->GetHandle();

  // `openAuthTab()` opens a separate tab that Chrome tracks.
  ContinueJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 3; }));
  tabs::TabInterface* auth_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(auth_tab);
  EXPECT_NE(auth_tab->GetHandle(), window_open_handle);
  EXPECT_EQ(auth_tab->GetContents()->GetVisibleURL(),
            GURL("https://accounts.google.com/signin"));

  // `closeAuthTab()` closes only the tracked tab, not the `window.open()` tab.
  ContinueJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 2; }));
  EXPECT_NE(tab_list->GetIndexOfTab(window_open_handle), -1);
}

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest,
                       testGeicSwitchAssistantNotAllowed) {
  base::HistogramTester histogram_tester;
  int relaunch_count = 0;
  auto reset_relaunch = SetRestartCallbackForTesting(
      base::BindLambdaForTesting([&]() { ++relaunch_count; }));

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(relaunch_count, 0);
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
      std::to_underlying(prefs::AssistantSelection::kAuto));
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.Unknown",
      mojom::SwitchAssistantResult::kErrorInvalidTarget, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.Unknown",
      mojom::SwitchAssistantResult::kErrorInvalidTarget, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

// On a consumer-latched profile with no switchable `GeminiEnterpriseUrl`
// policy, `HostCapability::kSwitchAssistant` is omitted and `SwitchAssistant()`
// calls resolve cleanly with `kErrorNotAllowed` without modifying prefs or
// restarting Chrome.
class GlicGeicApiConsumerBrowserTest : public GlicGeicApiBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip `GlicGeicApiBrowserTest::SetUpCommandLine()` so that no GEiC guest
    // URL is configured and the profile latches to consumer Gemini.
    GlicApiBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(GlicGeicApiConsumerBrowserTest,
                       testSwitchAssistantFromConsumer) {
  ASSERT_EQ(GlicProviderLatch::GetProviderForTesting(*GetProfile()),
            GlicProvider::kGemini);
  base::HistogramTester histogram_tester;
  int relaunch_count = 0;
  auto reset_relaunch = SetRestartCallbackForTesting(
      base::BindLambdaForTesting([&]() { ++relaunch_count; }));

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(relaunch_count, 0);
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
      std::to_underlying(prefs::AssistantSelection::kAuto));
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Consumer.SwitchAssistant.Result.GeminiEnterprise",
      mojom::SwitchAssistantResult::kErrorNotAllowed, 1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
class GlicGeicSwitchAssistantApiBrowserTest : public GlicGeicApiBrowserTest {
 public:
  GlicGeicSwitchAssistantApiBrowserTest() {
    // Must be configured before `GlicBrowserTest::SetUpCommandLine()` binds the
    // HTTPS test server.
    embedded_https_test_server().SetCertHostnames(
        {"business.gemini.google", "127.0.0.1"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Skip `GlicGeicApiBrowserTest::SetUpCommandLine()` so the guest URL comes
    // from the machine-scoped `GeminiEnterpriseUrl` policy rather than the
    // non-switchable `--geic-guest-url` command-line flag.
    GlicApiBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    GlicGeicApiBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    GURL::Replacements replacements;
    replacements.SetHostStr("business.gemini.google");
    GURL policy_guest_url = GetGuestURL().ReplaceComponents(replacements);
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(policy_guest_url.spec()), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    GlicGeicApiBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("business.gemini.google", "127.0.0.1");
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicSwitchAssistantApiBrowserTest,
                       testGeicSwitchAssistant) {
  ASSERT_EQ(GlicProviderLatch::GetProviderForTesting(*GetProfile()),
            GlicProvider::kGeminiEnterprise);
  std::optional<geic::GeicGuestUrl> resolved_url =
      geic::ResolveGeicGuestUrl(GetProfile());
  ASSERT_TRUE(resolved_url.has_value());
  ASSERT_TRUE(geic::ShouldLatchToGeminiEnterprise(
      GetProfile(), base::OptionalToPtr(resolved_url)));
  GetProfile()->GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey,
      resolved_url->url().spec());

  base::HistogramTester histogram_tester;
  int relaunch_count = 0;
  auto reset_relaunch = SetRestartCallbackForTesting(
      base::BindLambdaForTesting([&]() { ++relaunch_count; }));

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  EXPECT_EQ(relaunch_count, 1);
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
      std::to_underlying(prefs::AssistantSelection::kConsumer));
  EXPECT_FALSE(GetProfile()->GetPrefs()->HasPrefPath(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  resolved_url = geic::ResolveGeicGuestUrl(GetProfile());
  ASSERT_TRUE(resolved_url.has_value());
  EXPECT_FALSE(geic::ShouldLatchToGeminiEnterprise(
      GetProfile(), base::OptionalToPtr(resolved_url)));
  EXPECT_EQ(GlicEnabling::GetPendingProviderChange(GetProfile()),
            GlicProviderChange::kGeicToGemini);
  histogram_tester.ExpectUniqueSample(
      "Glic.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.SwitchAssistant.Result.GeminiConsumer",
      mojom::SwitchAssistantResult::kSuccess, 1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
// GEiC doesn't use the Chrome primary account: the GE web app handles sign-in,
// and there is no AuthController. Pausing Chrome sign-in leaves the GE guest
// showing, and the web client can still call the host API.
IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testSignInPauseStateGeic) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ExecuteJsTest();

  // Pause the sign-in.
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(GetProfile()));

  // The web client calls the host API again after the pause.
  ContinueJsTest();
  ASSERT_OK(WaitUntilGuestIsShowing());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
  EXPECT_TRUE(FindGlicGuestMainFrame());
}

class GlicGeicFreApiBrowserTest : public GlicGeicApiBrowserTest {
 public:
  GlicGeicFreApiBrowserTest() {
    geic_fre_feature_list_.InitAndEnableFeature(features::kGeicFre);
    // Start with neither FRE completed. The GEiC FRE ignores this, since it
    // can only be completed.
    glic_test_environment().SetFreStatusForNewProfiles(
        prefs::FreStatus::kNotStarted);
  }

 private:
  base::test::ScopedFeatureList geic_fre_feature_list_;
};

// The onboarding API completes the GEiC FRE and skips the consumer-only
// onboarding steps, but records provider-split onboarding metrics.
IN_PROC_BROWSER_TEST_F(GlicGeicFreApiBrowserTest,
                       testGeicSetOnboardingCompleted) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContinueJsTest();

  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  EXPECT_EQ(geic::GetFreStatus(GetProfile()),
            geic::prefs::GeicFreStatus::kCompleted);
  // The consumer FRE is untouched.
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"));
  histogram_tester.ExpectUniqueSample("Glic.Fre.Accept.FlowSource",
                                      OptInFlow::kGlicFre, 1);

  // `OpenGlicForActiveTab()` opens the panel from the top Chrome button.
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.Fre.Accept.InvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
  histogram_tester.ExpectTotalCount("Glic.Consumer.Fre.Accept.InvocationSource",
                                    0);
  histogram_tester.ExpectUniqueSample("Glic.Fre.Accept.InvocationSource",
                                      mojom::InvocationSource::kTopChromeButton,
                                      1);
}

}  // namespace
}  // namespace glic
