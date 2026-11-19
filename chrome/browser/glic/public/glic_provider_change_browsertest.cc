// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_change.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/glic_overlay.mojom.h"
#include "chrome/browser/glic/host/glic_provider_change_relaunch.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace glic {
namespace {

constexpr char kPolicyUrl[] = "https://business.gemini.google/home/cid/abc123";
constexpr char kCanonicalPolicyUrl[] =
    "https://business.gemini.google/side-panel?configId=abc123";
constexpr char kOtherPolicyUrl[] =
    "https://business.gemini.google/home/cid/xyz789";

class GlicProviderChangeBrowserTestBase : public GlicBrowserTest {
 public:
  explicit GlicProviderChangeBrowserTestBase(
      std::optional<std::string_view> initial_policy_url)
      : initial_policy_url_(initial_policy_url) {
    // GEiC always uses the NoWebview host.
    scoped_feature_list_.InitWithFeatures(
        {features::kGeic, features::kGeicProviderChangePrompt,
         features::kGlicNoWebview},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    GlicBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    UpdateGeminiEnterpriseUrlPolicy(initial_policy_url_);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    // Every test starts latched to the provider of the initial policy, with no
    // pending change.
    ASSERT_EQ(initial_policy_url_ ? GlicProvider::kGeminiEnterprise
                                  : GlicProvider::kGemini,
              GlicProviderLatch::GetProvider(*profile()));
    ASSERT_EQ(GlicProviderChange::kNone, pending_change());
    ASSERT_EQ(mojom::ProfileReadyState::kReady, ready_state());
  }

 protected:
  void UpdateGeminiEnterpriseUrlPolicy(std::optional<std::string_view> url) {
    policy::PolicyMap policies;
    if (url.has_value()) {
      policies.Set(policy::key::kGeminiEnterpriseUrl,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD, base::Value(*url), nullptr);
    }
    policy_provider_.UpdateChromePolicy(policies);
  }

  Profile* profile() { return GetProfile(); }

  GlicProviderChange pending_change() {
    return GlicEnabling::GetPendingProviderChange(profile());
  }

  mojom::ProfileReadyState ready_state() {
    return GlicEnabling::GetProfileReadyState(profile());
  }

  GlicNoWebviewContentsManager* GetContentsManager(GlicInstanceImpl* instance) {
    return static_cast<GlicNoWebviewContentsManager*>(
        instance->host().contents_manager());
  }

 private:
  std::optional<std::string_view> initial_policy_url_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicProviderChangeFromGeminiBrowserTest
    : public GlicProviderChangeBrowserTestBase {
 public:
  GlicProviderChangeFromGeminiBrowserTest()
      : GlicProviderChangeBrowserTestBase(std::nullopt) {}
};

class GlicProviderChangeFromGeicBrowserTest
    : public GlicProviderChangeBrowserTestBase {
 public:
  GlicProviderChangeFromGeicBrowserTest()
      : GlicProviderChangeBrowserTestBase(kPolicyUrl) {}
};

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       AddingPolicyRequiresRelaunchAndRevertClearsIt) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  EXPECT_EQ(GlicProviderChange::kGeminiToGeic, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());

  // Reverting the policy clears the pending change and restores kReady.
  UpdateGeminiEnterpriseUrlPolicy(std::nullopt);
  EXPECT_EQ(GlicProviderChange::kNone, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());
}

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       MaybeRelaunchIsNoOpWhenNoPendingChange) {
  base::UserActionTester user_action_tester;
  MaybeRelaunchForProviderChange(profile());
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Glic.ProviderChange.RelaunchClicked"));
}

// A pending change doesn't affect the profile ready state, which keeps
// describing the latched provider.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       PendingProviderChangeDoesNotAffectReadyState) {
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  ASSERT_EQ(mojom::ProfileReadyState::kDisabledByAdmin, ready_state());

  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  EXPECT_EQ(GlicProviderChange::kGeminiToGeic, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kDisabledByAdmin, ready_state());
}

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeicBrowserTest,
                       RemovingPolicyRequiresRelaunch) {
  UpdateGeminiEnterpriseUrlPolicy(std::nullopt);
  EXPECT_EQ(GlicProviderChange::kGeicToGemini, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());
}

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeicBrowserTest,
                       ChangingToOtherGeicInstanceRequiresRelaunch) {
  UpdateGeminiEnterpriseUrlPolicy(kOtherPolicyUrl);
  EXPECT_EQ(GlicProviderChange::kGeicToOtherGeic, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());
}

// Selecting consumer Gemini latches Gemini after a relaunch.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeicBrowserTest,
                       SelectingConsumerRequiresRelaunch) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicAssistantSelection,
      std::to_underlying(prefs::AssistantSelection::kConsumer));
  EXPECT_EQ(GlicProviderChange::kGeicToGemini, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());
}

IN_PROC_BROWSER_TEST_F(
    GlicProviderChangeFromGeicBrowserTest,
    ChangingToCanonicalFormOfSameInstanceDoesNotRequireRelaunch) {
  UpdateGeminiEnterpriseUrlPolicy(kCanonicalPolicyUrl);
  EXPECT_EQ(GlicProviderChange::kNone, pending_change());
  EXPECT_EQ(mojom::ProfileReadyState::kReady, ready_state());
}

// A pending change doesn't affect an existing instance: it keeps working with
// the latched provider until Chrome relaunches, including after the side panel
// is closed and reopened.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       PendingProviderChangeDoesNotInterruptSession) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  PreventDeletionOnClose(instance);
  GlicNoWebviewContentsManager* manager = GetContentsManager(instance);
  ASSERT_TRUE(manager);

  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  EXPECT_EQ(std::nullopt, manager->error_type());

  ASSERT_OK(CloseGlicForTabAndWait(tab));
  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  ASSERT_EQ(manager, GetContentsManager(instance));
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_EQ(std::nullopt, manager->error_type());
}

// An instance created before the provider change keeps the latched provider,
// including when an error panel clears after the change.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       ExistingInstanceReloadsGuestWhenErrorClears) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  // Consumer Gemini is disabled by admin, so the panel shows that error.
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  ASSERT_OK(WaitForOverlayErrorPanel(mojom::ErrorPanelType::kDisabledByAdmin,
                                     instance));

  // Re-enabling it with a change pending clears the error and loads the guest.
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  EXPECT_OK(WaitForErrorPanelType(std::nullopt, instance));
  EXPECT_OK(WaitUntilGuestIsShowing(instance));
}

// The warming pool refills after every open, so contents can be warmed before
// the provider change. A new instance with a pending change still uses them,
// and shows the relaunch panel when it adopts them.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       NewInstanceShowsRelaunchPanelOnWarmedContents) {
  ASSERT_OK(OpenGlicForActiveTab());

  // Warm a container before the provider changes, like the refill the pool
  // schedules after every open.
  GlicWebContentsWarmingPool& pool =
      coordinator().GetWebContentsWarmingPoolForTesting();
  ASSERT_TRUE(pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  GlicWebContentsManager* warmed = pool.GetWarmedContainerForTesting();
  ASSERT_TRUE(warmed);

  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  EXPECT_EQ(std::nullopt,
            static_cast<GlicNoWebviewContentsManager*>(warmed)->error_type());

  // A new instance on another tab adopts the warmed contents.
  ASSERT_TRUE(CreateUserInitiatedTab(GURL(url::kAboutBlankURL)));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  EXPECT_EQ(warmed, instance->host().contents_manager());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));
}

// An instance created before the provider change keeps the latched provider
// even when it gets new contents, e.g. after being hibernated.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       ExistingInstanceKeepsSessionAfterHibernation) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  PreventDeletionOnClose(instance);
  ASSERT_OK(CloseGlicForTabAndWait(tab));

  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  instance->Hibernate();
  ASSERT_TRUE(instance->IsHibernated());

  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_EQ(std::nullopt, GetContentsManager(instance)->error_type());
}

// A new instance checks for a pending change and shows the relaunch panel.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       OpeningGlicShowsGeicAddedPanel) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));
}

// Ready state updates while the relaunch panel is showing (e.g. from an
// unrelated pref change) don't clear it and load the latched provider.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       RelaunchPanelPersistsAcrossReadyStateUpdates) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));

  // Toggling GeminiSettings notifies ready state observers twice, ending in
  // kReady.
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  ASSERT_EQ(mojom::ProfileReadyState::kReady, ready_state());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));
}

// The relaunch panel takes priority over profile errors, since a relaunch
// replaces the provider they describe.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeminiBrowserTest,
                       RelaunchPanelTakesPriorityOverProfileErrors) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));

  // Consumer Gemini disabled by admin is a profile error for the latched
  // provider, but the relaunch panel stays.
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  ASSERT_EQ(mojom::ProfileReadyState::kDisabledByAdmin, ready_state());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic, instance));
}

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeicBrowserTest,
                       OpeningGlicShowsGeicRemovedPanel) {
  UpdateGeminiEnterpriseUrlPolicy(std::nullopt);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeicToGemini, instance));
}

IN_PROC_BROWSER_TEST_F(GlicProviderChangeFromGeicBrowserTest,
                       OpeningGlicShowsOtherGeicPanel) {
  UpdateGeminiEnterpriseUrlPolicy(kOtherPolicyUrl);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  EXPECT_OK(WaitForOverlayErrorPanel(
      mojom::ErrorPanelType::kPendingProviderChangeGeicToOtherGeic, instance));
}

// The webview host only serves Gemini-latched profiles (GEiC requires
// NoWebview), so the only change it can see is Gemini to GEiC.
class GlicProviderChangeWebviewBrowserTest
    : public GlicProviderChangeFromGeminiBrowserTest {
 public:
  GlicProviderChangeWebviewBrowserTest() {
    webview_feature_list_.InitAndDisableFeature(features::kGlicNoWebview);
  }

 protected:
  // `WaitForLoad()` goes over the same `Page` pipe as `IntentToShow()`, so it's
  // answered after the page handles it. A page that reloaded the client
  // reports success; one still showing the relaunch panel reports a failure.
  // The page reports its state on a different pipe, so wait for it.
  void ExpectRelaunchPanelAfterIntentToShow(GlicInstanceImpl* instance) {
    base::test::TestFuture<bool> failed;
    instance->host().WaitForLoad(failed.GetCallback());
    EXPECT_TRUE(failed.Get());
    EXPECT_TRUE(base::test::RunUntil([&] {
      return instance->host().GetPrimaryWebUiState() ==
             mojom::WebUiState::kPendingProviderChangeGeminiToGeic;
    }));
  }

 private:
  base::test::ScopedFeatureList webview_feature_list_;
};

// The WebUI is told to show the relaunch panel when an instance created with a
// pending change adopts it.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeWebviewBrowserTest,
                       OpeningGlicShowsRelaunchPanel) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);

  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_OK(
      WaitForWebUiState(mojom::WebUiState::kPendingProviderChangeGeminiToGeic));
}

// Opening on warmed WebUI contents sends `IntentToShow()` right after
// `ShowPendingProviderChange()`, which must not reload the stale client.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeWebviewBrowserTest,
                       WarmedContentsKeepRelaunchPanel) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicWebContentsWarmingPool& pool =
      coordinator().GetWebContentsWarmingPoolForTesting();
  ASSERT_TRUE(pool.MaybeStartWarming(GlicWarmingTrigger::kStartup));
  GlicWebContentsManager* warmed = pool.GetWarmedContainerForTesting();
  ASSERT_TRUE(warmed);

  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);

  ASSERT_TRUE(CreateUserInitiatedTab(GURL(url::kAboutBlankURL)));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_EQ(warmed, instance->host().contents_manager());
  ExpectRelaunchPanelAfterIntentToShow(instance);
}

// Closing and reopening sends `IntentToShow()`, which must not reload the
// stale client.
IN_PROC_BROWSER_TEST_F(GlicProviderChangeWebviewBrowserTest,
                       ReopeningKeepsRelaunchPanel) {
  UpdateGeminiEnterpriseUrlPolicy(kPolicyUrl);
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(
      WaitForWebUiState(mojom::WebUiState::kPendingProviderChangeGeminiToGeic));
  PreventDeletionOnClose(instance);

  ASSERT_OK(CloseGlicForTabAndWait(tab));
  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  ExpectRelaunchPanelAfterIntentToShow(instance);
}

}  // namespace
}  // namespace glic
