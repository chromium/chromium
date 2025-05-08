// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using glic::prefs::SettingsPolicyState;
using ::prefs::kGeminiSettings;

using policy::PolicyTest;

namespace glic {

class GlicButton;

namespace {

// An observer of the GlicWindowController's panel state. Fires the given
// callback when the state changes to the given kind.
class PanelStateObserver : public GlicWindowController::StateObserver {
 public:
  PanelStateObserver(mojom::PanelState::Kind kind, base::OnceClosure callback)
      : kind_(kind), callback_(std::move(callback)) {}

  void PanelStateChanged(const mojom::PanelState& panel_state,
                         Browser* attached_browser) override {
    if (panel_state.kind == kind_) {
      std::move(callback_).Run();
    }
  }

 private:
  mojom::PanelState::Kind kind_;
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicRollout},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming});
  }
  GlicPolicyTest(const GlicPolicyTest&) = delete;
  GlicPolicyTest& operator=(const GlicPolicyTest&) = delete;

  ~GlicPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);

    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, true);

    profile_1_ = browser()->profile();
    glic_test_environment_ =
        std::make_unique<glic::GlicTestEnvironment>(browser()->profile());

    // "policy_for_profile_1_" is provider_, setup in PolicyTest.

    {
      policy_for_profile_2_.SetDefaultReturns(
          /*is_initialization_complete_return=*/true,
          /*is_first_policy_load_complete_return=*/true);
      policy::PushProfilePolicyConnectorProviderForTesting(
          &policy_for_profile_2_);

      ProfileManager* profile_manager = g_browser_process->profile_manager();
      base::FilePath new_path =
          profile_manager->GenerateNextProfileDirectoryPath();
      profile_2_ =
          &profiles::testing::CreateProfileSync(profile_manager, new_path);
      ForceSigninAndModelExecutionCapability(profile_2_);
    }
  }

  void TearDownOnMainThread() override {
    PolicyTest::TearDownOnMainThread();

    if (GlicBackgroundModeManager* background_mode_manager =
            g_browser_process->GetFeatures()->glic_background_mode_manager()) {
      background_mode_manager->ExitBackgroundMode();
    }
    profile_1_ = nullptr;
    profile_2_ = nullptr;
    glic_test_environment_.reset();
  }

  GlicButton* GetGlicButtonForBrowser(Browser* browser) {
    TabStripActionContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser)
            ->tab_strip_region_view()
            ->GetTabStripActionContainer();
    CHECK(container);
    return container->GetGlicButton();
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

  testing::NiceMock<policy::MockConfigurationPolicyProvider>&
  policy_for_profile_1() {
    // This comes from the PolicyTest base class.
    return provider_;
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider>&
  policy_for_profile_2() {
    return policy_for_profile_2_;
  }

 protected:
  // The first profile.
  raw_ptr<Profile> profile_1_;
  // The second profile.
  raw_ptr<Profile> profile_2_;

  static constexpr int kEnabledValue =
      static_cast<int>(SettingsPolicyState::kEnabled);
  static constexpr int kDisabledValue =
      static_cast<int>(SettingsPolicyState::kDisabled);

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      policy_for_profile_2_;

  std::unique_ptr<glic::GlicTestEnvironment> glic_test_environment_;

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

// Ensure navigating to chrome://glic is enabled only if the policy is enabled.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyDisablesWebUi) {
  GURL glic_url = GURL(chrome::kChromeUIGlicURL);

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_1_);

  // Navigating to chrome://glic should succeed.
  {
    GlicAppStateObserver app_observer(&service->host());
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    app_observer.Wait(mojom::WebUiState::kError);
  }

  // Disable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            browser()->profile()->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigate to chrome://glic. The glic page should be unavailable.
  {
    GlicAppStateObserver app_observer(&service->host());
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    app_observer.Wait(mojom::WebUiState::kUnavailable);
  }

  // Re-enable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigating to chrome://glic should now succeed again.
  {
    GlicAppStateObserver app_observer(&service->host());
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    app_observer.Wait(mojom::WebUiState::kError);
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

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_1_);

  // Glic shouldn't load since it's disabled by policy from startup.
  {
    GlicAppStateObserver app_observer(&service->host());
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    app_observer.Wait(mojom::WebUiState::kUnavailable);
  }

  // Enable the policy at runtime
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kEnabled);
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  // Navigating to chrome://glic should now load the webview.
  {
    GlicAppStateObserver app_observer(&service->host());
    content::TestNavigationObserver observer(glic_url);
    observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
    observer.WaitForNavigationFinished();
    ASSERT_EQ(observer.last_navigation_url(), glic_url);
    ASSERT_TRUE(observer.last_navigation_succeeded());
    // WebUi will be in an error state since the mock web client is not setup.
    app_observer.Wait(mojom::WebUiState::kError);
  }
}

// Ensure that if the policy changes to disabled at runtime, and the user has an
// an open Glic window, that window should show the unavailable page.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, DisableGlicWhenIsOpen) {
  // The pref defaults to enabled.
  ASSERT_EQ(kEnabledValue, profile_1_->GetPrefs()->GetInteger(kGeminiSettings));

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_1_);
  ASSERT_FALSE(service->window_controller().IsShowing());

  // Show the panel as if the glic button was clicked.
  {
    base::test::TestFuture<void> wait_for_panel;
    PanelStateObserver panel_state_observer(
        mojom::PanelState::Kind::kDetached,
        wait_for_panel.GetCallback());
    service->window_controller().AddStateObserver(&panel_state_observer);
    BrowserWindowInterface* bwi = browser()
                                      ->window()
                                      ->AsBrowserView()
                                      ->tabstrip()
                                      ->controller()
                                      ->GetBrowserWindowInterface();
    service->ToggleUI(bwi, /*prevent_close=*/false,
                      mojom::InvocationSource::kOsButton);

    EXPECT_TRUE(wait_for_panel.Wait());
    service->window_controller().RemoveStateObserver(&panel_state_observer);
  }

  ASSERT_TRUE(service->window_controller().IsShowing());

  // Disable the policy.
  SetGlicPolicy(policy_for_profile_1(), SettingsPolicyState::kDisabled);
  ASSERT_EQ(kDisabledValue,
            profile_1_->GetPrefs()->GetInteger(kGeminiSettings));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return service->host().GetPrimaryWebUiState() ==
           mojom::WebUiState::kUnavailable;
  })) << "Timed out waiting for unavailable state. Current state: "
      << service->host().GetPrimaryWebUiState();
  ASSERT_TRUE(service->window_controller().IsShowing());
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
}  // namespace

}  // namespace glic
