// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/default_pinned_apps/default_pinned_apps.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/test/find_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {
namespace {

class FilesAppInteractiveTest : public InteractiveAshTest {
 public:
  // InteractiveBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Ensure the File Manager app is installed.
    InstallSystemApps();
  }
};

// Launches the files app from the shelf context menu to test integration.
IN_PROC_BROWSER_TEST_F(FilesAppInteractiveTest, LaunchFromShelfContextMenu) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-7b10de50-1ede-4b3e-8fd8-5c54c8e7331b");

  // Kombucha requires a context widget.
  views::Widget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->shelf_widget();
  SetContextWidget(status_area_widget);

  ShelfModel* shelf_model = ShelfModel::Get();
  ASSERT_TRUE(shelf_model);

  // The files app should be pinned by default to the shelf in the initial
  // configuration.
  int app_index =
      shelf_model->ItemIndexByAppID(file_manager::kFileManagerSwaAppId);
  ASSERT_LE(0, app_index);

  ShelfView* shelf_view = Shell::GetPrimaryRootWindowController()
                              ->shelf()
                              ->GetShelfViewForTesting();
  auto shelf_children = shelf_view->children();
  ASSERT_GT(static_cast<int>(shelf_children.size()), app_index);
  views::View* file_button_view = shelf_children[app_index];
  gfx::Rect file_button_rect = file_button_view->GetBoundsInScreen();

  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  RunTestSequence(MoveMouseTo(file_button_rect.CenterPoint()),
                  ClickMouse(ui_controls::RIGHT),
                  InAnyContext(SelectMenuItem(apps::kLaunchNewMenuItem)),
                  WaitForWindowWithTitle(env, u"Files"));
}

}  // namespace
}  // namespace ash
