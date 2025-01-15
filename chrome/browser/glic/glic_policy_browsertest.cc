// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

using glic::prefs::kGlicEnabledByPolicy;

namespace glic {
class GlicButton;
}

namespace policy {

class GlicPolicyTest : public PolicyTest {
 public:
  GlicPolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
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
    }
  }

  void TearDownOnMainThread() override {
    if (glic::GlicBackgroundModeManager* background_mode_manager =
            g_browser_process->GetFeatures()->glic_background_mode_manager()) {
      background_mode_manager->ExitBackgroundMode();
    }
    profile_1_ = nullptr;
    profile_2_ = nullptr;
  }

  glic::GlicButton* GetGlicButtonForBrowser(Browser* browser) {
    TabStripActionContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser)
            ->tab_strip_region_view()
            ->GetTabStripActionContainer();
    CHECK(container);
    return container->GetGlicButton();
  }

 protected:
  // The first profile.
  raw_ptr<Profile> profile_1_;
  // The second profile.
  raw_ptr<Profile> profile_2_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      policy_for_profile_2_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PrefDisabledByPolicy) {
  // By default the pref should start off unmanaged and defaulted to enabled.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(kGlicEnabledByPolicy));
  EXPECT_TRUE(prefs->GetBoolean(kGlicEnabledByPolicy));

  // Verify that policy can force-disable Glic.
  PolicyMap policies;
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(kGlicEnabledByPolicy));
  EXPECT_FALSE(prefs->GetBoolean(kGlicEnabledByPolicy));

  // Verify the policy value cannot be overridden.
  prefs->SetBoolean(kGlicEnabledByPolicy, true);
  EXPECT_FALSE(prefs->GetBoolean(kGlicEnabledByPolicy));
}

// Ensure that when policy disables Glic, a browser window doesn't show the Glic
// button.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyAffectsGlicButtonInNewWindows) {
  ASSERT_EQ(browser()->profile(), profile_1_);
  ASSERT_NE(profile_1_, profile_2_);

  // The pref defaults to enabled.
  ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  // Disable the policy in the default profile.
  PolicyMap policies;
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_FALSE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  {
    // A new window in profile 1 shouldn't have the Glic button.
    Browser* new_window_profile_1 = CreateBrowser(profile_1_);
    EXPECT_FALSE(GetGlicButtonForBrowser(new_window_profile_1));

    // A new window in profile 2 should continue to have the Glic button since
    // only profile 1 disabled Glic.
    Browser* new_window_profile_2 = CreateBrowser(profile_2_);
    EXPECT_TRUE(GetGlicButtonForBrowser(new_window_profile_2));
  }

  // Re-enable the policy. Ensure the button is recreated.
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(true), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  {
    // A new window in profile 1 should again get the Glic button now that the
    // policy is re-enabled.
    Browser* new_window_profile_1 = CreateBrowser(profile_1_);
    EXPECT_TRUE(GetGlicButtonForBrowser(new_window_profile_1));
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

  // The pref defaults to enabled. Ensure the button was created in each window.
  ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_1));
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_2));
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1));
  EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2));

  // Disable the policy in the first profile.
  PolicyMap policies;
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_FALSE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  {
    // The windows in profile 1 should have lost their Glic button.
    EXPECT_FALSE(GetGlicButtonForBrowser(profile_1_window_1));
    EXPECT_FALSE(GetGlicButtonForBrowser(profile_1_window_2));

    // The windows in profile 2 should have kept their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1));
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2));
  }

  // Re-enable the policy. Ensure the button is recreated.
  policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(true), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  {
    // The windows in profile 1 should get back their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_1));
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_1_window_2));

    // The windows in profile 2 still have their Glic button.
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_1));
    EXPECT_TRUE(GetGlicButtonForBrowser(profile_2_window_2));
  }
}

// Ensure that background mode is entered if and only if a profile with the
// policy enabled is loaded.
IN_PROC_BROWSER_TEST_F(GlicPolicyTest, PolicyDisablesBackgroundMode) {
  ASSERT_EQ(browser()->profile(), profile_1_);
  ASSERT_NE(profile_1_, profile_2_);

  Browser* new_window_profile_2 = CreateBrowser(profile_2_);
  ASSERT_TRUE(new_window_profile_2);

  // The pref defaults to enabled.
  ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
  ASSERT_TRUE(profile_2_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));

  glic::GlicBackgroundModeManager* background_mode_manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());

  // Disable the policy in the default profile.
  {
    PolicyMap policies;
    policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
    UpdateProviderPolicy(policies);
    ASSERT_FALSE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
    ASSERT_TRUE(profile_2_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
  }

  // Background mode should remain active since profile_2_ still has it enabled.
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());

  // Disable the policy in the second profile.
  {
    PolicyMap policies;
    policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false), nullptr);
    policy_for_profile_2_.UpdateChromePolicy(policies);
    ASSERT_FALSE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
    ASSERT_FALSE(profile_2_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
  }

  // Background mode should be exited since none of the loaded profiles enable
  // Glic.
  EXPECT_FALSE(background_mode_manager->IsInBackgroundModeForTesting());

  // Enable the policy in the default profile again.
  {
    PolicyMap policies;
    policies.Set(key::kGlicEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(true), nullptr);
    UpdateProviderPolicy(policies);
    ASSERT_TRUE(profile_1_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
    ASSERT_FALSE(profile_2_->GetPrefs()->GetBoolean(kGlicEnabledByPolicy));
  }

  // Background mode should be reentered since the first profile is enabled.
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());
}

}  // namespace policy
