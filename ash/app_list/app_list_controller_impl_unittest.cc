// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <string>
#include <vector>

#include "ash/app_list/app_list_badge_controller.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_utils.h"

namespace ash {

namespace {

void PressHomeButton() {
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks());
}

AppListModel* GetAppListModel() {
  return AppListModelProvider::Get()->model();
}

AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
}

ContentsView* GetContentsView() {
  return GetAppListView()->app_list_main_view()->contents_view();
}

aura::Window* GetVirtualKeyboardWindow() {
  return Shell::Get()
      ->keyboard_controller()
      ->keyboard_ui_controller()
      ->GetKeyboardWindow();
}

AppsContainerView* GetAppsContainerView() {
  return GetContentsView()->apps_container_view();
}

PagedAppsGridView* GetAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
}

void ShowAppListNow(AppListViewState state) {
  Shell::Get()->app_list_controller()->fullscreen_presenter()->Show(
      state, display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      base::TimeTicks::Now(), /*show_source*/ std::nullopt);
}

void DismissAppListNow() {
  Shell::Get()->app_list_controller()->fullscreen_presenter()->Dismiss(
      base::TimeTicks::Now());
}

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;

  // ShelfModel::ShelfItemFactory:
  std::unique_ptr<ShelfItem> CreateShelfItemForApp(
      const ShelfID& shelf_id,
      ShelfItemStatus status,
      ShelfItemType shelf_item_type,
      const std::u16string& title) override {
    auto item = std::make_unique<ShelfItem>();
    item->id = shelf_id;
    item->status = status;
    item->type = shelf_item_type;
    item->title = title;
    return item;
  }

  std::unique_ptr<ShelfItemDelegate> CreateShelfItemDelegateForAppId(
      const std::string& app_id) override {
    return std::make_unique<TestShelfItemDelegate>(ShelfID(app_id));
  }
};

}  // namespace

class AppListControllerImplTest : public AshTestBase {
 public:
  AppListControllerImplTest() = default;

  AppListControllerImplTest(const AppListControllerImplTest&) = delete;
  AppListControllerImplTest& operator=(const AppListControllerImplTest&) =
      delete;

  ~AppListControllerImplTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    shelf_item_factory_ = std::make_unique<ShelfItemFactoryFake>();
    ShelfModel::Get()->SetShelfItemFactory(shelf_item_factory_.get());
    // Disable nested loops to avoid blocking during drag and drop sequences.
    ShellTestApi().drag_drop_controller()->SetDisableNestedLoopForTesting(true);
  }

  void TearDown() override {
    ShelfModel::Get()->SetShelfItemFactory(nullptr);
    AshTestBase::TearDown();
  }

  void PopulateItem(int num) {
    AppListModel* const model = GetAppListModel();
    for (int i = 0; i < num; i++) {
      AppListItem* item = model->AddItem(std::make_unique<AppListItem>(
          "app_id" +
          base::UTF16ToUTF8(base::FormatNumber(populated_item_count_))));
      // Give each item a name so that the accessibility paint checks pass.
      // (Focusable items should have accessible names.)
      model->SetItemName(item, item->id());

      ++populated_item_count_;
    }
  }

  bool IsAppListBoundsAnimationRunning() {
    AppListView* app_list_view = GetAppListTestHelper()->GetAppListView();
    ui::Layer* widget_layer =
        app_list_view ? app_list_view->GetWidget()->GetLayer() : nullptr;
    return widget_layer && widget_layer->GetAnimator()->is_animating();
  }

 private:
  // The count of the items created by `PopulateItem()`.
  int populated_item_count_ = 0;

  std::unique_ptr<ShelfItemFactoryFake> shelf_item_factory_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the AppList hides when shelf alignment changes. This necessary
// because the AppList is shown with certain assumptions based on shelf
// orientation.
TEST_F(AppListControllerImplTest, AppListHiddenWhenShelfAlignmentChanges) {
  Shelf* const shelf = AshTestBase::GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);

  const std::vector<ShelfAlignment> alignments(
      {ShelfAlignment::kLeft, ShelfAlignment::kRight, ShelfAlignment::kBottom});
  for (ShelfAlignment alignment : alignments) {
    ShowAppListNow(AppListViewState::kFullscreenAllApps);
    EXPECT_TRUE(Shell::Get()
                    ->app_list_controller()
                    ->fullscreen_presenter()
                    ->IsVisibleDeprecated());
    shelf->SetAlignment(alignment);
    EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  }
}

// Verifies that the dragged item has the correct focusable siblings after drag
// (https://crbug.com/990071).
TEST_F(AppListControllerImplTest, CheckTabOrderAfterDragIconToShelf) {
  // Adds three items to AppsGridView.
  PopulateItem(3);

  // Shows the app list in fullscreen.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);
  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* item1 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  AppListItemView* item2 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 1));
  const AppListItemView* item3 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 2));

  // Verifies that AppListItemView has the correct focusable siblings before
  // drag.
  ASSERT_EQ(item1, item2->GetPreviousFocusableView());
  ASSERT_EQ(item3, item2->GetNextFocusableView());

  // Pins |item2| by dragging it to ShelfView.
  ShelfView* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  ASSERT_EQ(0u, shelf_view->view_model()->view_size());
  GetEventGenerator()->MoveMouseTo(item2->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  item2->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseTo(
      shelf_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ReleaseLeftButton();
  ASSERT_EQ(1u, shelf_view->view_model()->view_size());

  // Verifies that the dragged item has the correct previous/next focusable
  // view after drag.
  EXPECT_EQ(item1, item2->GetPreviousFocusableView());
  EXPECT_EQ(item3, item2->GetNextFocusableView());
}

TEST_F(AppListControllerImplTest, PageResetByTimerInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  PopulateItem(30);

  PagedAppsGridView* apps_grid_view = GetAppsGridView();
  apps_grid_view->pagination_model()->SelectPage(1, false /* animate */);

  // Create a test window to hide the app list.
  std::unique_ptr<views::Widget> dummy =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());

  // When timer is not skipped the selected page should not change when app list
  // is closed.
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());

  // Skip the page reset timer to simulate timer exipration.
  GetAppListView()->SetSkipPageResetTimerForTesting(true);

  dummy->Minimize();

  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());

  dummy->Show();
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());

  // Once the app list is closed, the page should be reset when the timer is
  // skipped.
  EXPECT_EQ(0, apps_grid_view->pagination_model()->selected_page());
}

TEST_F(AppListControllerImplTest, PagePersistanceTabletModeTest) {
  PopulateItem(30);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());

  PagedAppsGridView* const apps_grid_view = GetAppsGridView();
  apps_grid_view->pagination_model()->SelectPage(1, false /* animate */);

  // Close and re-open the app list to ensure the current page persists.
  std::unique_ptr<views::Widget> dummy =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
  dummy->Minimize();
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());

  // The current page should not be reset for the tablet mode app list.
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());
}

// Verifies that the the virtual keyboard does not get shown if the search box
// is activated by user typing when the app list in the fullscreen state in
// tablet mode.
TEST_F(AppListControllerImplTest, VirtualKeyboardNotShownWhenUserStartsTyping) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Show the AppListView, then simulate a key press - verify that the virtual
  // keyboard is not shown.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
  PressAndReleaseKey(ui::KeyboardCode::VKEY_0);
  EXPECT_EQ(AppListViewState::kFullscreenSearch,
            GetAppListView()->app_list_state());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetVirtualKeyboardWindow()->IsVisible());

  // The keyboard should get shown if the user taps on the search box.
  GestureTapOn(GetAppListView()->search_box_view());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
}

#if defined(ADDRESS_SANITIZER)
#define MAYBE_CloseNotificationWithAppListShown \
  DISABLED_CloseNotificationWithAppListShown
#else
#define MAYBE_CloseNotificationWithAppListShown \
  CloseNotificationWithAppListShown
#endif

// Verifies that closing notification by gesture should not dismiss the AppList.
// (see https://crbug.com/948344)
// TODO(crbug.com/40714854): Test is flaky on ASAN builds.
TEST_F(AppListControllerImplTest, MAYBE_CloseNotificationWithAppListShown) {
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  // Add one notification.
  ASSERT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
  const std::string notification_id("id");
  const std::string notification_title("title");
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::UTF8ToUTF16(notification_title), u"test message",
          ui::ImageModel(), std::u16string() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(
      1u, message_center::MessageCenter::Get()->GetPopupNotifications().size());

  // Calculate the drag start point and end point.
  SystemTrayTestApi test_api;
  message_center::MessagePopupView* popup_view =
      test_api.GetPopupViewForNotificationID(notification_id);
  ASSERT_TRUE(popup_view);
  gfx::Rect bounds_in_screen = popup_view->GetBoundsInScreen();
  const gfx::Point drag_start = bounds_in_screen.left_center();
  const gfx::Point drag_end = bounds_in_screen.right_center();

  // Swipe away notification by gesture. Verifies that AppListView still shows.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureScrollSequence(drag_start, drag_end,
                                         base::Microseconds(500), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetAppListView());
  EXPECT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
}

// Verifiy that when showing the launcher, the virtual keyboard dismissed before
// will not show automatically due to the feature called "transient blur" (see
// https://crbug.com/1057320).
TEST_F(AppListControllerImplTest,
       TransientBlurIsNotTriggeredWhenShowingLauncher) {
  // Enable animation.
  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Enable virtual keyboard.
  KeyboardController* const keyboard_controller =
      Shell::Get()->keyboard_controller();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  // Create |window1| which contains a textfield as child view.
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  auto* widget = views::Widget::GetWidgetForNativeView(window1.get());
  std::unique_ptr<views::Textfield> text_field =
      std::make_unique<views::Textfield>();

  // Focusable views need an accessible name to pass the accessibility paint
  // checks.
  text_field->GetViewAccessibility().SetName(u"Name");

  // Note that the bounds of |text_field| cannot be too small. Otherwise, it
  // may not receive the gesture event.
  text_field->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  const auto* text_field_p = text_field.get();
  widget->GetRootView()->AddChildView(std::move(text_field));
  wm::ActivateWindow(window1.get());
  widget->Show();

  // Create |window2|.
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(200, 0, 200, 200));
  window2->Show();

  // Tap at the textfield in |window1|. The virtual keyboard should be visible.
  GestureTapOn(text_field_p);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  // Tap at the center of |window2| to hide the virtual keyboard.
  GetEventGenerator()->GestureTapAt(window2->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(keyboard::test::WaitUntilHidden());

  // Press the home button to show the launcher. Wait for the animation of
  // launcher to finish. Note that the launcher does not exist before toggling
  // the home button.
  PressHomeButton();
  const base::TimeDelta delta = base::Milliseconds(200);
  do {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delta);
    run_loop.Run();
  } while (IsAppListBoundsAnimationRunning());

  // Expect that the virtual keyboard is invisible when the launcher shows.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Regression test for https://crbug.com/1073548
// Verifies that app list shown from overview after toggling tablet mode can be
// closed.
TEST_F(AppListControllerImplTest,
       CloseAppListShownFromOverviewAfterTabletExit) {
  auto* shell = Shell::Get();
  auto* app_list_controller = shell->app_list_controller();
  // Move to tablet mode and back.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ash::TabletModeControllerTestApi().LeaveTabletMode();

  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  EnterOverview();

  // Press home button - verify overview exits and the app list is shown.
  PressHomeButton();

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(app_list_controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(app_list_controller->IsVisible());

  // Pressing home button again should close the app list.
  PressHomeButton();

  EXPECT_FALSE(app_list_controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(app_list_controller->IsVisible());
}

// Tests that swapping out an AppListModel (simulating a profile swap with
// multiprofile enabled) drops all references to previous folders (see
// https://crbug.com/1130901).
TEST_F(AppListControllerImplTest, SimulateProfileSwapNoCrashOnDestruct) {
  // Add a folder, whose AppListItemList the AppListModel will observe.
  AppListModel* model = GetAppListModel();
  const std::string folder_id("folder_1");
  model->CreateFolderItem(folder_id);

  for (int i = 0; i < 2; ++i) {
    auto item = std::make_unique<AppListItem>(base::StringPrintf("app_%d", i));
    model->AddItemToFolder(std::move(item), folder_id);
  }

  // Set a new model, simulating profile switching in multi-profile mode. This
  // should cleanly drop the reference to the folder added earlier.
  auto updated_model = std::make_unique<test::AppListTestModel>();
  auto update_search_model = std::make_unique<SearchModel>();
  auto update_quick_app_access_model = std::make_unique<QuickAppAccessModel>();
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, updated_model.get(), update_search_model.get(),
      update_quick_app_access_model.get());

  Shell::Get()->app_list_controller()->ClearActiveModel();
  updated_model.reset();
  // Test that there is no crash on ~AppListModel() when the test finishes.
}

class AppListControllerImplTestWithNotificationBadging
    : public AppListControllerImplTest {
 public:
  AppListControllerImplTestWithNotificationBadging() = default;
  AppListControllerImplTestWithNotificationBadging(
      const AppListControllerImplTestWithNotificationBadging& other) = delete;
  AppListControllerImplTestWithNotificationBadging& operator=(
      const AppListControllerImplTestWithNotificationBadging& other) = delete;
  ~AppListControllerImplTestWithNotificationBadging() override = default;

  void UpdateAppHasBadge(const std::string& app_id, bool app_has_badge) {
    AppListControllerImpl* controller = Shell::Get()->app_list_controller();
    AccountId account_id = AccountId::FromUserEmail("test@gmail.com");

    apps::App test_app(apps::AppType::kArc, app_id);
    test_app.has_badge = app_has_badge;
    apps::AppUpdate test_update(nullptr, /*delta=*/&test_app, account_id);
    controller->badge_controller_for_test()->OnAppUpdate(test_update);
  }
};

// Tests that when an app has an update to its notification badge, the change
// gets propagated to the corresponding AppListItemView.
TEST_F(AppListControllerImplTestWithNotificationBadging,
       NotificationBadgeUpdateTest) {
  PopulateItem(1);
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* item_view =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  ASSERT_TRUE(item_view);

  const std::string app_id = item_view->item()->id();

  EXPECT_FALSE(item_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge(app_id, /*app_has_badge=*/true);
  EXPECT_TRUE(item_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge(app_id, /*app_has_badge=*/false);
  EXPECT_FALSE(item_view->IsNotificationIndicatorShownForTest());
}

TEST_F(AppListControllerImplTestWithNotificationBadging,
       NotificationBadgeUpdateForFolderTest) {
  std::string folder_id = "folder_1";
  AppListModel* model = GetAppListModel();
  model->CreateFolderItem(folder_id);
  model->AddItemToFolder(std::make_unique<AppListItem>("app_1"), folder_id);
  model->AddItemToFolder(std::make_unique<AppListItem>("app_2"), folder_id);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* folder_view =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  ASSERT_TRUE(folder_view);

  EXPECT_FALSE(folder_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge("app_1", /*app_has_badge=*/true);
  EXPECT_TRUE(folder_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge("app_2", /*app_has_badge=*/true);
  EXPECT_TRUE(folder_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge("app_1", /*app_has_badge=*/false);
  EXPECT_TRUE(folder_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge("app_2", /*app_has_badge=*/false);
  EXPECT_FALSE(folder_view->IsNotificationIndicatorShownForTest());
}

TEST_F(AppListControllerImplTestWithNotificationBadging,
       NotificationBadgeUpdateAfterAddingRemovingAppTest) {
  std::string folder_id = "folder_1";
  AppListModel* model = GetAppListModel();
  model->CreateFolderItem(folder_id);
  AppListItem* app = model->AddItem(std::make_unique<AppListItem>("app_1"));
  model->AddItemToFolder(std::make_unique<AppListItem>("app_2"), folder_id);

  // Give this item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  GetAppListModel()->SetItemName(app, app->id());

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* folder_view =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  ASSERT_TRUE(folder_view);

  EXPECT_FALSE(folder_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge("app_1", /*app_has_badge=*/true);
  EXPECT_FALSE(folder_view->IsNotificationIndicatorShownForTest());

  model->MoveItemToFolder(app, folder_id);
  EXPECT_TRUE(folder_view->IsNotificationIndicatorShownForTest());

  model->MoveItemToRootAt(app, model->FindFolderItem(folder_id)->position());
  EXPECT_FALSE(folder_view->IsNotificationIndicatorShownForTest());
}

// Verifies that the pinned app should still show after canceling the drag from
// AppsGridView to Shelf (https://crbug.com/1021768).
TEST_F(AppListControllerImplTest, DragItemFromAppsGridView) {
  // Turn on the tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  Shelf* const shelf = GetPrimaryShelf();

  // Add icons with the same app id to Shelf and AppsGridView respectively.
  ShelfViewTestAPI shelf_view_test_api(shelf->GetShelfViewForTesting());
  shelf_view_test_api.SetAnimationDuration(base::Milliseconds(1));
  std::string app_id = shelf_view_test_api.AddItem(TYPE_PINNED_APP).app_id;
  AppListItem* item =
      GetAppListModel()->AddItem(std::make_unique<AppListItem>(app_id));

  // Give each item a name so that the accessibility paint checks pass.
  // (Focusable items should have accessible names.)
  GetAppListModel()->SetItemName(item, item->id());

  AppsGridView* apps_grid_view = GetAppsGridView();
  views::test::RunScheduledLayout(apps_grid_view);

  AppListItemView* app_list_item_view =
      test::AppsGridViewTestApi(apps_grid_view).GetViewAtIndex(GridIndex(0, 0));
  views::View* shelf_icon_view =
      shelf->GetShelfViewForTesting()->view_model()->view_at(0);

  // Drag the app icon from AppsGridView to Shelf. Then move the icon back to
  // AppsGridView before drag ends.
  GetEventGenerator()->MoveMouseTo(
      app_list_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  app_list_item_view->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseTo(
      shelf_icon_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->MoveMouseTo(
      apps_grid_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ReleaseLeftButton();

  // The icon's opacity updates at the end of animation.
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  // The icon is pinned before drag starts. So the shelf icon should show in
  // spite that drag is canceled.
  EXPECT_TRUE(shelf_icon_view->GetVisible());
  EXPECT_EQ(1.0f, shelf_icon_view->layer()->opacity());
}

TEST_F(AppListControllerImplTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  std::unique_ptr<aura::Window> w2(CreateTestWindow(
      gfx::Rect(0, 0, 400, 400), aura::client::WINDOW_TYPE_POPUP));

  ash::TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::kMousePressed, ui::VKEY_UNKNOWN, ui::EF_NONE);
  Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMinimized());
}

// Tests that the home screen is visible after rotating the screen in overview
// mode.
TEST_F(AppListControllerImplTest,
       HomeScreenVisibleAfterDisplayUpdateInOverview) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EnterOverview();

  // Trigger a display configuration change, this simulates screen rotation.
  Shell::Get()->app_list_controller()->OnDidApplyDisplayChanges();

  // End overview mode, the home launcher should be visible.
  ExitOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->GetHomeScreenWindow()->IsVisible());
}

TEST_F(AppListControllerImplTest, CreatePage) {
  ShowAppListNow(AppListViewState::kFullscreenAllApps);
  PagedAppsGridView* apps_grid_view = GetAppsGridView();
  test::AppsGridViewTestApi test_api(apps_grid_view);
  PopulateItem(test_api.TilesPerPageInPagedGrid(0));
  EXPECT_EQ(1, apps_grid_view->pagination_model()->total_pages());

  // Add an extra item and verify that the page count is 2 now.
  PopulateItem(1);
  EXPECT_EQ(2, apps_grid_view->pagination_model()->total_pages());
}

TEST_F(AppListControllerImplTest, ShowAppListOpensBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplTest, ToggleAppListOpensBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kShelfButton,
                            /*event_time_stamp=*/{});

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplTest, DismissAppListClosesBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  controller->DismissAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplTest, ShowAppListDoesNotOpenBubbleInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplTest, ToggleAppListDoesNotOpenBubbleInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kShelfButton,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplTest, EnteringTabletModeClosesBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
}

TEST_F(AppListControllerImplTest, WallpaperColorChangeDoesNotCrash) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);
  // Simulate synced wallpaper update while bubble is open.
  controller->OnWallpaperColorsChanged();
  // No crash.
}

TEST_F(AppListControllerImplTest, HideContinueSectionUpdatesPref) {
  auto* controller = Shell::Get()->app_list_controller();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Continue section defaults to not hidden.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kLauncherContinueSectionHidden));
  EXPECT_FALSE(controller->ShouldHideContinueSection());

  // Hiding continue section is reflected in prefs.
  controller->SetHideContinueSection(true);
  EXPECT_TRUE(controller->ShouldHideContinueSection());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kLauncherContinueSectionHidden));

  // Showing continue section is reflected in prefs.
  controller->SetHideContinueSection(false);
  EXPECT_FALSE(controller->ShouldHideContinueSection());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kLauncherContinueSectionHidden));
}

// AppListControllerImpl test that start in inactive session.
class AppListControllerImplNotLoggedInTest : public AppListControllerImplTest {
 public:
  AppListControllerImplNotLoggedInTest() = default;
  ~AppListControllerImplNotLoggedInTest() override = default;

  void SetUp() override {
    AppListControllerImplTest::SetUp();
    SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  }

  void SetSessionState(session_manager::SessionState state) {
    SessionInfo info;
    info.state = state;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }
};

TEST_F(AppListControllerImplNotLoggedInTest, ToggleAppListOnLoginScreen) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Verify app list cannot be toggled in logged in but inactive state.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ShowAppListOnLoginScreen) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Verify app list cannot be toggled in logged in but inactive state.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ToggleAppListInOobe) {
  SetSessionState(session_manager::SessionState::OOBE);
  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ShowAppListInOobe) {
  SetSessionState(session_manager::SessionState::OOBE);
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Verify app list cannot be toggled in logged in but inactive state.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ToggleAppListOnLockScreen) {
  SetSessionState(session_manager::SessionState::ACTIVE);

  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Lock screen - toggling app list should fail.
  SetSessionState(session_manager::SessionState::LOCKED);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Unlock and verify toggling app list works.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kSearchKey,
                            /*event_time_stamp=*/{});

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());

  // Locking the session hides the app list.
  SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ShowAppListOnLockScreen) {
  SetSessionState(session_manager::SessionState::ACTIVE);

  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Lock screen - toggling app list should fail.
  SetSessionState(session_manager::SessionState::LOCKED);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Unlock and verify toggling app list works.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());

  // Locking the session hides the app list.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  controller->ShowAppList(AppListShowSource::kSearchKey);
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ShowAppListWhenInTabletMode) {
  // Enable tablet mode while on login screen.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Fullscreen app list should be shown upon login.
  SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest,
       FullscreenLauncherInTabletModeWhenLocked) {
  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  // Enable tablet mode and lock screen - fullscreen launcher should be shown
  // (behind the lock screen).
  ash::TabletModeControllerTestApi().EnterTabletMode();
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest,
       FullscreenLauncherShownWhenEnteringTabletModeOnLockScreen) {
  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  SetSessionState(session_manager::SessionState::LOCKED);

  // Enable tablet mode and lock screen - fullscreen launcher should be shown
  // (behind the lock screen).
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

// Kiosk tests with the bubble launcher enabled.
class AppListControllerImplKioskTest : public AppListControllerImplTest {
 public:
  AppListControllerImplKioskTest() = default;
  ~AppListControllerImplKioskTest() override = default;

  void SetUp() override {
    AppListControllerImplTest::SetUp();
    SessionInfo info;
    info.is_running_in_app_mode = true;
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }
};

TEST_F(AppListControllerImplKioskTest, ShouldNotShowLauncherInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  EXPECT_FALSE(controller->ShouldHomeLauncherBeVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAnyAppListInClamshellModeWhenShowAppListCalled) {
  auto* controller = Shell::Get()->app_list_controller();

  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAnyAppListInTabletModeWhenShowAppListCalled) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  controller->ShowAppList(AppListShowSource::kSearchKey);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowHomeLauncherInTabletModeWhenOnSessionStateChangedCalled) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  controller->OnSessionStateChanged(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(controller->ShouldHomeLauncherBeVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotMinimizeAppWindowInTabletModeWhenGoHomeCalled) {
  // Emulation of a Kiosk app window.
  std::unique_ptr<aura::Window> w(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  ash::TabletModeControllerTestApi().EnterTabletMode();

  Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());

  EXPECT_FALSE(WindowState::Get(w.get())->IsMinimized());
  EXPECT_TRUE(w->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAppListInTabletModeWhenPressHomeButton) {
  // Emulation of a Kiosk app window.
  std::unique_ptr<aura::Window> w(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  ash::TabletModeControllerTestApi().EnterTabletMode();

  PressHomeButton();

  EXPECT_FALSE(WindowState::Get(w.get())->IsMinimized());
  EXPECT_TRUE(w->IsVisible());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotOpenAnyAppListAfterSwitchingFromTabletMode) {
  auto* controller = Shell::Get()->app_list_controller();
  ash::TabletModeControllerTestApi().EnterTabletMode();

  controller->OnDisplayTabletStateChanged(display::TabletState::kInTabletMode);
  EXPECT_FALSE(controller->IsVisible());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  controller->OnDisplayTabletStateChanged(
      display::TabletState::kInClamshellMode);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

// App list assistant tests.
class AppListControllerWithAssistantTest : public AppListControllerImplTest {
 public:
  AppListControllerWithAssistantTest()
      : assistant_test_api_(AssistantTestApi::Create()) {}
  AppListControllerWithAssistantTest(
      const AppListControllerWithAssistantTest&) = delete;
  AppListControllerWithAssistantTest& operator=(
      const AppListControllerWithAssistantTest&) = delete;
  ~AppListControllerWithAssistantTest() override = default;

  // AppListControllerImplTest:
  void SetUp() override {
    AppListControllerImplTest::SetUp();

    assistant_test_api_->SetAssistantEnabled(true);
    assistant_test_api_->GetAssistantState()->NotifyFeatureAllowed(
        assistant::AssistantAllowedState::ALLOWED);
    assistant_test_api_->GetAssistantState()->NotifyStatusChanged(
        assistant::AssistantStatus::READY);
    assistant_test_api_->WaitUntilIdle();
  }

 protected:
  void ToggleAssistantUiWithAccelerator() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_COMMAND_DOWN);
    EXPECT_TRUE(assistant_test_api_->IsVisible());
  }

  AssistantVisibility GetAssistantVisibility() const {
    return AssistantUiController::Get()->GetModel()->visibility();
  }

  std::unique_ptr<AssistantTestApi> assistant_test_api_;
};

// Verifies the assistant can open and close with the Search-A shortcut.
TEST_F(AppListControllerWithAssistantTest, HotkeySearchA) {
  // Press once to open the assistant.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(assistant_test_api_->IsVisible());
  EXPECT_EQ(GetAssistantVisibility(), AssistantVisibility::kVisible);
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());

  // Press again to close the assistant.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(assistant_test_api_->IsVisible());
  EXPECT_EQ(GetAssistantVisibility(), AssistantVisibility::kClosed);
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

// Verifies the assistant can open and close with the assistant keyboard key.
TEST_F(AppListControllerWithAssistantTest, HotkeyAssistant) {
  // Press once to open the assistant.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ASSISTANT);
  EXPECT_TRUE(assistant_test_api_->IsVisible());
  EXPECT_EQ(GetAssistantVisibility(), AssistantVisibility::kVisible);
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());

  // Press again to close the assistant.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_ASSISTANT);
  EXPECT_FALSE(assistant_test_api_->IsVisible());
  EXPECT_EQ(GetAssistantVisibility(), AssistantVisibility::kClosed);
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

// Verifies the scenario that the Assistant shortcut is triggered when the app
// list close animation is running.
TEST_F(AppListControllerWithAssistantTest,
       TriggerAssistantKeyWhenAppListClosing) {
  // Show the Assistant and verify the app list state.
  ToggleAssistantUiWithAccelerator();
  auto* app_list_controller = Shell::Get()->app_list_controller();
  EXPECT_TRUE(app_list_controller->IsVisible());
  EXPECT_FALSE(AssistantUiController::Get()->HasShownOnboarding());
  EXPECT_EQ(AssistantVisibility::kVisible, GetAssistantVisibility());

  assistant_test_api_->input_text_field()->SetText(u"xyz");
  EXPECT_EQ(u"xyz", assistant_test_api_->input_text_field()->GetText());

  {
    // Enable animation with non-zero duration.
    ui::ScopedAnimationDurationScaleMode non_zero_duration(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // Press the search key. The launcher starts to close.
    PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
    EXPECT_EQ(AssistantVisibility::kClosing, GetAssistantVisibility());

    // Toggle the Assistant ui and wait for app list animation to finish.
    AppListBubbleView* bubble_view =
        app_list_controller->bubble_presenter_for_test()
            ->bubble_view_for_test();
    ToggleAssistantUiWithAccelerator();
    ui::LayerAnimationStoppedWaiter().Wait(bubble_view->layer());
  }

  // Verify that the Assistant ui is visible. In addition, the text in the
  // textfield does not change.
  EXPECT_TRUE(assistant_test_api_->IsVisible());
  EXPECT_EQ(u"xyz", assistant_test_api_->input_text_field()->GetText());
  EXPECT_TRUE(app_list_controller->IsVisible());
  EXPECT_EQ(AssistantVisibility::kVisible, GetAssistantVisibility());

  // Press the search key to close the app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
  EXPECT_FALSE(app_list_controller->IsVisible());

  // Toggle the Assistant ui. The text is still the same in the input field.
  ToggleAssistantUiWithAccelerator();
  EXPECT_TRUE(app_list_controller->IsVisible());
  EXPECT_TRUE(assistant_test_api_->IsVisible());
  EXPECT_EQ(u"xyz", assistant_test_api_->input_text_field()->GetText());
}

// Verifies the scenario that the search key is triggered when the app list
// close animation is running.
TEST_F(AppListControllerWithAssistantTest, TriggerSearchKeyWhenAppListClosing) {
  ToggleAssistantUiWithAccelerator();
  auto* app_list_controller = Shell::Get()->app_list_controller();
  EXPECT_TRUE(app_list_controller->IsVisible());

  // Enable animation with non-zero duration.
  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Press the search key to close the app list.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
  EXPECT_EQ(AssistantVisibility::kClosing, GetAssistantVisibility());

  // Press the search key to reshow the app list.
  AppListBubbleView* bubble_view =
      app_list_controller->bubble_presenter_for_test()->bubble_view_for_test();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
  ui::LayerAnimationStoppedWaiter().Wait(bubble_view->layer());

  // The Assistant should be closed.
  EXPECT_EQ(AssistantVisibility::kClosed, GetAssistantVisibility());
}

TEST_F(AppListControllerWithAssistantTest,
       AppListWindowIsNotShowingOnTopOfOtherApps) {
  CreateAppWindow();
  ash::TabletModeControllerTestApi().EnterTabletMode();

  auto* home_screen_container = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HomeScreenContainer);
  auto* app_list_window = Shell::Get()
                              ->app_list_controller()
                              ->fullscreen_presenter()
                              ->GetView()
                              ->GetWidget()
                              ->GetNativeWindow();

  // Default placement is in home screen container behind other app windows.
  EXPECT_TRUE(home_screen_container->Contains(app_list_window));

  // The app list window shows on top of other app windows when assistant UI is
  // active.
  ToggleAssistantUiWithAccelerator();
  EXPECT_FALSE(home_screen_container->Contains(app_list_window));

  // And stays there during tablet -> clamshell mode transition when assistant
  // UI is active.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(home_screen_container->Contains(app_list_window));

  // Enter tablet mode again. App list window should return to its default
  // position and shouldn't move during transition to clamshell mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(home_screen_container->Contains(app_list_window));
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(home_screen_container->Contains(app_list_window));
}

}  // namespace ash
