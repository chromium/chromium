// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <set>
#include <string>

#include "ash/app_list/app_list_badge_controller.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_impl.h"
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
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
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
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_animation_waiter.h"

namespace ash {

namespace {

void PressHomeButton() {
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks());
}

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
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

SearchBoxView* GetSearchBoxView() {
  return GetContentsView()->GetSearchBoxView();
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
      base::TimeTicks::Now(), /*show_source*/ absl::nullopt);
}

void DismissAppListNow() {
  Shell::Get()->app_list_controller()->fullscreen_presenter()->Dismiss(
      base::TimeTicks::Now());
}

aura::Window* GetAppListViewNativeWindow() {
  return GetAppListView()->GetWidget()->GetNativeView();
}

void EnableTabletMode() {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
}

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;

  bool CreateShelfItemForAppId(
      const std::string& app_id,
      ShelfItem* item,
      std::unique_ptr<ShelfItemDelegate>* delegate) override {
    *item = ShelfItem();
    item->id = ShelfID(app_id);
    *delegate = std::make_unique<TestShelfItemDelegate>(item->id);
    return true;
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

  int CountPageBreakItems() {
    auto* top_list = GetAppListModel()->top_level_item_list();
    int count = 0;
    for (size_t index = 0; index < top_list->item_count(); ++index) {
      if (top_list->item_at(index)->is_page_break())
        ++count;
    }
    return count;
  }

 private:
  // The count of the items created by `PopulateItem()`.
  int populated_item_count_ = 0;

  std::unique_ptr<ShelfItemFactoryFake> shelf_item_factory_;
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
    ShowAppListNow(AppListViewState::kPeeking);
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
  ASSERT_TRUE(GetAppsGridView()->FireDragToShelfTimerForTest());
  GetEventGenerator()->ReleaseLeftButton();
  ASSERT_EQ(1u, shelf_view->view_model()->view_size());

  // Verifies that the dragged item has the correct previous/next focusable
  // view after drag.
  EXPECT_EQ(item1, item2->GetPreviousFocusableView());
  EXPECT_EQ(item3, item2->GetNextFocusableView());
}

TEST_F(AppListControllerImplTest, PageResetByTimerInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  PopulateItem(30);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  PagedAppsGridView* apps_grid_view = GetAppsGridView();
  apps_grid_view->pagination_model()->SelectPage(1, false /* animate */);

  DismissAppListNow();

  // When timer is not skipped the selected page should not change when app list
  // is closed.
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());

  // Skip the page reset timer to simulate timer exipration.
  GetAppListView()->SetSkipPageResetTimerForTesting(true);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());
  DismissAppListNow();

  // Once the app list is closed, the page should be reset when the timer is
  // skipped.
  EXPECT_EQ(0, apps_grid_view->pagination_model()->selected_page());
}

// Verifies that the the virtual keyboard does not get shown if the search box
// is activated by user typing when the app list in the fullscreen state in
// tablet mode.
TEST_F(AppListControllerImplTest, VirtualKeyboardNotShownWhenUserStartsTyping) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

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
  ASSERT_TRUE(keyboard::WaitUntilShown());

  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
}

// Verifies that in tablet mode, the AppListView has correct bounds when the
// virtual keyboard is dismissed (see https://crbug.com/944133).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenDismissVKeyboard) {
  // This isn't relevant with ProductivityLauncher, which uses separate widgets
  // in clamshell versus tablet mode. See bug above. Also, the clamshell
  // launcher closes when transitioning into tablet mode. This test can be
  // deleted when ProductivityLauncher is the default.
  if (features::IsProductivityLauncherEnabled())
    return;

  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse so the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow(AppListViewState::kPeeking);
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Turn on the tablet mode. The virtual keyboard should still show.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Close the virtual keyboard. Wait until it is hidden.
  Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Check the following things:
  // (1) AppListView's state is FULLSCREEN_SEARCH
  // (2) AppListView's bounds are the same as the preferred bounds for
  // the FULLSCREEN_SEARCH state.
  EXPECT_EQ(AppListViewState::kFullscreenSearch,
            GetAppListView()->app_list_state());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                AppListViewState::kFullscreenSearch),
            GetAppListViewNativeWindow()->bounds());
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
// TODO(crbug.com/1120501): Test is flaky on ASAN builds.
TEST_F(AppListControllerImplTest, MAYBE_CloseNotificationWithAppListShown) {
  ShowAppListNow(AppListViewState::kPeeking);

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
  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  text_field->SetProperty(views::kSkipAccessibilityPaintChecks, true);

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
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap at the center of |window2| to hide the virtual keyboard.
  GetEventGenerator()->GestureTapAt(window2->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(keyboard::WaitUntilHidden());

  // Press the home button to show the launcher. Wait for the animation of
  // launcher to finish. Note that the launcher does not exist before toggling
  // the home button.
  PressHomeButton();
  const base::TimeDelta delta = base::Milliseconds(200);
  do {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  // This test is not relevant for ProductivityLauncher because it uses separate
  // widgets in clamshell and tablet mode. This test can be deleted when
  // ProductivityLauncher is the default.
  if (features::IsProductivityLauncherEnabled())
    return;

  auto* shell = Shell::Get();
  auto* tablet_mode_controller = shell->tablet_mode_controller();
  // Move to tablet mode and back.
  tablet_mode_controller->SetEnabledForTest(true);
  tablet_mode_controller->SetEnabledForTest(false);

  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  EnterOverview();

  // Press home button - verify overview exits and the app list is shown.
  PressHomeButton();

  EXPECT_FALSE(shell->overview_controller()->InOverviewSession());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
  GetAppListTestHelper()->CheckVisibility(true);
  ASSERT_TRUE(GetAppListView()->GetWidget());
  EXPECT_TRUE(GetAppListView()->GetWidget()->GetNativeWindow()->IsVisible());

  // Pressing home button again should close the app list.
  PressHomeButton();

  EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  GetAppListTestHelper()->CheckVisibility(false);
  ASSERT_TRUE(GetAppListView()->GetWidget());
  EXPECT_FALSE(GetAppListView()->GetWidget()->GetNativeWindow()->IsVisible());
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
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, updated_model.get(), update_search_model.get());

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
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  Shelf* const shelf = GetPrimaryShelf();

  // Add icons with the same app id to Shelf and AppsGridView respectively.
  ShelfViewTestAPI shelf_view_test_api(shelf->GetShelfViewForTesting());
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

// Verifies that apps grid and hotseat bounds do not overlap when switching from
// side shelf app list to tablet mode.
TEST_F(AppListControllerImplTest, NoOverlapWithHotseatOnSwitchFromSideShelf) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  Shelf* const shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kRight);
  ShowAppListNow(AppListViewState::kFullscreenAllApps);
  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  gfx::Rect apps_grid_view_bounds = GetAppsGridView()->GetBoundsInScreen();
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->shelf_widget()->GetWindowBoundsInScreen()));
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->hotseat_widget()->GetWindowBoundsInScreen()));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  apps_grid_view_bounds = GetAppsGridView()->GetBoundsInScreen();
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->shelf_widget()->GetWindowBoundsInScreen()));
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->hotseat_widget()->GetWindowBoundsInScreen()));
}

TEST_F(AppListControllerImplTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  std::unique_ptr<aura::Window> w2(CreateTestWindow(
      gfx::Rect(0, 0, 400, 400), aura::client::WINDOW_TYPE_POPUP));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMinimized());
}

// Tests that the home screen is visible after rotating the screen in overview
// mode.
TEST_F(AppListControllerImplTest,
       HomeScreenVisibleAfterDisplayUpdateInOverview) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EnterOverview();

  // Trigger a display configuration change, this simulates screen rotation.
  Shell::Get()->app_list_controller()->OnDisplayConfigurationChanged();

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
  PopulateItem(test_api.TilesPerPage(0));
  EXPECT_EQ(1, apps_grid_view->pagination_model()->total_pages());

  // Add an extra item and verify that the page count is 2 now.
  PopulateItem(1);
  EXPECT_EQ(2, apps_grid_view->pagination_model()->total_pages());

  // Verify that there is no page break items.
  EXPECT_EQ(0, CountPageBreakItems());
}

// The test parameter indicates whether the shelf should auto-hide. In either
// case the animation behaviors should be the same.
class AppListAnimationTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  AppListAnimationTest() = default;

  AppListAnimationTest(const AppListAnimationTest&) = delete;
  AppListAnimationTest& operator=(const AppListAnimationTest&) = delete;

  ~AppListAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    Shelf* const shelf = AshTestBase::GetPrimaryShelf();
    shelf->SetAlignment(ShelfAlignment::kBottom);

    if (GetParam()) {
      shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
    }

    // The shelf should be shown at this point despite auto hide behavior, given
    // that no windows are shown.
    shown_shelf_bounds_ = shelf->shelf_widget()->GetWindowBoundsInScreen();
  }

  int GetAppListCurrentTop() {
    gfx::Point app_list_top =
        GetAppListView()->GetBoundsInScreen().top_center();
    GetAppListView()->GetWidget()->GetLayer()->transform().TransformPoint(
        &app_list_top);
    return app_list_top.y();
  }

  int GetAppListTargetTop() {
    gfx::Point app_list_top =
        GetAppListView()->GetBoundsInScreen().top_center();
    GetAppListView()
        ->GetWidget()
        ->GetLayer()
        ->GetTargetTransform()
        .TransformPoint(&app_list_top);
    return app_list_top.y();
  }

  int shown_shelf_top() const { return shown_shelf_bounds_.y(); }

  // The offset that should be animated between kFullscreenAllApps and kClosed
  // app list view states - the vertical distance between shelf top (in shown
  // state) and the app list top in fullscreen state.
  int FullscreenHeightOffset() const {
    return shown_shelf_bounds_.y() - FullscreenHeightTop();
  }

  // The app list view y coordinate in peeking state.
  int FullscreenHeightTop() const {
    return shown_shelf_bounds_.bottom() -
           GetAppListView()->GetHeightForState(
               AppListViewState::kFullscreenAllApps);
  }

 private:
  // Set during setup.
  gfx::Rect shown_shelf_bounds_;
};

INSTANTIATE_TEST_SUITE_P(AutoHideShelf, AppListAnimationTest, testing::Bool());

// Tests app list animation to fullscreen state.
TEST_P(AppListAnimationTest, AppListShowFullscreenAnimation) {
  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  // Verify that the app list view's top matches the shown shelf top as the show
  // animation starts.
  EXPECT_EQ(shown_shelf_top(), GetAppListCurrentTop());
  EXPECT_EQ(FullscreenHeightTop(), GetAppListTargetTop());
}

// Tests app list animation from fullscreen to closed state.
TEST_P(AppListAnimationTest, AppListCloseFromFullscreenAnimation) {
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Dismiss app list, initial app list position should be at fullscreen height.
  const int offset_to_animate = FullscreenHeightOffset();
  DismissAppListNow();
  EXPECT_EQ(shown_shelf_top() - offset_to_animate, GetAppListCurrentTop());
  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());
}

// Tests app list close animation when app list gets dismissed while animating
// to fullscreen state.
TEST_P(AppListAnimationTest, AppListDismissWhileShowingFullscreen) {
  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  // Verify that the app list view's top matches the shown shelf top as the show
  // animation starts.
  EXPECT_EQ(shown_shelf_top(), GetAppListCurrentTop());
  EXPECT_EQ(FullscreenHeightTop(), GetAppListTargetTop());

  // Start dismissing app list. Verify the new animation starts at the same
  // point the show animation ended.
  DismissAppListNow();

  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());
}

// Tests app list animation when show is requested while app list close
// animation is in progress.
TEST_P(AppListAnimationTest, AppListShowFullscreenWhileClosing) {
  // Show app list while animations are still instantanious.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  int offset_to_animate = FullscreenHeightOffset();
  DismissAppListNow();

  // Verify that the app list view's top initially matches the fullscreen
  // height.
  EXPECT_EQ(shown_shelf_top() - offset_to_animate, GetAppListCurrentTop());
  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());

  // Start showing the app list. Verify the new animation starts at the same
  // point the show animation ended.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  EXPECT_EQ(FullscreenHeightTop(), GetAppListTargetTop());
}

// Tests that how search box opacity is animated when the app list is shown and
// closed.
TEST_P(AppListAnimationTest, SearchBoxOpacityDuringShowAndClose) {
  // Set a transition duration that prevents the app list view from snapping to
  // the final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  SearchBoxView* const search_box = GetSearchBoxView();

  // The search box opacity should start  at 0, and animate to 1.
  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());

  // If the app list is closed while the animation is still in progress, the
  // search box opacity should animate from the current opacity.
  DismissAppListNow();

  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(0.0f, search_box->layer()->GetTargetOpacity());

  search_box->layer()->GetAnimator()->StopAnimating();

  // When show again, verify the app list animates from 0 opacity again.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());

  search_box->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(1.0f, search_box->layer()->opacity());

  // Search box opacity animates from the current (full opacity) when closed
  // from shown state.
  DismissAppListNow();

  EXPECT_EQ(1.0f, search_box->layer()->opacity());
  EXPECT_EQ(0.0f, search_box->layer()->GetTargetOpacity());

  // If the app list is show again during close animation, the search box
  // opacity should animate from the current value.
  ShowAppListNow(AppListViewState::kFullscreenAllApps);

  EXPECT_EQ(1.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());
}

class AppListControllerImplMetricsTest : public AshTestBase {
 public:
  AppListControllerImplMetricsTest() = default;

  AppListControllerImplMetricsTest(const AppListControllerImplMetricsTest&) =
      delete;
  AppListControllerImplMetricsTest& operator=(
      const AppListControllerImplMetricsTest&) = delete;

  ~AppListControllerImplMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->app_list_controller();
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }

  void TearDown() override {
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    AshTestBase::TearDown();
  }

  AppListControllerImpl* controller_;
  const base::HistogramTester histogram_tester_;
};

// Tests with the bubble launcher enabled. This is a separate test suite
// because the feature must be enabled before ash::Shell constructs the
// AppListControllerImpl.
class AppListControllerImplAppListBubbleTest : public AshTestBase {
 public:
  AppListControllerImplAppListBubbleTest() {
    scoped_features_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~AppListControllerImplAppListBubbleTest() override = default;

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AppListControllerImplAppListBubbleTest, ShowAppListOpensBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList();

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplAppListBubbleTest, ToggleAppListOpensBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kShelfButton,
                            /*event_time_stamp=*/{});

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplAppListBubbleTest, DismissAppListClosesBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList();

  controller->DismissAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplAppListBubbleTest,
       ShowAppListDoesNotOpenBubbleInTabletMode) {
  EnableTabletMode();

  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplAppListBubbleTest,
       ToggleAppListDoesNotOpenBubbleInTabletMode) {
  EnableTabletMode();

  auto* controller = Shell::Get()->app_list_controller();
  controller->ToggleAppList(GetPrimaryDisplay().id(),
                            AppListShowSource::kShelfButton,
                            /*event_time_stamp=*/{});

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->IsVisible());
}

TEST_F(AppListControllerImplAppListBubbleTest, EnteringTabletModeClosesBubble) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList();

  EnableTabletMode();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
}

TEST_F(AppListControllerImplAppListBubbleTest,
       WallpaperColorChangeDoesNotCrash) {
  auto* controller = Shell::Get()->app_list_controller();
  controller->ShowAppList();
  // Simulate synced wallpaper update while bubble is open.
  controller->OnWallpaperColorsChanged();
  // No crash.
}

TEST_F(AppListControllerImplAppListBubbleTest, HideContinueSectionUpdatesPref) {
  base::test::ScopedFeatureList feature_list(
      features::kLauncherHideContinueSection);

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
class AppListControllerImplNotLoggedInTest
    : public AppListControllerImplAppListBubbleTest {
 public:
  AppListControllerImplNotLoggedInTest() = default;
  ~AppListControllerImplNotLoggedInTest() override = default;

  void SetUp() override {
    AppListControllerImplAppListBubbleTest::SetUp();
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
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Verify app list cannot be toggled in logged in but inactive state.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList();

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
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Verify app list cannot be toggled in logged in but inactive state.
  SetSessionState(session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Toggle app list works when session is active.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList();

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
  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  // Unlock and verify toggling app list works.
  SetSessionState(session_manager::SessionState::ACTIVE);
  controller->ShowAppList();

  EXPECT_TRUE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());

  // Locking the session hides the app list.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  controller->ShowAppList();
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplNotLoggedInTest, ShowAppListWhenInTabletMode) {
  // Enable tablet mode while on login screen.
  EnableTabletMode();

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
  EnableTabletMode();
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
  EnableTabletMode();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_FALSE(controller->IsVisible());

  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_TRUE(controller->fullscreen_presenter()->GetTargetVisibility());
  EXPECT_TRUE(controller->IsVisible());
}

// Kiosk tests with the bubble launcher enabled.
class AppListControllerImplKioskTest
    : public AppListControllerImplAppListBubbleTest {
 public:
  AppListControllerImplKioskTest() = default;
  ~AppListControllerImplKioskTest() override = default;

  void SetUp() override {
    AppListControllerImplAppListBubbleTest::SetUp();
    SessionInfo info;
    info.is_running_in_app_mode = true;
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }
};

TEST_F(AppListControllerImplKioskTest, ShouldNotShowLauncherInTabletMode) {
  EnableTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  EXPECT_FALSE(controller->ShouldHomeLauncherBeVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAnyAppListInClamshellModeWhenShowAppListCalled) {
  auto* controller = Shell::Get()->app_list_controller();

  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAnyAppListInTabletModeWhenShowAppListCalled) {
  EnableTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  controller->ShowAppList();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowHomeLauncherInTabletModeWhenOnSessionStateChangedCalled) {
  EnableTabletMode();
  auto* controller = Shell::Get()->app_list_controller();

  controller->OnSessionStateChanged(session_manager::SessionState::ACTIVE);

  EXPECT_FALSE(controller->ShouldHomeLauncherBeVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotMinimizeAppWindowInTabletModeWhenGoHomeCalled) {
  // Emulation of a Kiosk app window.
  std::unique_ptr<aura::Window> w(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  EnableTabletMode();

  Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());

  EXPECT_FALSE(WindowState::Get(w.get())->IsMinimized());
  EXPECT_TRUE(w->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotShowAppListInTabletModeWhenPressHomeButton) {
  // Emulation of a Kiosk app window.
  std::unique_ptr<aura::Window> w(CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  EnableTabletMode();

  PressHomeButton();

  EXPECT_FALSE(WindowState::Get(w.get())->IsMinimized());
  EXPECT_TRUE(w->IsVisible());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

TEST_F(AppListControllerImplKioskTest,
       DoNotOpenAnyAppListAfterSwitchingFromTabletMode) {
  auto* controller = Shell::Get()->app_list_controller();
  EnableTabletMode();

  controller->OnTabletModeStarted();
  EXPECT_FALSE(controller->IsVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  controller->OnTabletModeEnded();

  EXPECT_FALSE(controller->bubble_presenter_for_test()->IsShowing());
  EXPECT_FALSE(controller->IsVisible());
}

// App list assistant tests, parameterized by ProductivityLauncher.
class AppListControllerWithAssistantTest
    : public AppListControllerImplTest,
      public testing::WithParamInterface<bool> {
 public:
  AppListControllerWithAssistantTest()
      : assistant_test_api_(AssistantTestApi::Create()) {
    feature_list_.InitWithFeatureState(features::kProductivityLauncher,
                                       GetParam());
  }
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
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         AppListControllerWithAssistantTest,
                         testing::Bool());

// Verifies the scenario that the Assistant shortcut is triggered when the the
// app list close animation is running.
TEST_P(AppListControllerWithAssistantTest,
       TriggerAssistantKeyWhenAppListClosing) {
  // Show the Assistant and verify the app list state.
  ToggleAssistantUiWithAccelerator();
  auto* app_list_controller = Shell::Get()->app_list_controller();
  EXPECT_TRUE(app_list_controller->IsVisible());
  EXPECT_TRUE(AssistantUiController::Get()->HasShownOnboarding());
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
    if (features::IsProductivityLauncherEnabled()) {
      AppListBubbleView* bubble_view =
          app_list_controller->bubble_presenter_for_test()
              ->bubble_view_for_test();
      ToggleAssistantUiWithAccelerator();
      ui::LayerAnimationStoppedWaiter().Wait(bubble_view->layer());
    } else {
      views::WidgetAnimationWaiter waiter(GetAppListView()->GetWidget());
      ToggleAssistantUiWithAccelerator();
      waiter.WaitForAnimation();
    }
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

  // Toggle the Assistant ui. The text input field should be cleared.
  ToggleAssistantUiWithAccelerator();
  EXPECT_TRUE(app_list_controller->IsVisible());
  // TODO(jamescook): Decide if we want this behavior for ProductivityLauncher.
  if (!features::IsProductivityLauncherEnabled())
    EXPECT_TRUE(assistant_test_api_->input_text_field()->GetText().empty());
}

// Verifies the scenario that the search key is triggered when the the app list
// close animation is running.
TEST_P(AppListControllerWithAssistantTest, TriggerSearchKeyWhenAppListClosing) {
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
  if (features::IsProductivityLauncherEnabled()) {
    AppListBubbleView* bubble_view =
        app_list_controller->bubble_presenter_for_test()
            ->bubble_view_for_test();
    PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
    ui::LayerAnimationStoppedWaiter().Wait(bubble_view->layer());
  } else {
    views::WidgetAnimationWaiter waiter(GetAppListView()->GetWidget());
    PressAndReleaseKey(ui::KeyboardCode::VKEY_COMMAND);
    waiter.WaitForAnimation();
  }

  // The Assistant should be closed.
  EXPECT_EQ(AssistantVisibility::kClosed, GetAssistantVisibility());
}

}  // namespace ash
