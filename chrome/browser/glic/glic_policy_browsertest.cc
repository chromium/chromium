// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom-data-view.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"  // nogncheck
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using glic::prefs::GlicActuationOnWebPolicyState;
using glic::prefs::kGlicActuationOnWeb;
using glic::prefs::SettingsPolicyState;
using ::prefs::kGeminiSettings;

using policy::PolicyTest;

namespace glic {

class GlicButton;

namespace {

int ToInt(GlicActuationOnWebPolicyState state) {
  return static_cast<int>(state);
}

// An observer of the GlicWindowController's panel state. Fires the given
// callback when the state changes to the given kind.
class PanelStateObserver : public GlicWindowController::StateObserver {
 public:
  PanelStateObserver(mojom::PanelStateKind kind, base::OnceClosure callback)
      : kind_(kind), callback_(std::move(callback)) {}

  void PanelStateChanged(
      const mojom::PanelState& panel_state,
      const GlicWindowController::PanelStateContext& context) override {
    if (panel_state.kind == kind_) {
      std::move(callback_).Run();
    }
  }

 private:
  mojom::PanelStateKind kind_;
  base::OnceClosure callback_;
};

class GlicAppStateObserver : public Host::Observer {
 public:
  explicit GlicAppStateObserver(Host* host)
      : GlicAppStateObserver(host, host->GetPrimaryWebUiState()) {}

  explicit GlicAppStateObserver(Host* host, mojom::WebUiState initial_state) {
    observation_.Observe(host);
    state_ = initial_state;
  }

  ~GlicAppStateObserver() override { observation_.Reset(); }

  void Wait(mojom::WebUiState state) {
    waiting_for_state_ = state;
    if (state_ == waiting_for_state_) {
      return;
    }
    // Run run_loop until the state_ == waiting_for_state_.
    run_loop_.Run();
  }

  void WebUiStateChanged(mojom::WebUiState state) override {
    state_ = state;
    if (state_ != waiting_for_state_) {
      return;
    }
    run_loop_.Quit();
    observation_.Reset();
  }

 private:
  base::ScopedObservation<Host, Host::Observer> observation_{this};
  mojom::WebUiState state_ = mojom::WebUiState::kUninitialized;
  mojom::WebUiState waiting_for_state_ = mojom::WebUiState::kUninitialized;
  base::RunLoop run_loop_;
};

class GlicPolicyTest : public PolicyTest {
 public:
  GlicPolicyTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlic,
          {
              // This test currently loads about:blank instead of a client which
              // could ever reach the kReady state. To speed that up, cut down
              // the time we wait for it.
              {features::kGlicMaxLoadingTimeMs.name, "500"},
          }}},
        {});
  }

  GlicPolicyTest(const GlicPolicyTest&) = delete;
  GlicPolicyTest& operator=(const GlicPolicyTest&) = delete;

  ~GlicPolicyTest() override = default;

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    PolicyTest::SetUpLocalStatePrefService(local_state);

    // Register two users.
    user_manager::TestHelper::RegisterPersistedUser(*local_state, kAccountId1);
    user_manager::TestHelper::RegisterPersistedUser(*local_state, kAccountId2);
  }
#endif

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);

    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");

#if BUILDFLAG(IS_CHROMEOS)
    // Log-in with the first user.
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    kAccountId1.GetUserEmail());
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
#endif
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, true);

    profile_1_ = browser()->profile();
    instance_tracker_.SetProfile(profile_1_);

    // "policy_for_profile_1_" is provider_, setup in PolicyTest.
    // Creating multi-profiles in a single user session is prohibited.
    {
      // The policy configuration here causes signin::WaitForRefreshTokensLoaded
      // to hang when run from GlicTestEnvironmentFactory, so disable it here
      // and run ForceSigninAndModelExecutionCapability() directly afterward.

      glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
      policy_for_profile_2_.SetDefaultReturns(
          /*is_initialization_complete_return=*/true,
          /*is_first_policy_load_complete_return=*/true);
      policy::PushProfilePolicyConnectorProviderForTesting(
          &policy_for_profile_2_);

#if BUILDFLAG(IS_CHROMEOS)
      // ChromeOS does not support multi profile, but multi-user signin.
      // I.e., we cannot create multiple Profile instances within a user
      // session.
      // Here we create another user session corresponding to another Profile.
      // The tests below with multi profiles make sense for multi-user
      // cases in ChromeOS conceptually, too.
      const std::string userhash2 =
          user_manager::TestHelper::GetFakeUsernameHash(kAccountId2);
      session_manager::SessionManager::Get()->CreateSession(
          kAccountId2, userhash2,
          /*new_user=*/false,
          /*has_active_session=*/false);

      // Set up the secondary profile.
      base::FilePath user_data_directory;
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
      base::FilePath profile_dir = user_data_directory.AppendASCII(
          ash::BrowserContextHelper::GetUserBrowserContextDirName(userhash2));
      profile_2_ =
          g_browser_process->profile_manager()->GetProfile(profile_dir);
      ASSERT_EQ(kAccountId2, *ash::AnnotatedAccountId::Get(profile_2_.get()));
#else
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      base::FilePath new_path =
          profile_manager->GenerateNextProfileDirectoryPath();
      profile_2_ =
          &profiles::testing::CreateProfileSync(profile_manager, new_path);
#endif  // BUILDFLAG(IS_CHROMEOS)
      ForceSigninAndModelExecutionCapability(profile_2_);
    }
  }

  void TearDownOnMainThread() override {
    PolicyTest::TearDownOnMainThread();

    instance_tracker_.SetProfile(nullptr);
    if (GlicBackgroundModeManager* background_mode_manager =
            g_browser_process->GetFeatures()->glic_background_mode_manager()) {
      background_mode_manager->ExitBackgroundMode();
    }
    profile_1_ = nullptr;
    profile_2_ = nullptr;
  }

  GlicButton* GetGlicButtonForBrowser(Browser* browser) {
    return BrowserElementsViews::From(browser)->GetViewAs<glic::GlicButton>(
        kGlicButtonElementId);
  }

  void SetGlicPolicy(
      testing::NiceMock<policy::MockConfigurationPolicyProvider>& provider,
      SettingsPolicyState value) {
    using policy::POLICY_LEVEL_MANDATORY;
    using policy::POLICY_SCOPE_USER;
    using policy::POLICY_SOURCE_ENTERPRISE_DEFAULT;
    using policy::PolicyMap;
    using policy::key::kGeminiSettings;

    PolicyMap policies;
    policies.Set(kGeminiSettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_ENTERPRISE_DEFAULT,
                 base::Value(static_cast<int>(value)), nullptr);
    provider.UpdateChromePolicy(policies);
  }

  // Simulates a click on an element with the given |id|.
  void ClickElementWithId(content::WebContents* web_contents,
                          const std::string& id) {
    // Get the center coordinates of the DOM element.
    const int x =
        EvalJs(web_contents,
               content::JsReplace("const bounds = "
                                  "document.getElementById($1)."
                                  "getBoundingClientRect();"
                                  "Math.floor(bounds.left + bounds.width / 2)",
                                  id))
            .ExtractInt();
    const int y =
        EvalJs(web_contents,
               content::JsReplace("const bounds = "
                                  "document.getElementById($1)."
                                  "getBoundingClientRect();"
                                  "Math.floor(bounds.top + bounds.height / 2)",
                                  id))
            .ExtractInt();
    SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                         gfx::Point(x, y));
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>&
  policy_for_profile_1() {
    // This comes from the PolicyTest base class.
    return provider_;
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>&
  policy_for_profile_2() {
    return policy_for_profile_2_;
  }

  base::expected<mojom::WebUiState, std::string> GetWebUIStateForActiveTab() {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    if (!tab) {
      return base::unexpected("no active tab");
    }
    content::BrowserContext* browser_context =
        tab->GetContents()->GetBrowserContext();

    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser_context);
    Host* host = service->host_manager().FindHostForTabForTesting(
        *browser()->tab_strip_model()->GetActiveTab());

    if (!host) {
      return base::unexpected("no host for tab");
    }
    return base::ok(host->GetPrimaryWebUiState());
  }

  [[nodiscard]] base::expected<void, std::string> RunUntilWebUIState(
      mojom::WebUiState state) {
    if (GetWebUIStateForActiveTab() == state) {
      return base::ok();
    }
    bool ok = base::test::RunUntil([&]() {
      auto current = GetWebUIStateForActiveTab();
      return current.has_value() && current.value() == state;
    });
    if (ok) {
      return base::ok();
    }
    auto current_state = GetWebUIStateForActiveTab();
    std::stringstream ss;
    ss << "Waiting until WebUI state equals " << state << ", current state is "
       << *current_state;
    return base::unexpected(ss.str());
  }

 protected:
  // Get the active tab's glic host. Must be called only after instantiating
  // glic.
  Host* GetHost() { return instance_tracker_.GetHost(); }
  GlicInstance* GetGlicInstance() {
    return instance_tracker_.GetGlicInstance();
  }

  // The first profile.
  raw_ptr<Profile> profile_1_;
  // The second profile.
  raw_ptr<Profile> profile_2_;

  static constexpr int kEnabledValue =
      static_cast<int>(SettingsPolicyState::kEnabled);
  static constexpr int kDisabledValue =
      static_cast<int>(SettingsPolicyState::kDisabled);

 private:
#if BUILDFLAG(IS_CHROMEOS)
  static constexpr auto kAccountId1 =
      AccountId::Literal::FromUserEmailGaiaId("test1@test",
                                              GaiaId::Literal("123456789"));
  static constexpr auto kAccountId2 =
      AccountId::Literal::FromUserEmailGaiaId("test2@test",
                                              GaiaId::Literal("987654321"));
#endif  // BUILDFLAG(IS_CHROMEOS)

  GlicTestEnvironment glic_test_environment_;

  GlicInstanceTracker instance_tracker_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      policy_for_profile_2_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PrefDefaultsToEnabled) {
  // The pref defaults to enabled.
  EXPECT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
  EXPECT_EQ(kEnabledValue, profile_2_->GetPrefs()->GetInteger(kGeminiSettings));
}

IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PrefDisabledByPolicy) {
  // By default the pref should start off unmanaged and defaulted to enabled.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(kGeminiSettings));
  EXPECT_EQ(kEnabledValue, prefs->GetInteger(kGeminiSettings));

  // Verify that policy can force-disable Glic.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  EXPECT_TRUE(prefs->IsManagedPreference(kGeminiSettings));
  EXPECT_EQ(kDisabledValue, prefs->GetInteger(kGeminiSettings));

  // Verify the policy value cannot be overridden.
  prefs->SetInteger(kGeminiSettings, kEnabledValue);
  EXPECT_EQ(kDisabledValue, prefs->GetInteger(kGeminiSettings));
}

// Ensure that when policy disables Glic, a browser window doesn't show the Glic
// button.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyAffectsGlicButtonInNewWindows) {
  ASSERT_EQ(browser()->profile(), profile_1_);
  ASSERT_NE(profile_1_, profile_2_);

  // Disable the policy in the default profile.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  {
    // A new window in profile 1 shouldn't have the Glic button.
    Browser* new_window_profile_1 = CreateBrowser(profile_1_);
    EXPECT_FALSE(GetGlicButtonForBrowser(new_window_profile_1)->GetVisible());

    // A new window in profile 2 should continue to have the Glic button since
    // only profile 1 disabled Glic.
    Browser* new_window_profile_2 = CreateBrowser(profile_2_);
    EXPECT_TRUE(GetGlicButtonForBrowser(new_window_profile_2)->GetVisible());
  }

  // Re-enable the policy. Ensure the button is recreated.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  {
    // A new window in profile 1 should again get the Glic button now that the
    // policy is re-enabled.
    Browser* new_window_profile_1 = CreateBrowser(profile_1_);
    EXPECT_TRUE(GetGlicButtonForBrowser(new_window_profile_1)->GetVisible());
  }
}

// Ensure that when policy disables Glic, a browser window doesn't show the Glic
// button.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, GlicButtonInExistingWindows) {
  ASSERT_EQ(browser()->profile(), profile_1_);
  ASSERT_NE(profile_1_, profile_2_);

  // Create two windows in each profile.
  Browser* profile_1_window_1 = browser();
  Browser* profile_1_window_2 = CreateBrowser(profile_1_);
  Browser* profile_2_window_1 = CreateBrowser(profile_2_);
  Browser* profile_2_window_2 = CreateBrowser(profile_2_);

  // Ensure the button was created in each window.
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_1)->GetVisible());
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_2)->GetVisible());
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1)->GetVisible());
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2)->GetVisible());

  // Disable the policy in the first profile.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  {
    // The windows in profile 1 should have lost their Glic button.
    EXPECT_FALSE(GetGlicButtonForBrowser(profile_1_window_1)->GetVisible());
    EXPECT_FALSE(GetGlicButtonForBrowser(profile_1_window_2)->GetVisible());

    // The windows in profile 2 should have kept their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1)->GetVisible());
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2)->GetVisible());
  }

  // Re-enable the policy. Ensure the button is recreated.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  {
    // The windows in profile 1 should get back their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_1)->GetVisible());
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_2)->GetVisible());

    // The windows in profile 2 still have their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1)->GetVisible());
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2)->GetVisible());
  }
}

// Ensure that background mode is entered if and only if a profile with the
// policy enabled is loaded.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyDisablesBackgroundMode) {
  ASSERT_EQ(browser()->profile(), profile_1_);
  ASSERT_NE(profile_1_, profile_2_);

  Browser* new_window_profile_2 = CreateBrowser(profile_2_);
  ASSERT_TRUE(new_window_profile_2);

  GlicBackgroundModeManager* background_mode_manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());

  // Disable the policy in the default profile.
  {
    SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
    ASSERT_EQ(kDisabledValue,
              profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
    ASSERT_EQ(kEnabledValue,
              profile_2_->GetPrefs()->GetInteger(kGeminiSettings));
  }

  // Background mode should remain active since profile_2_ still has it enabled.
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());

  // Disable the policy in the second profile.
  {
    SetGlicPolicy(policy_for_profile_2(), SettingsPolicyState::kDisabled);
    ASSERT_EQ(kDisabledValue,
              profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
    ASSERT_EQ(kDisabledValue,
              profile_2_->GetPrefs()->GetInteger(kGeminiSettings));
  }

  // Background mode should be exited since none of the loaded profiles enable
  // Glic.
  EXPECT_FALSE(background_mode_manager->IsInBackgroundModeForTesting());

  // Enable the policy in the default profile again.
  {
    SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
    ASSERT_EQ(kEnabledValue,
              profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
    ASSERT_EQ(kDisabledValue,
              profile_2_->GetPrefs()->GetInteger(kGeminiSettings));
  }

  // Background mode should be reentered since the first profile is enabled.
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());
}

auto IsOk() -> auto {
  return testing::Property(&base::expected<void, std::string>::has_value,
                           testing::IsTrue());
}

// Ensure navigating to chrome://glic is enabled only if the policy is enabled.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyDisablesWebUi) {
  GURL glic_url = GURL(chrome::kChromeUIGlicURL);

  // Navigating to chrome://glic should succeed.
  {
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    ASSERT_THAT(RunUntilWebUIState(mojom::WebUiState::kError), IsOk());
  }

  // Disable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            browser()->profile()->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigate to chrome://glic. The glic page should be unavailable.
  {
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    ASSERT_THAT(RunUntilWebUIState(mojom::WebUiState::kDisabledByAdmin),
                IsOk());
  }

  // Re-enable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigating to chrome://glic should now succeed again.
  {
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    ASSERT_THAT(RunUntilWebUIState(mojom::WebUiState::kError), IsOk());
  }
}

// Same as GlicPolicyTest but starts Chrome with the Glic policy disabled.
class GlicPolicyDisabledTest : public GlicPolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    GlicPolicyTest::SetUpInProcessBrowserTestFixture();
    SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  }
};

// Test that navigations to chrome://glic when the policy is disabled from
// startup shows glic as unavailable.
IN_PROC_BROWSER_TEST_F(GlicPolicyDisabledTest, WebUiDisabledAtLoad) {
  GURL glic_url = GURL(chrome::kChromeUIGlicURL);

  // Glic shouldn't load since it's disabled by policy from startup.
  {
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());

    auto state = GetWebUIStateForActiveTab();
    ASSERT_THAT(RunUntilWebUIState(mojom::WebUiState::kDisabledByAdmin),
                IsOk());
  }

  // Enable the policy at runtime
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigating to chrome://glic should now load the webview.
  {
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    ASSERT_THAT(RunUntilWebUIState(mojom::WebUiState::kError), IsOk());
  }
}

// Ensure that if the policy changes to disabled at runtime, and the user has an
// an open Glic window, that window should show the unavailable page.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, DisableGlicWhenIsOpen) {
  // The pref defaults to enabled.
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_1_);

  GlicInstanceTracker instance_tracker(profile_1_);
  // Show the panel as if the glic button was clicked.
  {
    service->ToggleUI(/*bwi=*/browser(), /*prevent_close=*/false,
                      /*source=*/mojom::InvocationSource::kOsButton);

    ASSERT_TRUE(instance_tracker.WaitForShow());
  }

  ASSERT_TRUE(GetGlicInstance());
  ASSERT_TRUE(GetGlicInstance()->IsShowing());
  Host* host = GetHost();
  GlicAppStateObserver app_observer(host);
  app_observer.Wait(mojom::WebUiState::kError);

  // Disable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return host->GetPrimaryWebUiState() == mojom::WebUiState::kDisabledByAdmin;
  })) << "Timed out waiting for unavailable state. Current state: "
      << host->GetPrimaryWebUiState();

  ASSERT_TRUE(GetGlicInstance());
  ASSERT_TRUE(GetGlicInstance()->IsShowing());

// Flakiness on linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // TODO(crbug.com/426583248) Wait for animation to finish instead of using the
  // arbitrary 1000ms wait.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1000));
  run_loop.Run();
  ClickElementWithId(GetHost()->webui_contents(), "disabledByAdminCloseButton");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !GetGlicInstance()->IsShowing();
  })) << "Timed out waiting for glic to close";
#endif
}

// Ensure the chrome://settings page for Glic is available when the feature is
// disabled by policy (but the profile is otherwise eligible, completed FRE
// etc).
IN_PROC_BROWSER_TEST_F(GlicPolicyTest,
                       SettingsPageAvailableWithPolicyDisabled) {
  // Disable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigate to the Glic settings page URL.
  const GURL kGlicSettingsUrl =
      chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage);
  content::TestNavigationObserver observer(kGlicSettingsUrl);
  observer.WatchExistingWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kGlicSettingsUrl));
  observer.WaitForNavigationFinished();
  ASSERT_TRUE(observer.last_navigation_succeeded());

  // If the settings page wasn't registered, the navigation will redirect to
  // chrome://settings.
  EXPECT_EQ(kGlicSettingsUrl,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

class GlicActuationOnWebPolicyTest : public GlicPolicyTest {
 public:
  GlicActuationOnWebPolicyTest() {
    // The default pref value kForcedDisabled does not allow the policy to
    // change the pref value.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorEnterprisePrefDefault.name,
          features::kGlicActorEnterprisePrefDefault.GetName(
              features::GlicActorEnterprisePrefDefault::kEnabledByDefault)}});
  }
  ~GlicActuationOnWebPolicyTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActuationOnWebPolicyTest, DefaultToEnabled) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(kGlicActuationOnWeb));
  EXPECT_EQ(prefs->GetInteger(kGlicActuationOnWeb),
            ToInt(GlicActuationOnWebPolicyState::kEnabled));
}

IN_PROC_BROWSER_TEST_F(GlicActuationOnWebPolicyTest, PrefControlledByPolicy) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  ASSERT_EQ(prefs->GetInteger(kGlicActuationOnWeb),
            ToInt(GlicActuationOnWebPolicyState::kEnabled));

  // Set the policy to kDisabled. The pref value should be updated.
  policy::PolicyMap policies;
  policies.Set(
      policy::key::kGeminiActOnWebSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
      base::Value(ToInt(GlicActuationOnWebPolicyState::kDisabled)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(kGlicActuationOnWeb));
  EXPECT_EQ(prefs->GetInteger(kGlicActuationOnWeb),
            ToInt(GlicActuationOnWebPolicyState::kDisabled));

  // Verify the policy value cannot be overridden.
  prefs->SetInteger(kGlicActuationOnWeb,
                    ToInt(GlicActuationOnWebPolicyState::kEnabled));
  EXPECT_EQ(prefs->GetInteger(kGlicActuationOnWeb),
            ToInt(GlicActuationOnWebPolicyState::kDisabled));
}

}  // namespace

}  // namespace glic
