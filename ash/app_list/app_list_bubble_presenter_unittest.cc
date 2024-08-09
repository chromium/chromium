// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <set>
#include <string>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/display.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

// Distance under which two points are considered "near" each other.
constexpr int kNearDistanceDips = 20;

// The exact position of a bubble relative to its anchor is an implementation
// detail, so tests assert that points are "near" each other. This also makes
// the tests less fragile if padding changes.
testing::AssertionResult IsNear(const gfx::Point& a, const gfx::Point& b) {
  gfx::Vector2d delta = a - b;
  float distance = delta.Length();
  if (distance < float{kNearDistanceDips})
    return testing::AssertionSuccess();

  return testing::AssertionFailure()
         << a.ToString() << " is more than " << kNearDistanceDips
         << " dips away from " << b.ToString();
}

// Returns the number of widgets in the app list container for `display_id`.
size_t NumberOfWidgetsInAppListContainer(int64_t display_id) {
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  aura::Window* container =
      Shell::GetContainer(root, kShellWindowId_AppListContainer);
  std::set<raw_ptr<views::Widget, SetExperimental>> widgets;
  views::Widget::GetAllChildWidgets(container, &widgets);
  return widgets.size();
}

class AppListBubblePresenterTest : public AshTestBase {
 public:
  AppListBubblePresenterTest()
      : assistant_test_api_(AssistantTestApi::Create()) {}
  ~AppListBubblePresenterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay(GetInitialScreenConfig());
  }

  virtual std::string GetInitialScreenConfig() const {
    // Use a realistic screen size so the default size bubble will fit.
    return "1366x768";
  }
  // Returns the presenter instance. Use this instead of creating a new
  // presenter instance in each test to avoid situations where two bubbles
  // exist at the same time (the per-test one and the "production" one).
  AppListBubblePresenter* GetBubblePresenter() {
    return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  }

  void AddAppItems(int num_items) {
    GetAppListTestHelper()->AddAppItems(num_items);
    auto* presenter = GetBubblePresenter();
    if (presenter->bubble_widget_for_test()) {
      // Widget is cached between shows, so adding apps may require layout.
      presenter->bubble_widget_for_test()->LayoutRootViewIfNecessary();
    }
  }

  std::unique_ptr<AssistantTestApi> assistant_test_api_;
};

// Tests that verify app list bubble bounds. Parameterized by whether launcher
// is shown on primary or secondary display.
enum class AppListBubbleBoundsTestType {
  kSingleDisplay,
  kPrimaryDisplay,
  kSecondaryDisplay
};
class AppListBubbleBoundsTest
    : public AppListBubblePresenterTest,
      public ::testing::WithParamInterface<AppListBubbleBoundsTestType> {
 public:
  AppListBubbleBoundsTest() = default;
  ~AppListBubbleBoundsTest() override = default;

  // AppListBubblePresenterTest:
  std::string GetInitialScreenConfig() const override {
    switch (GetParam()) {
      case AppListBubbleBoundsTestType::kSingleDisplay:
        return "1366x768";
      case AppListBubbleBoundsTestType::kPrimaryDisplay:
        return "1366x768,768x1366";
      case AppListBubbleBoundsTestType::kSecondaryDisplay:
        return "768x1366,1366x768";
    }
  }

  void SetTestDisplaySize(const std::string& config) {
    switch (GetParam()) {
      case AppListBubbleBoundsTestType::kSingleDisplay:
        UpdateDisplay(config);
        return;
      case AppListBubbleBoundsTestType::kPrimaryDisplay:
        UpdateDisplay(base::StringPrintf("%s,769x1366", config.c_str()));
        return;
      case AppListBubbleBoundsTestType::kSecondaryDisplay:
        UpdateDisplay(base::StringPrintf("769x1366,%s", config.c_str()));
        return;
    }
  }

  display::Display GetTestDisplay() const {
    switch (GetParam()) {
      case AppListBubbleBoundsTestType::kSingleDisplay:
      case AppListBubbleBoundsTestType::kPrimaryDisplay:
        return GetPrimaryDisplay();
      case AppListBubbleBoundsTestType::kSecondaryDisplay:
        return GetSecondaryDisplay();
    }
  }

  int64_t GetTestDisplayId() const { return GetTestDisplay().id(); }

  Shelf* GetShelfForTestDisplay() {
    return Shell::GetRootWindowControllerWithDisplayId(GetTestDisplayId())
        ->shelf();
  }

  gfx::Rect GetShelfBounds() {
    return GetShelfForTestDisplay()->shelf_widget()->GetWindowBoundsInScreen();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppListBubbleBoundsTest,
    testing::ValuesIn({AppListBubbleBoundsTestType::kSingleDisplay,
                       AppListBubbleBoundsTestType::kPrimaryDisplay,
                       AppListBubbleBoundsTestType::kSecondaryDisplay}));

TEST_F(AppListBubblePresenterTest, ShowOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ShowStartsZeroStateSearch) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());

  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(2, GetTestAppListClient()->start_zero_state_search_count());
}

TEST_F(AppListBubblePresenterTest, ShowRecordsCreationTimeHistogram) {
  base::HistogramTester histogram_tester;
  AppListBubblePresenter* presenter = GetBubblePresenter();

  presenter->Show(GetPrimaryDisplay().id());
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 1);

  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  // The widget is cached, so the metric was not recorded again.
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 1);
}

TEST_F(AppListBubblePresenterTest, ShowOnSecondaryDisplay) {
  UpdateDisplay("1600x1200,1366x768");

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  presenter->Show(GetSecondaryDisplay().id());
  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ToggleWithHomeButtonOnSecondaryDisplay) {
  // Set up 2 displays.
  UpdateDisplay("1366x768,1920x1080");

  // Show and hide the widget on the primary display. This forces it to be
  // cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));

  // Click the home button on the secondary display.
  aura::Window* root =
      Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id());
  HomeButton* home_button =
      Shelf::ForWindow(root)->navigation_widget()->GetHomeButton();
  LeftClickOn(home_button);

  // Widget is shown.
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));

  // Click the home button again.
  LeftClickOn(home_button);

  // Widget is hidden.
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ShowAfterDisconnectingDisplay) {
  // Set up 2 displays.
  UpdateDisplay("1366x768,1920x1080");

  // Show and hide the widget on the secondary display. This forces it to be
  // cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetSecondaryDisplay().id());
  presenter->Dismiss();
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetSecondaryDisplay().id()));

  // Disconnect the secondary monitor.
  UpdateDisplay("1366x768");

  // Show the widget on the primary display.
  presenter->Show(GetPrimaryDisplay().id());

  // Widget is shown (and no crash).
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ToggleByFocusingWindowOnSecondaryDisplay) {
  UpdateDisplay("1600x1200,1366x768");

  std::unique_ptr<views::Widget> primary_display_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> secondary_display_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  secondary_display_widget->SetBounds(
      gfx::Rect(gfx::Point(1600, 0), gfx::Size(1366, 768)));

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Activate the window in secondary display, and verify the app list bubble
  // gets hidden.
  secondary_display_widget->Activate();

  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(secondary_display_widget->IsActive());
}

TEST_F(AppListBubblePresenterTest, DismissHidesWidget) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  presenter->Dismiss();

  // The widget still exists. It was cached for performance.
  views::Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(AppListBubblePresenterTest, DismissWhenNotShowingDoesNotCrash) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_FALSE(presenter->IsShowing());

  presenter->Dismiss();
  // No crash.
}

TEST_F(AppListBubblePresenterTest, ToggleOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));
}

TEST_F(AppListBubblePresenterTest, ToggleHidesWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());
  ASSERT_EQ(1u, NumberOfWidgetsInAppListContainer(GetPrimaryDisplay().id()));

  presenter->Toggle(GetPrimaryDisplay().id());

  views::Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingByDefault) {
  AppListBubblePresenter* presenter = GetBubblePresenter();

  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, BubbleIsShowingAfterShow) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingAfterDismiss) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, BubbleDoesNotCloseWhenShelfFocused) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Press Alt-Shift-L to focus the home button on the shelf.
  PressAndReleaseKey(ui::VKEY_L, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(presenter->GetWindow());
}

TEST_F(AppListBubblePresenterTest, CanShowWhileAnimatingClosed) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  presenter->Dismiss();
  // Widget is not considered showing because it is animating closed.
  EXPECT_FALSE(presenter->IsShowing());
  // Widget is still visible because the animation is still playing.
  EXPECT_TRUE(presenter->bubble_widget_for_test()->IsVisible());

  // Attempt to abort the dismiss by showing again.
  presenter->Show(GetPrimaryDisplay().id());

  // Widget shows.
  EXPECT_TRUE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1302026
TEST_F(AppListBubblePresenterTest, DismissWhileWaitingForZeroStateSearch) {
  // Simulate production behavior for animations and zero-state search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_EQ(1, GetTestAppListClient()->start_zero_state_search_count());
  EXPECT_EQ(0, GetTestAppListClient()->zero_state_search_done_count());

  // Toggle while the code is waiting for the zero-state results. This results
  // in a Dismiss(), and the widget is not created.
  presenter->Toggle(GetPrimaryDisplay().id());
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->bubble_widget_for_test());

  // Wait for the zero-state search callback to run. Widget is not created.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetTestAppListClient()->zero_state_search_done_count());
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->bubble_widget_for_test());

  // Toggle again should Show() and create the widget.
  presenter->Toggle(GetPrimaryDisplay().id());
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);
  EXPECT_TRUE(presenter->IsShowing());
  ASSERT_TRUE(presenter->bubble_widget_for_test());
  EXPECT_TRUE(presenter->bubble_widget_for_test()->IsVisible());

  // Toggle one last time should Dismiss() and hide the widget.
  presenter->Toggle(GetPrimaryDisplay().id());
  ui::LayerAnimationStoppedWaiter().Wait(
      presenter->bubble_view_for_test()->layer());
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_FALSE(presenter->bubble_widget_for_test()->IsVisible());
}

// Regression test for https://crbug.com/1275755
TEST_F(AppListBubblePresenterTest, AssistantKeyOpensToAssistantPage) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_TRUE(presenter->IsShowingEmbeddedAssistantUI());

  views::View* progress_indicator =
      presenter->bubble_view_for_test()->GetViewByID(
          AssistantViewID::kProgressIndicator);
  EXPECT_FLOAT_EQ(0.f, progress_indicator->layer()->opacity());

  // Check target opacity as footer is animating.
  views::View* footer = presenter->bubble_view_for_test()->GetViewByID(
      AssistantViewID::kFooterView);
  EXPECT_FLOAT_EQ(1.f, footer->layer()->GetTargetOpacity());
}

TEST_F(AppListBubblePresenterTest, AssistantKeyOpensAssistantPageWhenCached) {
  // Show and hide the widget to force it to be cached.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_TRUE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, AppsPageVisibleAfterShowingAssistant) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  // Show the assistant.
  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Hide the assistant.
  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  base::RunLoop().RunUntilIdle();

  AppListBubblePresenter* presenter = GetBubblePresenter();
  ASSERT_FALSE(presenter->IsShowing());

  // Show the launcher.
  PressAndReleaseKey(ui::VKEY_BROWSER_SEARCH);
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Apps page is visible, even though it was hidden when showing assistant.
  EXPECT_TRUE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_FALSE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, SearchKeyOpensToAppsPage) {
  // Simulate production behavior for animations, assistant, and zero-state
  // search results.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  assistant_test_api_->EnableAssistantAndWait();
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  PressAndReleaseKey(ui::VKEY_LWIN);  // Search key.
  AppListTestApi().WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(
      presenter->bubble_view_for_test()->apps_page_for_test()->GetVisible());
  EXPECT_FALSE(presenter->IsShowingEmbeddedAssistantUI());
}

TEST_F(AppListBubblePresenterTest, SearchFieldHasFocusAfterAssistantPageShown) {
  // Search box takes focus on show.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  auto* search_box_view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Switch to assistant page. Search box loses focus.
  presenter->ShowEmbeddedAssistantUI();
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());

  // The widget is still open, but hidden.
  presenter->Dismiss();
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());

  // Focus returns to the main search box on show.
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
}

TEST_F(AppListBubblePresenterTest, DoesNotCrashWhenNativeWidgetDestroyed) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  ASSERT_EQ(1u, container->children().size());
  aura::Window* native_window = container->children()[0];
  delete native_window;
  // No crash.

  // Trigger an event that would normally be handled by the event filter.
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();
  // No crash.
}

TEST_F(AppListBubblePresenterTest, ClickInTopLeftOfScreenClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  ASSERT_FALSE(widget->GetWindowBoundsInScreen().Contains(0, 0));
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(presenter->IsShowing());
}

// Verifies that the launcher does not reopen when it's closed by a click on the
// home button.
TEST_F(AppListBubblePresenterTest, ClickOnHomeButtonClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());

  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1237264.
TEST_F(AppListBubblePresenterTest, ClickInCornerOfScreenClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click the bottom left corner of the screen.
  GetEventGenerator()->MoveMouseTo(GetPrimaryDisplay().bounds().bottom_left());
  GetEventGenerator()->ClickLeftButton();

  // Bubble is closed (and did not reopen).
  EXPECT_FALSE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1268220.
TEST_F(AppListBubblePresenterTest, CreatingActiveWidgetClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Create a new widget, which will activate itself and deactivate the bubble.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();
  EXPECT_TRUE(widget->IsActive());

  // Bubble is closed.
  EXPECT_FALSE(presenter->IsShowing());
}

// Verifies that a child window of the help bubble container can gain focus
// from the app list bubble without closing the bubble.
TEST_F(AppListBubblePresenterTest, FocusHelpBubbleContainerChild) {
  AppListBubblePresenter* const presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  ASSERT_TRUE(presenter->IsShowing());

  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      /*delegate=*/nullptr, kShellWindowId_HelpBubbleContainer);
  EXPECT_TRUE(widget->GetNativeView()->HasFocus());

  // Bubble is shown without focus.
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(
      presenter->bubble_widget_for_test()->GetNativeView()->HasFocus());
}

// Regression test for https://crbug.com/1268220.
TEST_F(AppListBubblePresenterTest, CreatingChildWidgetDoesNotCloseBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Create a new widget parented to the bubble, similar to an app uninstall
  // confirmation dialog.
  aura::Window* bubble_window =
      presenter->bubble_widget_for_test()->GetNativeWindow();
  std::unique_ptr<views::Widget> widget = TestWidgetBuilder()
                                              .SetShow(true)
                                              .SetParent(bubble_window)
                                              .BuildOwnsNativeWidget();

  // Bubble stays open.
  EXPECT_TRUE(presenter->IsShowing());

  // Close the widget.
  widget.reset();

  // Bubble stays open.
  EXPECT_TRUE(presenter->IsShowing());
}

// Regression test for https://crbug.com/1285443.
TEST_F(AppListBubblePresenterTest, CanOpenBubbleThenOpenSystemTray) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a widget, which will activate itself when the launcher closes.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();

  // Show the launcher.
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click on the system tray.
  LeftClickOn(GetPrimaryUnifiedSystemTray());

  // Wait for launcher animations to end.
  ui::LayerAnimationStoppedWaiter().Wait(
      presenter->bubble_view_for_test()->layer());

  // Launcher is closed and system tray is open.
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

class AppListBubblePresenterFocusFollowsCursorTest
    : public AppListBubblePresenterTest {
 public:
  AppListBubblePresenterFocusFollowsCursorTest() {
    scoped_features_.InitAndEnableFeature(::features::kFocusFollowsCursor);
  }
  ~AppListBubblePresenterFocusFollowsCursorTest() override = default;

  base::test::ScopedFeatureList scoped_features_;
};

// Regression test for https://crbug.com/1316250.
TEST_F(AppListBubblePresenterFocusFollowsCursorTest,
       HoverOverWindowDoesNotHideBubble) {
  // Create a widget, which will activate itself when the launcher closes.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder()
          .SetBounds(gfx::Rect(gfx::Point(1, 1), gfx::Size(100, 100)))
          .SetShow(true)
          .BuildOwnsNativeWidget();

  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(widget->IsActive());

  // Show the bubble and verify that it is active.
  presenter->Show(GetPrimaryDisplay().id());
  views::Widget* bubble_widget = presenter->bubble_widget_for_test();
  auto* search_box_view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  GetEventGenerator()->MoveMouseTo(
      bubble_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(bubble_widget->IsActive());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(widget->IsActive());

  // Move the mouse onto an empty space. Activation of the bubble shouldn't be
  // lost.
  ASSERT_FALSE(widget->GetWindowBoundsInScreen().Contains(0, 0));
  ASSERT_FALSE(bubble_widget->GetWindowBoundsInScreen().Contains(0, 0));
  GetEventGenerator()->MoveMouseTo(0, 0);
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(bubble_widget->IsActive());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(widget->IsActive());

  // Move the mouse onto the window. Verify that the bubble is still showing,
  // but activation has moved to the window.
  gfx::Point widget_center = widget->GetWindowBoundsInScreen().CenterPoint();
  ASSERT_FALSE(
      bubble_widget->GetWindowBoundsInScreen().Contains(widget_center));
  GetEventGenerator()->MoveMouseTo(widget_center);
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(bubble_widget->IsActive());
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(widget->IsActive());

  // Verify that user inputs do not go to the bubble search box while it is not
  // focused even though the bubble is showing.
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_B);
  PressAndReleaseKey(ui::VKEY_C);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());

  // Clicking outside the bubble should close it.
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(widget->IsActive());
}

// Tests that creating a new window while the bubble is showing will hide it,
// regardless of if it was active or not.
TEST_F(AppListBubblePresenterFocusFollowsCursorTest,
       CreatingNewWindowHidesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  EXPECT_TRUE(presenter->IsShowing());

  // Create a new widget and verify it is active and that the bubble is hidden.
  std::unique_ptr<views::Widget> widget =
      TestWidgetBuilder()
          .SetBounds(gfx::Rect(gfx::Point(1, 1), gfx::Size(100, 100)))
          .SetShow(true)
          .BuildOwnsNativeWidget();
  EXPECT_FALSE(presenter->IsShowing());
  EXPECT_TRUE(widget->IsActive());

  // Show the bubble and verify that it is active.
  presenter->Show(GetPrimaryDisplay().id());
  views::Widget* bubble_widget = presenter->bubble_widget_for_test();
  GetEventGenerator()->MoveMouseTo(
      bubble_widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_TRUE(bubble_widget->IsActive());
  EXPECT_FALSE(widget->IsActive());

  // Hover over the window. Verify that the bubble is still showing, but
  // activation has moved to the window.
  gfx::Point widget_center = widget->GetWindowBoundsInScreen().CenterPoint();
  ASSERT_FALSE(
      bubble_widget->GetWindowBoundsInScreen().Contains(widget_center));
  GetEventGenerator()->MoveMouseTo(widget_center);
  EXPECT_TRUE(presenter->IsShowing());
  EXPECT_FALSE(bubble_widget->IsActive());
  EXPECT_TRUE(widget->IsActive());

  // Create another widget, which will hide the bubble.
  std::unique_ptr<views::Widget> widget_2 =
      TestWidgetBuilder().SetShow(true).BuildOwnsNativeWidget();
  EXPECT_FALSE(presenter->IsShowing());
}

TEST_P(AppListBubbleBoundsTest, BubbleOpensInBottomLeftForBottomShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetTestDisplay().work_area().bottom_left()));
}

TEST_P(AppListBubbleBoundsTest, BubbleOpensInTopLeftForLeftShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kLeft);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().origin(),
                     GetTestDisplay().work_area().origin()));
}

TEST_P(AppListBubbleBoundsTest, BubbleOpensInTopRightForRightShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kRight);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().top_right(),
                     GetTestDisplay().work_area().top_right()));
}

TEST_P(AppListBubbleBoundsTest, BubbleOpensInBottomRightForBottomShelfRTL) {
  base::test::ScopedRestoreICUDefaultLocale locale("he");
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_right(),
                     GetTestDisplay().work_area().bottom_right()));
}

// Regression test for https://crbug.com/1263697
TEST_P(AppListBubbleBoundsTest,
       BubbleStaysInBottomLeftAfterScreenResolutionChange) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  // Changing to a large display keeps the bubble in the corner.
  SetTestDisplaySize("2100x2000");
  Widget* widget = presenter->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetTestDisplay().work_area().bottom_left()));

  // Changing to a small display keeps the bubble in the corner.
  SetTestDisplaySize("800x600");
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetTestDisplay().work_area().bottom_left()));
}

TEST_P(AppListBubbleBoundsTest, BubbleSizedForNarrowDisplay) {
  const int default_bubble_height = 688;
  SetTestDisplaySize("800x900");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  views::View* client_view = presenter->bubble_view_for_test()->parent();

  // Check that the bubble launcher has the initial "compact" bounds.
  EXPECT_EQ(544, client_view->bounds().width());
  EXPECT_EQ(default_bubble_height, client_view->bounds().height());

  // Check that the space between the top of the bubble launcher and the top of
  // the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());

  // Change the display height to be smaller than 800.
  SetTestDisplaySize("800x600");
  presenter->Dismiss();
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // With a smaller display, check that the space between the top of the
  // bubble launcher and the top of the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());
  // The bubble height should be smaller than the default bubble height.
  EXPECT_LT(client_view->bounds().height(), default_bubble_height);
  EXPECT_EQ(544, client_view->bounds().width());

  // Change the display height so that the work area is slightly smaller than
  // twice the default bubble height.
  SetTestDisplaySize("800x1470");
  presenter->Dismiss();
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should still be the default.
  EXPECT_EQ(client_view->bounds().height(), default_bubble_height);
  EXPECT_EQ(544, client_view->bounds().width());

  // Change the display height so that the work area is slightly bigger than
  // twice the default bubble height. Add apps so the bubble height grows to its
  // maximum possible height.
  SetTestDisplaySize("800x1490");
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should be slightly larger than the default bubble height,
  // but less than half the display height.
  EXPECT_GT(client_view->bounds().height(), default_bubble_height);
  EXPECT_LT(client_view->bounds().height(), 1490 / 2);
  EXPECT_EQ(544, client_view->bounds().width());
}

TEST_P(AppListBubbleBoundsTest, BubbleSizedForWideDisplay) {
  const int default_bubble_height = 688;
  SetTestDisplaySize("1400x900");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  views::View* client_view = presenter->bubble_view_for_test()->parent();

  // Check that the bubble launcher has the initial "compact" bounds.
  EXPECT_EQ(640, client_view->bounds().width());
  EXPECT_EQ(default_bubble_height, client_view->bounds().height());

  // Check that the space between the top of the bubble launcher and the top of
  // the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());

  // Change the display height to be smaller than 800.
  SetTestDisplaySize("1400x600");
  presenter->Dismiss();
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // With a smaller display, check that the space between the top of the
  // bubble launcher and the top of the screen is greater than the shelf size.
  EXPECT_GE(client_view->GetBoundsInScreen().y(),
            ShelfConfig::Get()->shelf_size());
  // The bubble height should be smaller than the default bubble height.
  EXPECT_LT(client_view->bounds().height(), default_bubble_height);
  EXPECT_EQ(640, client_view->bounds().width());

  // Change the display height so that the work area is slightly smaller than
  // twice the default bubble height.
  SetTestDisplaySize("1400x1470");
  presenter->Dismiss();
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should still be the default.
  EXPECT_EQ(client_view->bounds().height(), default_bubble_height);
  EXPECT_EQ(640, client_view->bounds().width());

  // Change the display height so that the work area is slightly bigger than
  // twice the default bubble height. Add apps so the bubble height grows to its
  // maximum possible height.
  SetTestDisplaySize("1400x1490");
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetTestDisplayId());
  client_view = presenter->bubble_view_for_test()->parent();

  // The bubble height should be slightly larger than the default bubble height,
  // but less than half the display height.
  EXPECT_GT(client_view->bounds().height(), default_bubble_height);
  EXPECT_LT(client_view->bounds().height(), 1490 / 2);
  EXPECT_EQ(640, client_view->bounds().width());
}

// Test that the AppListBubbleView scales up with more apps on a larger display.
TEST_P(AppListBubbleBoundsTest, BubbleSizedForLargeDisplay) {
  SetTestDisplaySize("2100x2000");
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  int no_apps_bubble_view_height = presenter->bubble_view_for_test()->height();

  // Add enough apps to enlarge the bubble view size from its default height.
  presenter->Dismiss();
  AddAppItems(35);
  presenter->Show(GetTestDisplayId());

  int thirty_five_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // The AppListBubbleView should be larger after apps have been added to it.
  EXPECT_GT(thirty_five_apps_bubble_view_height, no_apps_bubble_view_height);

  // Add 50 more apps to the app list and reopen.
  presenter->Dismiss();
  AddAppItems(50);
  presenter->Show(GetTestDisplayId());

  int eighty_apps_bubble_view_height =
      presenter->bubble_view_for_test()->height();

  // With more apps added, the height of the bubble should increase.
  EXPECT_GT(eighty_apps_bubble_view_height,
            thirty_five_apps_bubble_view_height);

  // The bubble height should not be larger than half the display height.
  EXPECT_LE(eighty_apps_bubble_view_height, 1000);

  // The bubble should be contained within the display bounds.
  EXPECT_TRUE(GetTestDisplay().work_area().Contains(
      presenter->bubble_view_for_test()->GetBoundsInScreen()));
}

// Tests that the AppListBubbleView is positioned correctly when
// shown with bottom auto-hidden shelf.
TEST_P(AppListBubbleBoundsTest, BubblePositionWithBottomAutoHideShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kBottom);
  GetShelfForTestDisplay()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  gfx::Point bubble_view_bottom_left = presenter->bubble_widget_for_test()
                                           ->GetWindowBoundsInScreen()
                                           .bottom_left();

  // The bottom of the AppListBubbleView should be near the top of the shelf and
  // not near the bottom side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_bottom_left, GetTestDisplay().bounds().bottom_left()));
  EXPECT_TRUE(IsNear(bubble_view_bottom_left, GetShelfBounds().origin()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with left
// auto-hidden shelf.
TEST_P(AppListBubbleBoundsTest, BubblePositionWithLeftAutoHideShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kLeft);
  GetShelfForTestDisplay()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  gfx::Point bubble_view_origin =
      presenter->bubble_widget_for_test()->GetWindowBoundsInScreen().origin();

  // The left of the AppListBubbleView should be near the right of the shelf and
  // not near the left side of the display.
  EXPECT_FALSE(IsNear(bubble_view_origin, GetTestDisplay().bounds().origin()));
  EXPECT_TRUE(IsNear(bubble_view_origin, GetShelfBounds().top_right()));
}

// Tests that the AppListBubbleView is positioned correctly when shown with
// right auto-hidden shelf.
TEST_P(AppListBubbleBoundsTest, BubblePositionWithRightAutoHideShelf) {
  GetShelfForTestDisplay()->SetAlignment(ShelfAlignment::kRight);
  GetShelfForTestDisplay()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetTestDisplayId());

  gfx::Point bubble_view_top_right = presenter->bubble_widget_for_test()
                                         ->GetWindowBoundsInScreen()
                                         .top_right();

  // The right of the AppListBubbleView should be near the left of the shelf and
  // not near the right side of the display.
  EXPECT_FALSE(
      IsNear(bubble_view_top_right, GetTestDisplay().bounds().top_right()));
  EXPECT_TRUE(IsNear(bubble_view_top_right, GetShelfBounds().origin()));
}

// Regression test for https://crbug.com/1299088
TEST_F(AppListBubblePresenterTest, ContextMenuStaysOpenAfterDismissAppList) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Spawn a context menu by right-clicking outside the bubble's bounds.
  views::Widget* bubble_widget = presenter->bubble_widget_for_test();
  gfx::Point outside_bubble =
      bubble_widget->GetWindowBoundsInScreen().top_right() +
      gfx::Vector2d(10, 0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(outside_bubble);
  generator->ClickRightButton();

  auto* rwc = RootWindowController::ForWindow(bubble_widget->GetNativeWindow());
  ASSERT_TRUE(rwc->IsContextMenuShownForTest());

  // Wait for bubble to animate closed.
  ui::LayerAnimationStoppedWaiter().Wait(
      presenter->bubble_view_for_test()->layer());
  ASSERT_FALSE(presenter->IsShowing());

  // Context menu is still open.
  EXPECT_TRUE(rwc->IsContextMenuShownForTest());
}

}  // namespace
}  // namespace ash
