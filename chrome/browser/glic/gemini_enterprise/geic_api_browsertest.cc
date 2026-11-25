// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/glic_relaunch.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/provider_features.h"
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
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include <optional>

#include "base/types/optional_util.h"
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
  // TODO(b/590221698): Enable on Android. `AUTO_TOPLEVEL` still maps to
  // `FROM_CHROME_UI` -> `MaybeDaisyChainNewTab()` on Desktop Android, so the
  // auth tab still daisy-chains there.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GetInstanceForTab(auth_tab), nullptr);
  EXPECT_FALSE(GetOnlyGlicInstance()->GetSharingManager()->IsTabPinned(
      auth_tab->GetHandle()));
#endif  // !BUILDFLAG(IS_ANDROID)
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

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest,
                       testGeicRolloutStagesSuppressedBySwitch) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// Configures GEiC via Finch (`kGeicGuestURL` + `kGeicSelectableStages`) without
// `--geic-guest-url` so the rollout stage switcher API is active end-to-end.
class GlicGeicRolloutStageApiBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicRolloutStageApiBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {
    const std::string test_path_and_query = base::StrCat(
        {"/glic/browser_tests/test.html?test=",
         url::EncodeUriComponent(
             ::testing::UnitTest::GetInstance()->current_test_info()->name()),
         "&testsrc=", url::EncodeUriComponent("./geic_api_browsertest.js")});
    const std::string autopush_url = base::StrCat(
        {"https://business-autopush.gemini.google", test_path_and_query});
    const std::string staging_url = base::StrCat(
        {"https://business-staging.gemini.google", test_path_and_query});
    const std::string prod_url =
        base::StrCat({"https://business.gemini.google", test_path_and_query});

    base::ListValue stages_list;
    for (const auto& [id, label, url] : std::initializer_list<
             std::tuple<const char*, const char*, const std::string&>>{
             {"autopush", "Autopush", autopush_url},
             {"staging", "Staging", staging_url},
             {"prod", "Prod", prod_url},
         }) {
      base::DictValue stage;
      stage.Set("id", id);
      stage.Set("label", label);
      stage.Set("url", url);
      stages_list.Append(std::move(stage));
    }
    const std::string stages_json = base::WriteJson(stages_list).value_or("[]");

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{features::kGeicGuestURL.name, staging_url},
           {features::kGeicSelectableStages.name, stages_json}}}},
        {});
    embedded_https_test_server().SetCertHostnames(
        {"business-autopush.gemini.google", "business-staging.gemini.google",
         "business.gemini.google", "127.0.0.1"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    const int port = embedded_https_test_server().port();
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf("MAP business-autopush.gemini.google 127.0.0.1:%d, "
                           "MAP business-staging.gemini.google 127.0.0.1:%d, "
                           "MAP business.gemini.google 127.0.0.1:%d",
                           port, port, port));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicRolloutStageApiBrowserTest,
                       testGeicRolloutStages) {
  int restart_count = 0;
  auto restart_override = SetRestartCallbackForTesting(
      base::BindRepeating([](int* count) { ++(*count); }, &restart_count));

  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  // Unknown stage selection neither writes the pref nor restarts Chrome.
  EXPECT_EQ(restart_count, 0);
  EXPECT_TRUE(GetProfile()
                  ->GetPrefs()
                  ->GetString(geic::prefs::kGeicSelectedStageId)
                  .empty());

  ContinueJsTest();

  // Selecting the already-latched stage saves the pref without restarting.
  EXPECT_EQ(restart_count, 0);
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetString(geic::prefs::kGeicSelectedStageId),
      "staging");

  ContinueJsTest();

  // Selecting a different stage writes the pref and invokes the restart
  // callback.
  EXPECT_EQ(restart_count, 1);
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetString(geic::prefs::kGeicSelectedStageId),
      "autopush");
}

class GlicGeicPinOnBindApiBrowserTest : public GlicGeicApiBrowserTest {
 public:
  GlicGeicPinOnBindApiBrowserTest() {
    pin_on_bind_feature_list_.InitAndEnableFeature(features::kGeicPinOnBind);
  }

 private:
  base::test::ScopedFeatureList pin_on_bind_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicPinOnBindApiBrowserTest,
                       testGeicCreateTabPinOnBind) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* original_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(original_tab);
  EXPECT_TRUE(instance->GetSharingManagerInternal().IsTabPinned(
      original_tab->GetHandle()));

  // Calling `createTab()` pins the new tab when `kGeicPinOnBind` is enabled.
  ExecuteJsTest();
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);
  tabs::TabInterface* pinned_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(pinned_tab);
  EXPECT_NE(pinned_tab, original_tab);
  EXPECT_EQ(GetInstanceForTab(pinned_tab), instance);
  EXPECT_TRUE(instance->GetSharingManagerInternal().IsTabPinned(
      pinned_tab->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicGeicPinOnBindApiBrowserTest,
                       testGeicLinkClickPinOnBind) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* original_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(original_tab);
  EXPECT_TRUE(instance->GetSharingManagerInternal().IsTabPinned(
      original_tab->GetHandle()));

  ExecuteJsTest();
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);
  tabs::TabInterface* new_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(new_tab);
  EXPECT_NE(new_tab, original_tab);
  EXPECT_EQ(GetInstanceForTab(new_tab), instance);
  EXPECT_TRUE(
      instance->GetSharingManagerInternal().IsTabPinned(new_tab->GetHandle()));
}

// TODO(b/590221698): Enable on Android. `AUTO_TOPLEVEL` still maps to
// `FROM_CHROME_UI` -> `MaybeDaisyChainNewTab()` on Desktop Android, so the
// auth tab still daisy-chains there.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicGeicPinOnBindApiBrowserTest,
                       testGeicAuthTabDoesNotPin) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* original_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(original_tab);
  EXPECT_TRUE(instance->GetSharingManagerInternal().IsTabPinned(
      original_tab->GetHandle()));

  // Step 1: opening a sign-in tab while kGeicPinOnBind is enabled must neither
  // daisy-chain/bind it nor auto-pin it (nor create a second instance).
  ExecuteJsTest();
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);
  tabs::TabInterface* signin_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(signin_tab);
  EXPECT_NE(signin_tab, original_tab);
  EXPECT_EQ(GetOnlyGlicInstance(), instance);
  EXPECT_EQ(GetInstanceForTab(signin_tab), nullptr);
  EXPECT_FALSE(instance->GetSharingManagerInternal().IsTabPinned(
      signin_tab->GetHandle()));

  // Step 2: closing the sign-in tab restores focus to `original_tab`; opening a
  // connector OAuth tab from it must also neither bind nor auto-pin.
  ContinueJsTest();
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 2);
  tabs::TabInterface* oauth_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(oauth_tab);
  EXPECT_NE(oauth_tab, original_tab);
  EXPECT_EQ(GetOnlyGlicInstance(), instance);
  EXPECT_EQ(GetInstanceForTab(oauth_tab), nullptr);
  EXPECT_FALSE(instance->GetSharingManagerInternal().IsTabPinned(
      oauth_tab->GetHandle()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace glic
