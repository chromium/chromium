// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_aura_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"
#include "ui/ozone/public/ozone_platform.h"

namespace policy {

class SystemTitleBarEnabledTest : public PolicyTest {
 public:
  SystemTitleBarEnabledTest() {
    feature_list_.InitAndDisableFeature(features::kAppMenuGlowUp);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemTitleBarEnabledTest, PolicyUpdates) {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* native_widget = static_cast<BrowserNativeWidgetAuraLinux*>(
      browser_view->browser_widget()->browser_native_widget());
  SystemMenuModelDelegate menu_delegate(nullptr, browser());
  auto* action = actions::ActionManager::Get().FindAction(
      kUseSystemTitleBar, BrowserActions::From(browser())->root_action_item());
  ASSERT_TRUE(action);

  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kUseCustomChromeFrame));
  const bool user_choice = !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
  prefs->SetBoolean(prefs::kUseCustomChromeFrame, user_choice);
  const bool supports_system_title_bar =
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_server_side_window_decorations;
  EXPECT_EQ(supports_system_title_bar,
            chrome::IsCommandEnabled(browser(), IDC_USE_SYSTEM_TITLE_BAR));
  EXPECT_EQ(supports_system_title_bar, action->GetEnabled());
  // Start with the user's value to also exercise management-only changes.
  for (bool use_custom_frame : {user_choice, !user_choice}) {
    PolicyMap policies;
    policies.Set(key::kSystemTitleBarEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(!use_custom_frame), nullptr);
    UpdateProviderPolicy(policies);

    EXPECT_TRUE(prefs->IsManagedPreference(prefs::kUseCustomChromeFrame));
    EXPECT_EQ(use_custom_frame,
              prefs->GetBoolean(prefs::kUseCustomChromeFrame));
    EXPECT_FALSE(menu_delegate.IsCommandIdEnabled(IDC_USE_SYSTEM_TITLE_BAR));
    EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_USE_SYSTEM_TITLE_BAR));
    EXPECT_FALSE(action->GetEnabled());
    EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_USE_SYSTEM_TITLE_BAR));
    action->InvokeAction();
    EXPECT_EQ(use_custom_frame,
              prefs->GetBoolean(prefs::kUseCustomChromeFrame));
    EXPECT_EQ(use_custom_frame || !supports_system_title_bar,
              native_widget->UseCustomFrame());
  }

  UpdateProviderPolicy(PolicyMap());
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kUseCustomChromeFrame));
  EXPECT_EQ(user_choice, prefs->GetBoolean(prefs::kUseCustomChromeFrame));
  EXPECT_EQ(supports_system_title_bar,
            chrome::IsCommandEnabled(browser(), IDC_USE_SYSTEM_TITLE_BAR));
  EXPECT_EQ(supports_system_title_bar, action->GetEnabled());
}

}  // namespace policy
