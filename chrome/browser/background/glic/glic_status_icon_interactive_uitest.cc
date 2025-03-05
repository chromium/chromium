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
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {

class PanelStateChangeObserver : public GlicWindowController::StateObserver {
 public:
  PanelStateChangeObserver(Browser* browser,
                           base::OnceCallback<void()> callback)
      : callback_(std::move(callback)) {
    auto* service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser->GetProfile());
    panel_state_observer_.Observe(&service->window_controller());
  }
  ~PanelStateChangeObserver() override = default;

  void PanelStateChanged(const mojom::PanelState& panel_state,
                         Browser* attached_browser) override {
    std::move(callback_).Run();
  }

 private:
  base::OnceCallback<void()> callback_;
  base::ScopedObservation<GlicWindowController,
                          GlicWindowController::StateObserver>
      panel_state_observer_{this};
};

class GlicStatusIconUiTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
    InteractiveBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ForceSigninAndModelExecutionCapability(browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Disable glic so that the glic_background_mode_manager won't prevent the
    // browser process from closing which causes the test to hang.
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
  }

  bool IsContextMenuItemVisible(int id) {
    return GetContextMenu()->IsCommandIdVisible(id);
  }

  void ExecuteCommandAndWaitForPanelStateChange(int id) {
    base::RunLoop run_loop;
    PanelStateChangeObserver observer(browser(), run_loop.QuitClosure());
    GetContextMenu()->ExecuteCommand(id, 0);
    run_loop.Run();
  }

 private:
  StatusIconMenuModel* GetContextMenu() {
    GlicBackgroundModeManager* const manager =
        g_browser_process->GetFeatures()->glic_background_mode_manager();
    GlicStatusIcon* status_icon = manager->GetStatusIconForTesting();
    return status_icon->GetContextMenuForTesting();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Checks that the Show and Close status icon context menu items are mutually
// exclusive.
IN_PROC_BROWSER_TEST_F(GlicStatusIconUiTest, ShowClose) {
  ASSERT_FALSE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  EXPECT_TRUE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW));
  EXPECT_FALSE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_CLOSE));

  ExecuteCommandAndWaitForPanelStateChange(IDC_GLIC_STATUS_ICON_MENU_SHOW);

  EXPECT_FALSE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW));
  EXPECT_TRUE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_CLOSE));

  ExecuteCommandAndWaitForPanelStateChange(IDC_GLIC_STATUS_ICON_MENU_CLOSE);

  EXPECT_TRUE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW));
  EXPECT_FALSE(IsContextMenuItemVisible(IDC_GLIC_STATUS_ICON_MENU_CLOSE));
}

}  // namespace glic
