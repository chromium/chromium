// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {
// ProgressIndicatorWaiter -----------------------------------------------------

// A class which supports waiting for a progress indicator to reach a desired
// state of progress.
class ProgressIndicatorWaiter {
 public:
  ProgressIndicatorWaiter() = default;
  ProgressIndicatorWaiter(const ProgressIndicatorWaiter&) = delete;
  ProgressIndicatorWaiter& operator=(const ProgressIndicatorWaiter&) = delete;
  ~ProgressIndicatorWaiter() = default;

  // Waits for `progress_indicator` to reach the specified `progress`. If the
  // `progress_indicator` is already at `progress`, this method no-ops.
  void WaitForProgress(ProgressIndicator* progress_indicator,
                       const std::optional<float>& progress) {
    if (progress_indicator->progress() == progress) {
      return;
    }
    base::RunLoop run_loop;
    auto subscription = progress_indicator->AddProgressChangedCallback(
        base::BindLambdaForTesting([&]() {
          if (progress_indicator->progress() == progress) {
            run_loop.Quit();
          }
        }));
    run_loop.Run();
  }
};

}  // namespace

class AppListItemViewTest : public AshTestBase {
 public:
  AppListItemViewTest() = default;
  ~AppListItemViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kPromiseIcons, true}});

    AshTestBase::SetUp();

    ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
        base::BindLambdaForTesting([&]() {
          drag_started_on_controller_++;
          ASSERT_TRUE(drag_view_);
          EXPECT_EQ(GetDragState(drag_view_),
                    AppListItemView::DragState::kStarted);
          GetEventGenerator()->ReleaseTouch();
        }),
        base::DoNothing());
  }

  static views::View* GetNewInstallDot(AppListItemView* view) {
    return view->new_install_dot_;
  }

  AppListItem* CreateAppListItem(const std::string& name) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndAddItem(name + "_id");
    item->SetName(name);
    return item;
  }

  AppListItem* CreatePromiseAppListItem(const std::string& name) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndAddPromiseItem(name + "_id");
    item->SetName(name);
    return item;
  }

  AppListItem* CreateFolderItem(const int num_apps) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(
            num_apps);
    return item;
  }

  AppListItemView::DragState GetDragState(AppListItemView* view) {
    return view->drag_state_;
  }

  bool IsIconScaled(AppListItemView* view) { return view->icon_scale_ != 1.0f; }

  void SetAppListItemViewForTest(AppListItemView* view) { drag_view_ = view; }

  void MaybeCheckDragStartedOnControllerCount(int count) {
    EXPECT_EQ(count, drag_started_on_controller_);
  }

  int drag_started_on_controller_ = 0;
  raw_ptr<AppListItemView, DanglingUntriaged> drag_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppListItemViewTest, NewInstallDot) {
  AppListItem* item = CreateAppListItem("Google Buzz");
  ASSERT_FALSE(item->is_new_install());

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  ui::AXNodeData node_data;

  // By default, the new install dot is not visible.
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  views::View* new_install_dot = GetNewInstallDot(item_view);
  ASSERT_TRUE(new_install_dot);
  EXPECT_FALSE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz");
  EXPECT_EQ(item_view->GetViewAccessibility().GetCachedDescription(), u"");

  // When the app is a new install the dot is visible and the tooltip changes.
  item->SetIsNewInstall(true);
  EXPECT_TRUE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz\nNew install");

  EXPECT_EQ(item_view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_NEW_INSTALL_ACCESSIBILE_DESCRIPTION));
}

TEST_F(AppListItemViewTest, LabelInsetWithNewInstallDot) {
  AppListItem* long_item = CreateAppListItem("Very very very very long name");
  long_item->SetIsNewInstall(true);
  AppListItem* short_item = CreateAppListItem("Short");
  short_item->SetIsNewInstall(true);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* long_item_view = apps_grid_view->GetItemViewAt(0);
  AppListItemView* short_item_view = apps_grid_view->GetItemViewAt(1);

  // The item with the long name has its title bounds left edge inset to make
  // space for the blue dot.
  EXPECT_LT(long_item_view->GetDefaultTitleBoundsForTest().x(),
            long_item_view->title()->x());

  // The item with the short name does not have the title bounds inset, because
  // there is enough space for the blue dot as-is.
  EXPECT_EQ(short_item_view->GetDefaultTitleBoundsForTest(),
            short_item_view->title()->bounds());
}

TEST_F(AppListItemViewTest, AppItemReleaseTouchBeforeTimerFires) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_F(AppListItemViewTest, AppItemDragStateChange) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  generator->MoveTouchBy(10, 10);

  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_F(AppListItemViewTest, AppItemDragStateAfterLongPress) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // Verify that actual drag state is not started until the item is moved.
  ui::GestureEvent long_press(
      from.x(), from.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator->Dispatch(&long_press);
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // After a long press, the first event type is EventType::kGestureScrollBegin
  // but drag does not start until EventType::kGestureScrollUpdate, so do the
  // movement in two steps.
  generator->MoveTouchBy(5, 5);
  generator->MoveTouchBy(5, 5);

  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_F(AppListItemViewTest, AppItemReleaseTouchBeforeDragStart) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_F(AppListItemViewTest, AppItemReleaseTouchBeforeDragStartWithLongPress) {
  CreateAppListItem("TestItem");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  ui::GestureEvent long_press(
      from.x(), from.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator->Dispatch(&long_press);

  generator->ReleaseTouch();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_EQ(drag_started_on_controller_, 0);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(0);
}

TEST_F(AppListItemViewTest, TouchDragAppRemovedDoesNotCrash) {
  CreateAppListItem("TestItem 1");
  CreateAppListItem("TestItem 2");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  const std::string kDraggedItemId = view->item()->id();
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  const views::ViewModelT<AppListItemView>* view_model =
      apps_grid_view->view_model();
  EXPECT_EQ(view_model->view_size(), 2u);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // Make sure that the item view is deleted after releasing the drag. This
  // should occur within the same thread as the events to force the crash.
  ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        drag_started_on_controller_++;
        GetEventGenerator()->ReleaseTouch();
        GetAppListTestHelper()->model()->DeleteItem(kDraggedItemId);
      }),
      base::DoNothing());

  generator->MoveTouch(
      apps_grid_view->GetItemViewAt(1)->GetIconBoundsInScreen().CenterPoint());

  EXPECT_EQ(view_model->view_size(), 1u);
  EXPECT_FALSE(GetAppListTestHelper()->model()->FindItem(kDraggedItemId));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_F(AppListItemViewTest, AppListFolderLabelShowsAfterMouseClick) {
  CreateFolderItem(2);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(view->title()->GetVisible());

  auto* generator = GetEventGenerator();

  // Press folder icon to open.
  gfx::Point folder_center = view->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(folder_center);
  generator->ClickLeftButton();
  EXPECT_TRUE(helper->IsInFolderView());

  // Attempt to fire the mouse drag timer. The view should've stopped after
  // releasing the click.
  EXPECT_FALSE(view->FireMouseDragTimerForTest());

  // Press ESC key to close the folder grid.
  generator->PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(helper->IsInFolderView());

  EXPECT_TRUE(view->title()->GetVisible());
}

TEST_F(AppListItemViewTest, AppItemDragStateResetsAfterDrag) {
  CreateAppListItem("TestItem 1");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  ASSERT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  SetAppListItemViewForTest(view);

  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();
  view->FireTouchDragTimerForTest();
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kInitialized);

  // Make sure that the item view has a started drag state during drag.
  ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
      base::BindLambdaForTesting([&]() {
        drag_started_on_controller_++;
        generator->MoveTouchBy(10, 10);
        EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kStarted);
        generator->MoveMouseTo(apps_grid_view->GetBoundsInScreen().top_right());
        generator->MoveTouchBy(10, 10);
        EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kStarted);
        helper->Dismiss();
        EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kStarted);
        generator->ReleaseTouch();
      }),
      base::DoNothing());

  generator->MoveTouchBy(10, 10);

  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_FALSE(view->FireTouchDragTimerForTest());
  EXPECT_FALSE(IsIconScaled(view));
  MaybeCheckDragStartedOnControllerCount(1);
}

TEST_F(AppListItemViewTest, AppStatusReflectsOnProgressIndicator) {
  AppListItem* item = CreatePromiseAppListItem("TestItem 1");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);

  // Promise apps are created with app_status kPending.
  ProgressIndicator* progress_indicator = view->GetProgressIndicatorForTest();

  EXPECT_EQ(view->item()->progress(), -1.0f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Change app status to installing and send a progress update. Verify that the
  // progress indicator correctly reflects the progress.
  item->SetAppStatus(AppStatus::kInstalling);
  item->SetProgress(0.3f);
  EXPECT_EQ(view->item()->progress(), 0.3f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.3f);

  // Change app status back to pending state. Verify that even if the item had
  // progress previously associated to it, the progress indicator reflects as
  // 0 progress since it is pending.
  item->SetAppStatus(AppStatus::kPending);
  EXPECT_EQ(view->item()->progress(), 0.3f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Send another progress update. Since the app status is still pending, the
  // progress indicator still be 0
  item->SetProgress(0.8f);
  EXPECT_EQ(view->item()->progress(), 0.8f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  // Set the last status update to kInstallSuccess as if the app had finished
  // installing.
  item->SetAppStatus(AppStatus::kInstallSuccess);

  // No crash.
}

TEST_F(AppListItemViewTest, AccessibleDescription) {
  AppListItem* item = CreatePromiseAppListItem("TestItem 1");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);

  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(), u"");

  // Promise apps are created with app_status kPending.
  ProgressIndicator* progress_indicator = view->GetProgressIndicatorForTest();
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  item->SetAppStatus(AppStatus::kBlocked);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetStringUTF16(IDS_APP_LIST_BLOCKED_APP));

  item->SetAppStatus(AppStatus::kPaused);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetStringUTF16(IDS_APP_LIST_PAUSED_APP));

  item->SetAppStatus(AppStatus::kInstalling);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(), u"");
}

TEST_F(AppListItemViewTest, FolderItemAccessibleDescription) {
  AppListItem* item = CreateFolderItem(2);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);

  item->SetAppStatus(AppStatus::kInstalling);
  item->SetProgress(0.3f);
  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetPluralStringFUTF16(
                IDS_APP_LIST_FOLDER_NUMBER_OF_APPS_ACCESSIBILE_DESCRIPTION, 2));
}

TEST_F(AppListItemViewTest, UpdateProgressOnPromiseIcon) {
  AppListItem* item = CreatePromiseAppListItem("TestItem 1");

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* view = apps_grid_view->GetItemViewAt(0);

  // Start install progress bar.
  item->SetAppStatus(AppStatus::kInstalling);
  item->SetProgress(0.f);
  ProgressIndicator* progress_indicator = view->GetProgressIndicatorForTest();

  EXPECT_EQ(view->item()->progress(), 0.f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.0f);

  item->SetProgress(0.3f);
  EXPECT_EQ(view->item()->progress(), 0.3f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.3f);

  item->SetProgress(0.7f);
  EXPECT_EQ(view->item()->progress(), 0.7f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 0.7f);

  item->SetProgress(1.5f);
  EXPECT_EQ(view->item()->progress(), 1.5f);
  ProgressIndicatorWaiter().WaitForProgress(progress_indicator, 1.0f);
}

}  // namespace ash
