// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {

class GlicStatusIconUiTest : public test::InteractiveGlicTest,
                             public testing::WithParamInterface<bool> {
 public:
  GlicStatusIconUiTest() {
    feature_list_.InitAndDisableFeature(features::kGlicWarming);
  }
  ~GlicStatusIconUiTest() override = default;

  void TearDownOnMainThread() override {
    // Disable glic so that the glic_background_mode_manager won't prevent the
    // browser process from closing which causes the test to hang.
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    test_env_for_second_profile_.reset();
    test::InteractiveGlicTest::TearDownOnMainThread();
  }

  bool IsContextMenuItemVisible(int id) {
    return GetContextMenu()->IsCommandIdVisible(id);
  }

  auto EnableGlicLauncher() {
    return Do([]() {
      g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                   true);
    });
  }

  auto VerifyStatusIconVisibility(bool visible) {
    return Do([visible]() {
      EXPECT_EQ(visible,
                g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
                    StatusTray::StatusIconType::GLIC_ICON));
    });
  }

  auto VerifyStatusIconContextMenuItemVisibilty(bool show_item_is_visible) {
    return Do([this, show_item_is_visible]() {
      EXPECT_EQ(show_item_is_visible,
                this->IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW));
      EXPECT_NE(show_item_is_visible, this->IsContextMenuItemVisible(
                                          IDC_GLIC_STATUS_ICON_MENU_CLOSE));
    });
  }

  auto SimulateContextMenuItemClick(int id) {
    return Do([this, id]() { this->GetContextMenu()->ExecuteCommand(id, 0); });
  }

  bool ShouldForceEmptyButtonBounds() const { return GetParam(); }

  auto CreateAndActivateGlicForProfile(Profile* profile) {
    // Taken from GlicProfileManagerBrowserTest.
    return Do([profile] {
      auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
      GlicProfileManager::GetInstance()->SetActiveGlic(service);
      // We will also force this glic to be the default.
      GlicProfileManager::ForceProfileForLaunchForTesting(profile);
    });
  }

  auto ForceEmptyButtonBounds(Browser* browser) {
    return Do([browser] {
      auto* glic_button = browser->window()
                              ->AsBrowserView()
                              ->tab_strip_region_view()
                              ->GetGlicButton();
      glic_button->SetSize(gfx::Size(0, 0));
    });
  }

  Profile* CreateSecondProfile() {
    auto* profile_manager = g_browser_process->profile_manager();
    auto new_path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, new_path);
    auto* profile = profile_manager->GetProfile(new_path);
    // Build a test environment for the new profile to ensure that it can work
    // with glic.
    test_env_for_second_profile_ =
        std::make_unique<GlicTestEnvironment>(profile);
    return profile;
  }

  GlicWindowController& GetWindowControllerForProfile(Profile* profile) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    return service->window_controller();
  }

 private:
  StatusIconMenuModel* GetContextMenu() {
    GlicBackgroundModeManager* const manager =
        g_browser_process->GetFeatures()->glic_background_mode_manager();
    GlicStatusIcon* status_icon = manager->GetStatusIconForTesting();
    return status_icon->GetContextMenuForTesting();
  }

  std::unique_ptr<GlicTestEnvironment> test_env_for_second_profile_;
  base::test::ScopedFeatureList feature_list_;
};

// Checks that the Show and Close status icon context menu items are mutually
// exclusive. Also optionally forces empty button bounds for the glic button
// (we would previously have an error if we attempted to click the 'Show'
// context menu item in this case). This could naturally happen, racily, if the
// show button is clicked before the button view is shown, but to reliably
// reproduce the issue, we force the view to have zero bounds here.
IN_PROC_BROWSER_TEST_P(GlicStatusIconUiTest, ShowClose) {
  Profile* other_profile = CreateSecondProfile();
  RunTestSequence(
      VerifyStatusIconVisibility(false), EnableGlicLauncher(),
      VerifyStatusIconVisibility(true),
      VerifyStatusIconContextMenuItemVisibilty(true),
      WaitForShow(kGlicButtonElementId),
      If([this]() { return this->ShouldForceEmptyButtonBounds(); },
         Then(ForceEmptyButtonBounds(browser()))),
      SimulateContextMenuItemClick(IDC_GLIC_STATUS_ICON_MENU_SHOW),
      WaitForAndInstrumentGlic(kHostAndContents),
      VerifyStatusIconContextMenuItemVisibilty(false),
      SimulateContextMenuItemClick(IDC_GLIC_STATUS_ICON_MENU_CLOSE),
      WaitForHide(kGlicViewElementId),
      VerifyStatusIconContextMenuItemVisibilty(true),
      CreateAndActivateGlicForProfile(other_profile),
      VerifyStatusIconContextMenuItemVisibilty(true),
      SimulateContextMenuItemClick(IDC_GLIC_STATUS_ICON_MENU_SHOW),
      WaitForAndInstrumentGlic(kHostAndContents,
                               GetWindowControllerForProfile(other_profile)),
      VerifyStatusIconContextMenuItemVisibilty(false));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicStatusIconUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "empty_button_bounds"
                                             : "non_empty_button_bounds";
                         });

}  // namespace glic
