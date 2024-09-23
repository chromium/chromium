// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_context_menu_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_contents_view.h"
#include "ash/wm/window_restore/informed_restore_context_menu_model.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_restore/informed_restore_test_base.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "components/app_constants/constants.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr size_t kMenuItemCount = 5u;

}  // namespace

using InformedRestoreContextMenuModelTest = InformedRestoreTestBase;

// Tests that the layout and options of the inline context menu are correct.
TEST_F(InformedRestoreContextMenuModelTest, LayoutAndCommands) {
  InformedRestoreContextMenuModel menu;
  ASSERT_EQ(kMenuItemCount, menu.GetItemCount());
  EXPECT_EQ(ui::MenuModel::kTitleId, menu.GetCommandIdAt(0));
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(1)));
  EXPECT_EQ(full_restore::RestoreOption::kAlways,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(2)));
  EXPECT_EQ(full_restore::RestoreOption::kDoNotRestore,
            static_cast<full_restore::RestoreOption>(menu.GetCommandIdAt(3)));
  EXPECT_EQ(InformedRestoreContextMenuModel::kDescriptionId,
            menu.GetCommandIdAt(4));
  for (size_t i = 1; i < kMenuItemCount; ++i) {
    EXPECT_TRUE(menu.IsEnabledAt(i));
    EXPECT_TRUE(menu.IsVisibleAt(i));
  }
}

// Tests that pressing the settings button in the informed restore dialog
// properly displays the inline context menu.
TEST_F(InformedRestoreContextMenuModelTest,
       ShowContextMenuOnSettingsButtonClicked) {
  auto contents_data = std::make_unique<InformedRestoreContentsData>();
  contents_data->apps_infos.emplace_back(app_constants::kChromeAppId, "Title",
                                         /*window_id=*/0);
  Shell::Get()->informed_restore_controller()->MaybeStartInformedRestoreSession(
      std::move(contents_data));
  WaitForOverviewEntered();

  // Get the active informed restore widget.
  OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(grid);
  auto* informed_restore_widget =
      OverviewGridTestApi(grid).informed_restore_widget();
  ASSERT_TRUE(informed_restore_widget);

  // The context menu should not be open.
  auto* contents_view = views::AsViewClass<InformedRestoreContentsView>(
      informed_restore_widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  EXPECT_FALSE(contents_view->context_menu_model_.get());

  // Click on the settings button, the context menu should appear.
  LeftClickOn(contents_view->settings_button_.get());
  EXPECT_TRUE(contents_view->context_menu_model_.get());
}

// Tests that each menu option properly sets the restore preference when
// activated.
TEST_F(InformedRestoreContextMenuModelTest, RestorePreferences) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  ASSERT_TRUE(pref_service);

  // Check the default restore behavior.
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));

  // Activate the other menu options (commands) and ensure the preference is set
  // properly.
  InformedRestoreContextMenuModel menu;
  menu.ActivatedAt(2);
  EXPECT_EQ(full_restore::RestoreOption::kAlways,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
  menu.ActivatedAt(3);
  EXPECT_EQ(full_restore::RestoreOption::kDoNotRestore,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
  menu.ActivatedAt(1);
  EXPECT_EQ(full_restore::RestoreOption::kAskEveryTime,
            static_cast<full_restore::RestoreOption>(
                pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName)));
}

}  // namespace ash
