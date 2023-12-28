// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_keyboard_controller.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

class AppListKeyboardControllerTest : public AshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  AppListKeyboardControllerTest() = default;
  ~AppListKeyboardControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    app_list_test_helper_ = GetAppListTestHelper();
    app_list_test_helper_->AddRecentApps(5);
    app_list_test_helper_->AddAppItems(5);

    if (GetParam()) {
      TabletMode::Get()->SetEnabledForTest(true);
      // Engages keyboard navigation in tablet mode (otherwise `RecentAppsView`
      // and other views do not receive keyboard events).
      PressDown();
    } else {
      app_list_test_helper_->ShowAppList();
    }
  }

  void PressDown() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  }

  void PressUp() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressAndReleaseKey(ui::KeyboardCode::VKEY_UP);
  }

  AppsGridView* apps_grid_view() {
    if (GetParam())
      return app_list_test_helper_->GetRootPagedAppsGridView();
    return app_list_test_helper_->GetScrollableAppsGridView();
  }

  SearchBoxView* search_box_view() {
    return GetParam() ? app_list_test_helper_->GetSearchBoxView()
                      : app_list_test_helper_->GetBubbleSearchBoxView();
  }

  RecentAppsView* recent_apps_view() {
    return GetParam() ? app_list_test_helper_->GetFullscreenRecentAppsView()
                      : app_list_test_helper_->GetBubbleRecentAppsView();
  }

  AppListToastContainerView* toast_container() {
    return GetParam() ? app_list_test_helper_->GetAppsContainerView()
                            ->toast_container()
                      : app_list_test_helper_->GetBubbleAppsPage()
                            ->toast_container_for_test();
  }

 private:
  raw_ptr<AppListTestHelper, DanglingUntriaged> app_list_test_helper_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(IsInTabletMode,
                         AppListKeyboardControllerTest,
                         testing::Bool());

TEST_P(AppListKeyboardControllerTest, MovesFocusUpFromRecents) {
  // Focus an arbitrary item from recent apps.
  recent_apps_view()->GetItemViewAt(2)->RequestFocus();

  // Should focus on the view one step in reverse from the first recent app.
  PressUp();
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());
}

TEST_P(AppListKeyboardControllerTest, MovesFocusBetweenRecentsAndAppsGrid) {
  // Focus an arbitrary item from recent apps.
  recent_apps_view()->GetItemViewAt(2)->RequestFocus();

  // Should move focus to the apps grid keeping the same index.
  PressDown();
  EXPECT_TRUE(apps_grid_view()->GetItemViewAt(2)->HasFocus());

  // Should move focus back to recent apps keeping the same index.
  PressUp();
  EXPECT_TRUE(recent_apps_view()->GetItemViewAt(2)->HasFocus());
}

TEST_P(AppListKeyboardControllerTest,
       MovesFocusBetweenRecentsAndAppsGridViaToast) {
  // Sort apps grid (this should show the "Undo" toast).
  ASSERT_FALSE(toast_container()->IsToastVisible());
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor, false, base::OnceClosure());
  ASSERT_TRUE(toast_container()->IsToastVisible());

  // Focus an arbitrary item from recent apps.
  recent_apps_view()->GetItemViewAt(2)->RequestFocus();

  // Should move focus to the "Undo" button.
  PressDown();
  EXPECT_TRUE(toast_container()->GetToastButton()->HasFocus());

  // Should move focus to the apps grid keeping the same index.
  PressDown();
  EXPECT_TRUE(apps_grid_view()->GetItemViewAt(2)->HasFocus());

  // Should move focus back to the "Undo" button.
  PressUp();
  EXPECT_TRUE(toast_container()->GetToastButton()->HasFocus());

  // Should move focus back to recent apps keeping the same index.
  PressUp();
  EXPECT_TRUE(recent_apps_view()->GetItemViewAt(2)->HasFocus());
}

}  // namespace ash
