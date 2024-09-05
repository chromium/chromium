// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

constexpr int kBorderInset = 1;

SearchModel* GetSearchModel() {
  return AppListModelProvider::Get()->search_model();
}

void AddSearchResult(const std::string& id, const std::u16string& title) {
  auto search_result = std::make_unique<TestSearchResult>();
  search_result->set_result_id(id);
  search_result->set_display_type(SearchResultDisplayType::kList);
  search_result->SetTitle(title);
  search_result->set_best_match(true);
  GetSearchModel()->results()->Add(std::move(search_result));
}

AppListBubblePresenter* GetBubblePresenter() {
  return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
}

views::View* GetSearchBoxSeparator() {
  return GetBubblePresenter()->bubble_view_for_test()->separator_for_test();
}

AssistantVisibility GetAssistantVisibility() {
  return AssistantUiController::Get()->GetModel()->visibility();
}

bool IsAnimatingProperty(
    ui::Layer* layer,
    ui::LayerAnimationElement::AnimatableProperty property) {
  auto* animator = layer->GetAnimator();
  return animator && animator->IsAnimatingProperty(property);
}

class AppListBubbleViewTest : public AshTestBase {
 public:
  AppListBubbleViewTest() = default;
  ~AppListBubbleViewTest() override = default;

  // Simulates the Assistant being enabled.
  void SimulateAssistantEnabled() {
    assistant_test_api_ = AssistantTestApi::Create();
    assistant_test_api_->EnableAssistantAndWait();
  }

  // Shows the app list on the primary display.
  void ShowAppList() { GetAppListTestHelper()->ShowAppList(); }

  void DismissAppList() { GetAppListTestHelper()->Dismiss(); }

  void AddContinueSuggestionResult(int num_suggestions) {
    GetAppListTestHelper()->AddContinueSuggestionResults(num_suggestions);
  }

  void AddRecentApps(int num_apps) {
    GetAppListTestHelper()->AddRecentApps(num_apps);
  }

  void AddAppItems(int num_items) {
    GetAppListTestHelper()->model()->PopulateApps(num_items);
  }

  void AddFolderWithApps(int count) {
    GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(count);
  }

  SearchBoxView* GetSearchBoxView() {
    return GetAppListTestHelper()->GetBubbleSearchBoxView();
  }

  AppListBubbleAppsPage* GetAppsPage() {
    return GetAppListTestHelper()->GetBubbleAppsPage();
  }

  ContinueSectionView* GetContinueSectionView() {
    return GetAppListTestHelper()->GetBubbleContinueSectionView();
  }

  RecentAppsView* GetRecentAppsView() {
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  AppListToastContainerView* GetToastContainerView() {
    return GetAppsPage()->toast_container_for_test();
  }

  ScrollableAppsGridView* GetAppsGridView() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  AppListBubbleSearchPage* GetSearchPage() {
    return GetAppListTestHelper()->GetBubbleSearchPage();
  }

  AppListBubbleAssistantPage* GetAssistantPage() {
    return GetAppListTestHelper()->GetBubbleAssistantPage();
  }

  views::View* GetFocusedView() {
    return GetAppListTestHelper()
        ->GetBubbleView()
        ->GetFocusManager()
        ->GetFocusedView();
  }

  bool IsNotificationBubbleShown() {
    return GetPrimaryShelf()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->IsBubbleShown();
  }

  const char* GetFocusedViewName() {
    auto* view = GetFocusedView();
    return view ? view->GetClassName() : "none";
  }

  std::unique_ptr<AssistantTestApi> assistant_test_api_;
};

TEST_F(AppListBubbleViewTest, LayerConfiguration) {
  ShowAppList();

  // Verify that nothing has changed the layer configuration.
  ui::Layer* layer = GetBubblePresenter()->bubble_view_for_test()->layer();
  ASSERT_TRUE(layer);
  EXPECT_FALSE(layer->fills_bounds_opaquely());
  EXPECT_TRUE(layer->is_fast_rounded_corner());
  EXPECT_EQ(layer->background_blur(), ColorProvider::kBackgroundBlurSigma);
}

// Tests some basic layout coordinates, because we don't have screenshot tests.
// See go/cros-launcher-spec for layout.
TEST_F(AppListBubbleViewTest, Layout) {
  ShowAppList();

  // The view has a background.
  auto* app_list_bubble_view = GetAppListTestHelper()->GetBubbleView();
  EXPECT_TRUE(app_list_bubble_view->background());

  // Check the bounds of the search box search icon.
  auto* search_box_view = GetSearchBoxView();
  auto* search_icon = search_box_view->search_icon();
  gfx::Rect search_icon_bounds =
      search_icon->ConvertRectToWidget(search_icon->GetLocalBounds());
  EXPECT_EQ("17,19 20x20", search_icon_bounds.ToString());

  // Check height of search box view.
  EXPECT_EQ(56, search_box_view->height());

  // The separator is immediately under the search box.
  gfx::Point separator_origin;
  views::View::ConvertPointToWidget(GetSearchBoxSeparator(), &separator_origin);
  EXPECT_EQ(kBorderInset, separator_origin.x());
  EXPECT_EQ(kBorderInset + search_box_view->height(), separator_origin.y());
}

TEST_F(AppListBubbleViewTest,
       ShowingBubbleUpdatesContinueSectionAndRecentApps) {
  // Show the app list with 3 tasks and 4 recent apps.
  AddContinueSuggestionResult(3);
  AddRecentApps(4);
  AddAppItems(5);
  ShowAppList();

  // Hide the app list. The widget and views are cached.
  DismissAppList();

  // While the app list is hidden, update to have 4 tasks and 5 recent apps.
  GetAppListTestHelper()->GetSearchResults()->DeleteAll();
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  ShowAppList();

  // Continue section and recent apps have the updated item counts.
  EXPECT_EQ(GetContinueSectionView()->GetTasksSuggestionsCount(), 4u);
  EXPECT_EQ(GetRecentAppsView()->GetItemViewCount(), 5);
}

TEST_F(AppListBubbleViewTest, OpeningBubbleTriggersAnimations) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Enable app list nudge to test the animation.
  GetAppListTestHelper()->DisableAppListNudge(false);

  // Show an app list with all sections.
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  // The bubble view starts animating.
  auto* bubble_view = GetAppListTestHelper()->GetBubbleView();
  ui::Layer* bubble_layer = bubble_view->layer();
  EXPECT_TRUE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::BOUNDS));
  EXPECT_TRUE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  // Layer top edge animates up.
  EXPECT_GT(bubble_layer->bounds().y(), bubble_view->y());

  // Each section view animates, starting with a translation down, therefore
  // visually moving up.
  views::View* view = GetContinueSectionView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), 20.f);

  view = GetRecentAppsView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), 40.f);

  view = GetToastContainerView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), 60.f);

  view = GetAppsGridView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), 80.f);
}

TEST_F(AppListBubbleViewTest, OpeningBubbleWithSideShelfTriggersAnimations) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Enable app list nudge to test the animation.
  GetAppListTestHelper()->DisableAppListNudge(false);

  // Enable side shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  // Show an app list with all sections.
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  // The bubble view starts animating.
  auto* bubble_view = GetAppListTestHelper()->GetBubbleView();
  ui::Layer* bubble_layer = bubble_view->layer();
  EXPECT_TRUE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::BOUNDS));
  EXPECT_TRUE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::OPACITY));
  // Layer top edge animates down.
  EXPECT_LT(bubble_layer->bounds().y(), bubble_view->y());

  // Each section view animates, starting with a translation down, therefore
  // visually moving down.
  views::View* view = GetContinueSectionView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), -20.f);

  view = GetRecentAppsView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), -40.f);

  view = GetToastContainerView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), -60.f);

  view = GetAppsGridView();
  EXPECT_TRUE(IsAnimatingProperty(
      view->layer(), ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  EXPECT_FLOAT_EQ(view->layer()->transform().To2dTranslation().y(), -80.f);
}

TEST_F(AppListBubbleViewTest, ShowAnimationCreatesAndDestroysLayers) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Enable app list nudge to test the animation.
  GetAppListTestHelper()->DisableAppListNudge(false);

  // Show an app list with all sections.
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  // The animating sections have layers created.
  auto* continue_section =
      GetAppListTestHelper()->GetBubbleContinueSectionView();
  EXPECT_TRUE(continue_section->layer());
  auto* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps->layer());
  auto* separator = GetAppsPage()->separator_for_test();
  EXPECT_TRUE(separator->layer());
  auto* toast_container = GetToastContainerView();
  EXPECT_TRUE(toast_container->layer());
  auto* apps_grid_view = GetAppsGridView();
  EXPECT_TRUE(apps_grid_view->layer());

  // Finish the animation.
  ui::LayerAnimationStoppedWaiter().Wait(apps_grid_view->layer());

  // Temporary layers are cleaned up.
  EXPECT_FALSE(continue_section->layer());
  EXPECT_FALSE(recent_apps->layer());
  EXPECT_FALSE(separator->layer());
  EXPECT_FALSE(toast_container->layer());

  // The apps grid view always has a layer, it still exists.
  EXPECT_TRUE(apps_grid_view->layer());
}

TEST_F(AppListBubbleViewTest, ShowAnimationDestroysAndRestoresShadow) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AddAppItems(20);
  ShowAppList();

  // Shadow is suppressed during show animation for performance.
  auto* app_list_bubble_view = GetAppListTestHelper()->GetBubbleView();
  EXPECT_FALSE(app_list_bubble_view->view_shadow_for_test());

  // Finish the animation.
  auto* apps_grid_view = GetAppsGridView();
  ui::LayerAnimationStoppedWaiter().Wait(apps_grid_view->layer());

  // Shadow is restored - when kJelly is enabled, no shadow is expected, for
  // consistency with bubbles in system tray area.
  EXPECT_FALSE(app_list_bubble_view->view_shadow_for_test());
}

TEST_F(AppListBubbleViewTest, ShowAnimationRecordsSmoothnessHistogram) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show an app list with just the apps grid.
  AddAppItems(5);
  ShowAppList();

  // Wait for the animation to finish.
  ui::Layer* layer = GetAppsGridView()->layer();
  ui::LayerAnimationStoppedWaiter().Wait(layer);

  // Ensure there is one more frame presented after animation finishes to allow
  // animation throughput data to be passed from cc to ui.
  layer->GetCompositor()->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(layer->GetCompositor()));

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.OpenAppsPage", 1);
  histograms.ExpectTotalCount("Apps.ClamshellLauncher.AnimationSmoothness.Open",
                              1);
}

TEST_F(AppListBubbleViewTest, HideAnimationsRecordsSmoothnessHistogram) {
  base::HistogramTester histograms;

  // Show the app list without animation.
  AddAppItems(5);
  ShowAppList();

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AppListBubbleView* view = GetBubblePresenter()->bubble_view_for_test();
  ui::Layer* layer = view->layer();

  // Run the hide animation and wait for it to finish.
  view->StartHideAnimation(/*is_side_shelf=*/false, base::DoNothing());
  ui::LayerAnimationStoppedWaiter().Wait(layer);

  // Ensure there is one more frame presented after animation finishes to allow
  // animation throughput data to be passed from cc to ui.
  layer->GetCompositor()->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(layer->GetCompositor()));

  // Smoothness was recorded.
  histograms.ExpectTotalCount(
      "Apps.ClamshellLauncher.AnimationSmoothness.Close", 1);
}

TEST_F(AppListBubbleViewTest, AssistantScreenshotClosesBubbleWithoutAnimation) {
  SimulateAssistantEnabled();
  AddAppItems(5);

  // Show the app list without animation.
  ShowAppList();

  // Switch to the assistant page.
  LeftClickOn(GetSearchBoxView()->assistant_button());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Simulate the app list being closed by taking a screenshot with assistant.
  // This makes AppListControllerImpl::ShouldDismissImmediately() return true.
  AssistantUiController::Get()->ToggleUi(
      std::nullopt, assistant::AssistantExitPoint::kScreenshot);

  // The bubble dismissed immediately so it is not animating.
  ui::Layer* bubble_layer = GetAppListTestHelper()->GetBubbleView()->layer();
  ASSERT_TRUE(bubble_layer);
  EXPECT_FALSE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::BOUNDS));
  EXPECT_FALSE(IsAnimatingProperty(
      bubble_layer, ui::LayerAnimationElement::AnimatableProperty::OPACITY));

  // App list is closed.
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

TEST_F(AppListBubbleViewTest, ShutdownDuringHideAnimationDoesNotCrash) {
  base::HistogramTester histograms;

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show the app list and wait for the show animation to finish.
  AddAppItems(5);
  ShowAppList();
  ui::LayerAnimationStoppedWaiter().Wait(GetAppsGridView()->layer());

  // Dismiss the app list, but don't wait for the animation to finish.
  GetAppListTestHelper()->Dismiss();

  // No crash.
}

TEST_F(AppListBubbleViewTest, OpeningBubbleFocusesSearchBox) {
  ShowAppList();

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, SearchBoxCloseButtonVisibleLongQuery) {
  ShowAppList();

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Enter a query, and verify that search results page is shown.
  PressAndReleaseKey(ui::VKEY_A);

  EXPECT_TRUE(GetSearchPage()->GetVisible());
  EXPECT_TRUE(
      search_box_view->filter_and_close_button_container()->GetVisible());
  for (int i = 0; i < 100; ++i) {
    PressAndReleaseKey(ui::VKEY_A);
  }
  // Close button should be visible for long queries and within search box
  // view bounds.
  EXPECT_TRUE(
      search_box_view->filter_and_close_button_container()->GetVisible());
  EXPECT_TRUE(search_box_view->GetBoundsInScreen().Contains(
      search_box_view->close_button()->GetBoundsInScreen()));
}

TEST_F(AppListBubbleViewTest, OpeningBubbleTwiceFocusesSearchBox) {
  AddAppItems(1);
  ShowAppList();

  // Click on an item, which takes focus out of the search field.
  AppListItemView* item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(item);
  SearchBoxView* search_box_view = GetSearchBoxView();
  ASSERT_FALSE(search_box_view->search_box()->HasFocus());

  // The app list view and widget are cached after this close.
  DismissAppList();

  // Search box is focused on next show.
  ShowAppList();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, ClosingBubbleClearsSearch) {
  AddAppItems(1);
  ShowAppList();

  // Enter a query, and verify that search results page is shown.
  PressAndReleaseKey(ui::VKEY_A);

  EXPECT_FALSE(GetAppsPage()->GetVisible());
  EXPECT_TRUE(GetSearchPage()->GetVisible());
  EXPECT_FALSE(GetAssistantPage()->GetVisible());

  views::Textfield* search_box_input = GetSearchBoxView()->search_box();
  EXPECT_TRUE(search_box_input->HasFocus());
  EXPECT_EQ(u"a", search_box_input->GetText());
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());

  // The app list view and widget are cached after this close.
  DismissAppList();
  EXPECT_EQ(std::vector<std::u16string>({u""}),
            client->GetAndResetPastSearchQueries());

  // Search box is empty on next show.
  ShowAppList();
  EXPECT_TRUE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_FALSE(GetAssistantPage()->GetVisible());

  search_box_input = GetSearchBoxView()->search_box();
  EXPECT_TRUE(search_box_input->HasFocus());
  EXPECT_EQ(u"", search_box_input->GetText());
}

TEST_F(AppListBubbleViewTest, SearchBoxTextUsesPrimaryTextColor) {
  ShowAppList();

  views::Textfield* search_box = GetSearchBoxView()->search_box();
  EXPECT_EQ(search_box->GetTextColor(),
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary));
}

TEST_F(AppListBubbleViewTest, SearchBoxShowsAssistantButton) {
  SimulateAssistantEnabled();
  ShowAppList();

  // By default the assistant button is visible.
  SearchBoxView* view = GetSearchBoxView();
  EXPECT_TRUE(view->edge_button_container()->GetVisible());
  EXPECT_FALSE(view->filter_and_close_button_container()->GetVisible());

  // Typing text shows the close button instead.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(view->edge_button_container()->GetVisible());
  EXPECT_TRUE(view->filter_and_close_button_container()->GetVisible());
}

TEST_F(AppListBubbleViewTest, ClickingAssistantButtonShowsAssistantPage) {
  SimulateAssistantEnabled();
  ShowAppList();
  ASSERT_EQ(AssistantVisibility::kClosed, GetAssistantVisibility());

  SearchBoxView* search_box = GetSearchBoxView();
  LeftClickOn(search_box->assistant_button());

  EXPECT_FALSE(search_box->GetVisible());
  EXPECT_FALSE(GetSearchBoxSeparator()->GetVisible());
  EXPECT_FALSE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_TRUE(GetAssistantPage()->GetVisible());

  // Assistant was notified of visibility change.
  EXPECT_EQ(AssistantVisibility::kVisible, GetAssistantVisibility());
}

TEST_F(AppListBubbleViewTest, AssistantPageLayout) {
  SimulateAssistantEnabled();
  ShowAppList();
  LeftClickOn(GetSearchBoxView()->assistant_button());

  // Assistant not have a background so the blurred launcher is visible
  // underneath the AppListBubbleAssistantPage view.
  EXPECT_FALSE(GetAssistantPage()->GetBackground());

  // Assistant fills the bubble view, so that any suggestion chips will appear
  // at the bottom.
  auto* app_list_bubble_view = GetAppListTestHelper()->GetBubbleView();
  gfx::Rect expected_bounds = app_list_bubble_view->bounds();
  expected_bounds.Inset(kBorderInset);
  EXPECT_EQ(GetAssistantPage()->bounds(), expected_bounds);
}

TEST_F(AppListBubbleViewTest, SearchBoxCloseButton) {
  ShowAppList();
  PressAndReleaseKey(ui::VKEY_A);
  TestAppListClient* const app_list_client =
      GetAppListTestHelper()->app_list_client();
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            app_list_client->GetAndResetPastSearchQueries());

  // Close button is visible after typing text.
  SearchBoxView* search_box_view = GetSearchBoxView();
  search_box_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_TRUE(
      search_box_view->filter_and_close_button_container()->GetVisible());
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  // Clicking the close button clears the search, but the search box is still
  // focused/active.
  LeftClickOn(search_box_view->close_button());
  EXPECT_EQ(std::vector<std::u16string>({u""}),
            app_list_client->GetAndResetPastSearchQueries());
  EXPECT_FALSE(
      search_box_view->filter_and_close_button_container()->GetVisible());
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, AppsPageShownByDefault) {
  ShowAppList();

  EXPECT_TRUE(GetAppsPage()->GetVisible());
  EXPECT_FALSE(GetSearchPage()->GetVisible());
  EXPECT_FALSE(GetAssistantPage()->GetVisible());
}

TEST_F(AppListBubbleViewTest, TypingTextShowsSearchPage) {
  ShowAppList();

  AppListBubbleAppsPage* apps_page = GetAppsPage();
  AppListBubbleSearchPage* search_page = GetSearchPage();

  // Type some text.
  PressAndReleaseKey(ui::VKEY_A);

  // Search page is shown.
  EXPECT_FALSE(apps_page->GetVisible());
  EXPECT_TRUE(search_page->GetVisible());

  // Backspace to remove the text.
  PressAndReleaseKey(ui::VKEY_BACK);

  // Apps page is shown.
  EXPECT_TRUE(apps_page->GetVisible());
  EXPECT_FALSE(search_page->GetVisible());
}

TEST_F(AppListBubbleViewTest, TypingTextStartsSearch) {
  ShowAppList();

  PressAndReleaseKey(ui::VKEY_A);

  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());

  PressAndReleaseKey(ui::VKEY_B);
  EXPECT_EQ(std::vector<std::u16string>({u"ab"}),
            client->GetAndResetPastSearchQueries());

  PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_EQ(std::vector<std::u16string>({u"a"}),
            client->GetAndResetPastSearchQueries());
}

TEST_F(AppListBubbleViewTest, BackActionsClearSearch) {
  ShowAppList();
  SearchBoxView* search_box_view = GetSearchBoxView();

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->is_search_box_active());

  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(search_box_view->search_box()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(search_box_view->search_box()->GetText().empty());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, BackActionsCloseAppList) {
  ShowAppList();
  GetAppListTestHelper()->CheckVisibility(true);

  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  GetAppListTestHelper()->CheckVisibility(false);

  ShowAppList();
  GetAppListTestHelper()->CheckVisibility(true);

  PressAndReleaseKey(ui::VKEY_ESCAPE);
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListBubbleViewTest, BackActionsCloseFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(GetBubblePresenter()->IsShowing());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(GetBubblePresenter()->IsShowing());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, BackActionWithSelectedItemSelectsFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);

  LeftClickOn(folder_item);
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // Focus on first item in folder
  PressAndReleaseKey(ui::VKEY_TAB);

  // Folder closed.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);

  ScrollableAppsGridView* grid_view =
      GetAppListTestHelper()->GetScrollableAppsGridView();
  EXPECT_TRUE(grid_view->has_selected_view());
  EXPECT_TRUE(grid_view->selected_view() == folder_item);
}

TEST_F(AppListBubbleViewTest, CanSelectSearchResults) {
  ShowAppList();

  // Can't select results, search page isn't visible.
  AppListBubbleView* view = GetBubblePresenter()->bubble_view_for_test();
  EXPECT_FALSE(view->CanSelectSearchResults());

  // Typing a key switches to the search page, but we still don't have results.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(view->CanSelectSearchResults());

  // Search results becoming available allows keyboard selection.
  AddSearchResult("id", u"title");
  base::RunLoop().RunUntilIdle();  // Update search model observers.
  EXPECT_TRUE(view->CanSelectSearchResults());
}

TEST_F(AppListBubbleViewTest, DownArrowMovesFocusToApps) {
  // Add an app, but no "Continue" suggestions.
  AddAppItems(1);
  ShowAppList();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();
  AppListItemView* app_item = apps_grid_view->GetItemViewAt(0);
  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Pressing down arrow moves focus into apps.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(apps_grid_view->IsSelectedView(app_item));
  EXPECT_TRUE(app_item->HasFocus());

  // Pressing up arrow moves focus back to search box.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(apps_grid_view->has_selected_view());
  EXPECT_FALSE(app_item->HasFocus());
}

// Exercises AssistantButtonFocusSkipper.
TEST_F(AppListBubbleViewTest, DownAndUpArrowSkipsAssistantButton) {
  SimulateAssistantEnabled();
  // Add an app, but no "Continue" suggestions.
  AddAppItems(1);
  ShowAppList();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();
  AppListItemView* app_item = apps_grid_view->GetItemViewAt(0);
  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Pressing down arrow moves focus into apps.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(search_box_view->assistant_button()->HasFocus());
  EXPECT_TRUE(app_item->HasFocus());

  // Pressing up arrow moves focus back to search box.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(search_box_view->assistant_button()->HasFocus());
  EXPECT_FALSE(app_item->HasFocus());

  // Tab key moves focus to assistant button.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->assistant_button()->HasFocus());

  // Shift-tab moves focus back to search box.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_FALSE(search_box_view->assistant_button()->HasFocus());
}

TEST_F(AppListBubbleViewTest, DownArrowSelectsRecentsThenApps) {
  // Create enough apps to require scrolling.
  AddAppItems(50);
  // Create enough recent apps that the recents section will show.
  const int kNumRecentApps = 5;
  AddRecentApps(kNumRecentApps);
  ShowAppList();

  // Pressing down arrow once moves focus into recent apps.
  auto* focus_manager = GetAppsPage()->GetFocusManager();
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(GetRecentAppsView()->Contains(focus_manager->GetFocusedView()));

  // Pressing down arrow again moves focus into the apps grid.
  PressAndReleaseKey(ui::VKEY_DOWN);
  auto* apps_grid = GetAppListTestHelper()->GetScrollableAppsGridView();
  EXPECT_TRUE(apps_grid->Contains(focus_manager->GetFocusedView()));
}

TEST_F(AppListBubbleViewTest, DownArrowFromRecentsSelectsSameColumnInAppsGrid) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  for (int column = 0; column < 5; column++) {
    // Pressing down arrow from an item in recent apps selects the app in the
    // same column in the apps grid.
    AppListItemView* recent_app = GetRecentAppsView()->GetItemViewAt(column);
    recent_app->RequestFocus();
    ASSERT_TRUE(recent_app->HasFocus());

    PressAndReleaseKey(ui::VKEY_DOWN);

    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    EXPECT_TRUE(app->HasFocus()) << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, DownArrowFromToastKeepsSameColumnInAppsGrid) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor,
      /*animate=*/false, /*update_position_closure=*/base::OnceClosure());
  ASSERT_TRUE(GetToastContainerView()->GetToastButton());

  for (int column = 0; column < 5; column++) {
    // Pressing down arrow from an item in recent apps selects the app in the
    // same column in the apps grid.
    AppListItemView* recent_app = GetRecentAppsView()->GetItemViewAt(column);
    recent_app->RequestFocus();
    ASSERT_TRUE(recent_app->HasFocus());

    PressAndReleaseKey(ui::VKEY_DOWN);
    EXPECT_TRUE(GetToastContainerView()->GetToastButton()->HasFocus());

    PressAndReleaseKey(ui::VKEY_DOWN);
    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    EXPECT_TRUE(app->HasFocus()) << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, DownArrowFromRecentsSelectsLastColumnInAppsGrid) {
  AddRecentApps(5);
  AddFolderWithApps(2);
  AddFolderWithApps(3);
  ShowAppList();

  // There are only 2 folders, and hence 2 columns, in the top level apps grid.
  auto* apps_grid_view = GetAppsGridView();
  ASSERT_EQ(2u, apps_grid_view->view_model()->view_size());

  // Focus the 5th recent app.
  auto* recent_apps_view = GetRecentAppsView();
  ASSERT_EQ(5, recent_apps_view->GetItemViewCount());
  recent_apps_view->GetItemViewAt(4)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_DOWN);

  // There's no 5th column in the apps grid, so the 2nd item is selected.
  AppListItemView* item = GetAppsGridView()->GetItemViewAt(1);
  EXPECT_TRUE(item->HasFocus());
}

TEST_F(AppListBubbleViewTest, UpArrowFromRecentsSelectsContinueTasks) {
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  ContinueTaskView* last_continue_task =
      GetContinueSectionView()->GetTaskViewAtForTesting(3);
  auto* recent_apps_view = GetRecentAppsView();

  // Pressing 'up' from any column in recent apps moves to the last continue
  // task.
  for (int column = 0; column < 5; ++column) {
    recent_apps_view->GetItemViewAt(column)->RequestFocus();

    PressAndReleaseKey(ui::VKEY_UP);

    EXPECT_TRUE(views::IsViewClass<ContinueTaskView>(GetFocusedView()))
        << GetFocusedViewName();
    EXPECT_TRUE(last_continue_task->HasFocus());
  }
}

TEST_F(AppListBubbleViewTest, UpArrowFromAppsGridSelectsSameColumnInRecents) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  for (int column = 0; column < 5; column++) {
    // Pressing up arrow from an item in the apps grid selects the app in the
    // same column in the recents list.
    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    app->RequestFocus();
    ASSERT_TRUE(app->HasFocus());

    PressAndReleaseKey(ui::VKEY_UP);

    EXPECT_TRUE(GetRecentAppsView()->GetItemViewAt(column)->HasFocus())
        << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, UpArrowFromToastKeepsSameColumnInAppsGrid) {
  AddRecentApps(5);
  AddAppItems(5);
  ShowAppList();

  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor,
      /*animate=*/false, /*update_position_closure=*/base::OnceClosure());
  ASSERT_TRUE(GetToastContainerView()->GetToastButton());

  for (int column = 0; column < 5; column++) {
    // Pressing up arrow from an item in the apps grid selects the app in the
    // same column in the recents list.
    AppListItemView* app = GetAppsGridView()->GetItemViewAt(column);
    app->RequestFocus();
    ASSERT_TRUE(app->HasFocus());

    PressAndReleaseKey(ui::VKEY_UP);
    EXPECT_TRUE(GetToastContainerView()->GetToastButton()->HasFocus());

    PressAndReleaseKey(ui::VKEY_UP);
    EXPECT_TRUE(GetRecentAppsView()->GetItemViewAt(column)->HasFocus())
        << "Focus mismatch for column " << column;
  }
}

TEST_F(AppListBubbleViewTest, UpArrowFromAppsGridSelectsLastColumnInRecents) {
  // Add 4 columns of recents, but 5 columns of apps.
  AddRecentApps(4);
  AddAppItems(5);
  ShowAppList();

  // Select the app in the last column of the apps grid.
  GetAppsGridView()->GetItemViewAt(4)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_UP);

  // The last app in recents is selected.
  EXPECT_TRUE(GetRecentAppsView()->GetItemViewAt(3)->HasFocus());
}

TEST_F(AppListBubbleViewTest,
       UpArrowFromAppsGridWithNoRecentsSelectsContinueTasks) {
  AddContinueSuggestionResult(4);
  // Don't add recents.
  AddAppItems(5);
  ShowAppList();
  GetAppsGridView()->GetItemViewAt(0)->RequestFocus();

  PressAndReleaseKey(ui::VKEY_UP);

  auto* focus_manager = GetAppsPage()->GetFocusManager();
  EXPECT_TRUE(
      GetContinueSectionView()->Contains(focus_manager->GetFocusedView()))
      << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, DownArrowMovesFocusToContinueTasks) {
  // Add an app, and some "Continue" suggestions.
  AddAppItems(1);
  // Create enough recent apps that the recents section will show.
  AddContinueSuggestionResult(4);
  ShowAppList();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  // Pressing down arrow twice moves focus through the two rows of continue
  // tasks. It does not trigger ScrollView scrolling.
  auto* focus_manager = GetAppsPage()->GetFocusManager();
  auto* continue_section = GetContinueSectionView();
  for (int i = 0; i < 2; i++) {
    PressAndReleaseKey(ui::VKEY_DOWN);
    EXPECT_TRUE(continue_section->Contains(focus_manager->GetFocusedView()));
  }

  // Pressing down arrow again moves focus into the apps grid.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(apps_grid_view->Contains(focus_manager->GetFocusedView()));
}

TEST_F(AppListBubbleViewTest, ClickOnFolderOpensFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Folder opened.
  EXPECT_TRUE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, FolderClosedOnAppListDismiss) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // The bubble view and widget are cached after dismiss.
  DismissAppList();

  // The folder is closed when the app list is reopened.
  ShowAppList();
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, FolderClosedAfterInvokingAssistant) {
  SimulateAssistantEnabled();
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  ASSERT_TRUE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  PressAndReleaseKey(ui::VKEY_ASSISTANT);
  EXPECT_TRUE(GetAssistantPage()->GetVisible());
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, LargeFolderViewFitsInsideMainBubble) {
  // Create more apps than fit in the default sized folder.
  AddFolderWithApps(30);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // The folder fits inside the bubble.
  gfx::Rect folder_bounds =
      GetAppListTestHelper()->GetBubbleFolderView()->GetBoundsInScreen();
  gfx::Rect bubble_bounds =
      GetBubblePresenter()->bubble_view_for_test()->GetBoundsInScreen();
  EXPECT_TRUE(bubble_bounds.Contains(folder_bounds));

  // The top and bottom of the folder are inset from the bubble top and bottom.
  constexpr int kExpectedInset = 16;
  EXPECT_EQ(folder_bounds.y(), bubble_bounds.y() + kExpectedInset);
  EXPECT_EQ(folder_bounds.bottom(), bubble_bounds.bottom() - kExpectedInset);
}

TEST_F(AppListBubbleViewTest, ClickOutsideFolderClosesFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  gfx::Point outside_view =
      folder_view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
  GetEventGenerator()->MoveMouseTo(outside_view);
  GetEventGenerator()->ClickLeftButton();

  // Folder closed.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, ReparentDragOutOfFolderClosesFolder) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Drag the first app from the folder's app grid.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppListItemView* app_item = folder_view->items_grid_view()->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(app_item->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  app_item->FireMouseDragTimerForTest();

  // Drag item out of folder view.
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point outside_view =
        folder_view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
    generator->MoveMouseTo(outside_view);
    generator->MoveMouseBy(10, 10);
    // Folder visually closed.
    EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
    EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {  // End the drag.
    generator->ReleaseLeftButton();
    EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // End the drag.
  generator->ReleaseLeftButton();
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());
}

TEST_F(AppListBubbleViewTest, DragItemInsideFolderDoesNotSelectItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item =
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Drag the first app inside the folder's app grid.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppListItemView* first_app = folder_view->items_grid_view()->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(first_app->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  first_app->FireMouseDragTimerForTest();

  // Quickly drag and release the app.
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseBy(100, 100);
    generator->ReleaseLeftButton();
    // Nothing is selected or focused.
    EXPECT_FALSE(folder_view->items_grid_view()->has_selected_view());
    EXPECT_FALSE(GetFocusedView()) << GetFocusedViewName();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);
}

TEST_F(AppListBubbleViewTest, OpenFolderWithMouseDoesNotFocusItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  AppsGridView* items_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_FALSE(items_grid_view->has_selected_view());
  EXPECT_FALSE(GetFocusedView()) << GetFocusedViewName();
}

// Verifies that keyboard focus stays inside an open folder. If this test breaks
// then one of the DisableFocusForShowingActiveFolder() methods needs to be
// updated to include the incorrectly focused view.
TEST_F(AppListBubbleViewTest, PressingTabMovesFocusInsideFolder) {
  // Ensure all sections are showing, so the test verifies that none of these
  // sections (or the hide continue section button) take focus.
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddFolderWithApps(3);
  ShowAppList();

  // Force the sorting toast to show.
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor,
      /*animate=*/false, /*update_position_closure=*/base::OnceClosure());
  ASSERT_TRUE(GetToastContainerView()->GetToastButton());

  // Open the folder.
  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  PressAndReleaseKey(ui::VKEY_TAB);

  // First item is selected.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  AppsGridView* items_grid_view = folder_view->items_grid_view();
  EXPECT_TRUE(items_grid_view->has_selected_view());
  EXPECT_EQ(items_grid_view->GetItemViewAt(0), GetFocusedView())
      << GetFocusedViewName();

  // Repeatedly pressing tab keeps focus inside the folder view.
  for (int i = 0; i < 10; i++) {
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_TRUE(folder_view->Contains(GetFocusedView()))
        << GetFocusedViewName();
  }
}

// Verifies that focus does not move from a folder to the privacy notice. This
// is a separate test from PressingTabMovesFocusInsideFolder because that test
// verifies the sorting nudge, and the launcher only shows one nudge at a time.
TEST_F(AppListBubbleViewTest, PressingTabInFolderDoesNotFocusPrivacyNotice) {
  // Force the continue section privacy toast to show.
  AppListNudgeController::SetPrivacyNoticeAcceptedForTest(false);
  AddContinueSuggestionResult(4);
  AddFolderWithApps(3);
  ShowAppList();

  // Privacy notice is visible.
  auto* privacy_notice = GetContinueSectionView()->GetPrivacyNoticeForTest();
  ASSERT_TRUE(privacy_notice);
  ASSERT_TRUE(privacy_notice->GetVisible());

  // Open the folder.
  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Repeatedly pressing tab keeps focus inside the folder view. In particular,
  // it does not focus the privacy toast.
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  for (int i = 0; i < 10; i++) {
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_TRUE(folder_view->Contains(GetFocusedView()))
        << GetFocusedViewName();
  }
}

TEST_F(AppListBubbleViewTest, OpeningFolderRemovesOtherViewsFromAccessibility) {
  AddContinueSuggestionResult(4);
  AddRecentApps(5);
  AddFolderWithApps(5);
  ShowAppList();

  // Force the sorting toast to show.
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor,
      /*animate=*/false, /*update_position_closure=*/base::OnceClosure());
  ASSERT_TRUE(GetToastContainerView()->GetToastButton());

  // Open the folder.
  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* search_box = GetSearchBoxView();
  EXPECT_TRUE(search_box->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(search_box->GetViewAccessibility().IsLeaf());
  auto* continue_section = GetContinueSectionView();
  EXPECT_TRUE(continue_section->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(continue_section->GetViewAccessibility().IsLeaf());
  auto* recent_apps = GetRecentAppsView();
  EXPECT_TRUE(recent_apps->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(recent_apps->GetViewAccessibility().IsLeaf());
  auto* toast_container = GetToastContainerView();
  EXPECT_TRUE(toast_container->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(toast_container->GetViewAccessibility().IsLeaf());
  auto* apps_grid = GetAppsGridView();
  EXPECT_TRUE(apps_grid->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(apps_grid->GetViewAccessibility().IsLeaf());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(search_box->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(search_box->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(continue_section->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(continue_section->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(toast_container->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(toast_container->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(apps_grid->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(apps_grid->GetViewAccessibility().IsLeaf());
}

TEST_F(AppListBubbleViewTest, OpenFolderWithKeyboardFocusesFirstItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  folder_item->RequestFocus();
  PressAndReleaseKey(ui::VKEY_RETURN);

  // First item is selected and focused.
  AppsGridView* items_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  AppListItemView* first_item = items_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(items_grid_view->has_selected_view());
  EXPECT_TRUE(items_grid_view->IsSelectedView(first_item));
  EXPECT_TRUE(first_item->HasFocus()) << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, CloseFolderWithNoSelectedItemFocusesSearchBox) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  ASSERT_FALSE(folder_view->items_grid_view()->has_selected_view());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus())
      << GetFocusedViewName();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(AppListBubbleViewTest, CloseFolderWithSelectedItemFocusesFolderItem) {
  AddFolderWithApps(3);
  ShowAppList();

  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  folder_view->items_grid_view()->GetItemViewAt(0)->RequestFocus();
  ASSERT_TRUE(folder_view->items_grid_view()->has_selected_view());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  // Folder item is selected and focused.
  auto* root_apps_grid_view = GetAppsGridView();
  EXPECT_TRUE(root_apps_grid_view->has_selected_view());
  EXPECT_TRUE(root_apps_grid_view->IsSelectedView(folder_item));
  EXPECT_TRUE(folder_item->HasFocus()) << GetFocusedViewName();
}

TEST_F(AppListBubbleViewTest, ScrollInFolderHeaderScrollsFolder) {
  // Add a folder with enough apps that its grid will be scrollable.
  AddFolderWithApps(30);
  ShowAppList();

  // Open the folder and get the initial scroll position.
  AppListItemView* folder_item = GetAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  auto* scroll_view = folder_view->scroll_view_for_test();
  const int initial_scroll_offset = scroll_view->GetVisibleRect().y();

  // Simulate a mouse wheel scroll up event in the folder header.
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(
      folder_view->folder_header_view()->GetBoundsInScreen().CenterPoint());
  generator->MoveMouseWheel(0, -10);

  // The view scrolled.
  const int final_scroll_offset = scroll_view->GetVisibleRect().y();
  EXPECT_GT(final_scroll_offset, initial_scroll_offset);
}

gfx::Rect GetStartFadeRect(const gfx::LinearGradient& gradient_mask,
                           gfx::Rect layer_bounds) {
  // Vertical gradient from top to bottom.
  EXPECT_EQ(gradient_mask.angle(), -90);

  // No top gradient
  if (!cc::MathUtil::IsWithinEpsilon(gradient_mask.steps()[0].fraction, 0.f))
    return gfx::Rect();

  float fade_height = gradient_mask.steps()[1].fraction * layer_bounds.height();
  return gfx::Rect(layer_bounds.origin(),
                   gfx::Size(layer_bounds.width(), fade_height));
}

gfx::Rect GetEndFadeRect(const gfx::LinearGradient& gradient_mask,
                         gfx::Rect layer_bounds) {
  // Vertical gradient from top to bottom.
  EXPECT_EQ(gradient_mask.angle(), -90);

  // No bottom gradient
  if (!cc::MathUtil::IsWithinEpsilon(
          gradient_mask.steps()[gradient_mask.step_count() - 1].fraction,
          1.f)) {
    return gfx::Rect();
  }

  float fade_height =
      (1.f - gradient_mask.steps()[gradient_mask.step_count() - 2].fraction) *
      layer_bounds.height();
  float fade_y = layer_bounds.bottom() - fade_height;
  return gfx::Rect(layer_bounds.x(), fade_y, layer_bounds.width(), fade_height);
}

TEST_F(AppListBubbleViewTest, AutoScrollToFitViewOnFocus) {
  // Show an app list with enough apps to fill the page and trigger a gradient
  // at the bottom.
  AddAppItems(50);
  ShowAppList();

  // Scroll view gradient mask layer is created.
  auto* scroll_view = GetAppsPage()->scroll_view();
  EXPECT_FALSE(scroll_view->layer()->gradient_mask().IsEmpty());
  const int rows = base::ClampFloor(50.0 / GetAppsGridView()->cols());

  // Focus the first item on the last row.
  for (int i = 0; i < rows; i++)
    PressAndReleaseKey(ui::VKEY_DOWN);

  gfx::Rect app_view_bounds = GetAppsGridView()
                                  ->GetFocusManager()
                                  ->GetFocusedView()
                                  ->GetBoundsInScreen();
  const gfx::LinearGradient& gradient_mask =
      GetAppsPage()->gradient_helper_for_test()->gradient_mask_for_test();
  gfx::Rect gradient_mask_bounds_start =
      GetStartFadeRect(gradient_mask, scroll_view->GetVisibleRect());
  gfx::Rect gradient_mask_bounds_end =
      GetEndFadeRect(gradient_mask, scroll_view->GetVisibleRect());
  views::View::ConvertRectToScreen(scroll_view, &gradient_mask_bounds_start);
  views::View::ConvertRectToScreen(scroll_view, &gradient_mask_bounds_end);

  // The gradient mask should not obscure the focused app view.
  EXPECT_FALSE(gradient_mask_bounds_start.Intersects(app_view_bounds));
  EXPECT_FALSE(gradient_mask_bounds_end.Intersects(app_view_bounds));

  // Press down arrow two more times to move focus to the first row again.
  PressAndReleaseKey(ui::VKEY_DOWN);
  PressAndReleaseKey(ui::VKEY_DOWN);

  ASSERT_TRUE(GetAppsGridView()->GetItemViewAt(0)->HasFocus());

  app_view_bounds = GetAppsGridView()
                        ->GetFocusManager()
                        ->GetFocusedView()
                        ->GetBoundsInScreen();
  gradient_mask_bounds_start =
      GetStartFadeRect(gradient_mask, scroll_view->GetVisibleRect());
  gradient_mask_bounds_end =
      GetEndFadeRect(gradient_mask, scroll_view->GetVisibleRect());
  views::View::ConvertRectToScreen(scroll_view, &gradient_mask_bounds_start);
  views::View::ConvertRectToScreen(scroll_view, &gradient_mask_bounds_end);

  // The gradient mask should not obscure the focused app view.
  EXPECT_FALSE(gradient_mask_bounds_start.Intersects(app_view_bounds));
  EXPECT_FALSE(gradient_mask_bounds_end.Intersects(app_view_bounds));
}

TEST_F(AppListBubbleViewTest, AutoScrollOnTopOfTheBubble) {
  // Show an app list with enough apps to fill the page and trigger a gradient
  // at the bottom.
  const int kTotalAppItems = 50;
  AddAppItems(kTotalAppItems);
  ShowAppList();
  const int rows =
      base::ClampFloor(1.0f * kTotalAppItems / GetAppsGridView()->cols());

  // Focus the first item on the last row.
  for (int i = 0; i < rows; i++) {
    PressAndReleaseKey(ui::VKEY_DOWN);
  }

  // Drag the last app from the app grid.
  AppListItemView* app_item =
      GetAppsGridView()->GetItemViewAt(kTotalAppItems - 1);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(app_item->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  app_item->FireMouseDragTimerForTest();

  gfx::Point top_of_the_bubble = GetBubblePresenter()
                                     ->bubble_view_for_test()
                                     ->GetBoundsInScreen()
                                     .top_center();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag app  outside of the bubble. The scroll timer should not be running.
    gfx::Point bubble_view_outside(top_of_the_bubble);
    bubble_view_outside.Offset(0, -20);
    generator->MoveMouseTo(bubble_view_outside);
    ASSERT_FALSE(GetAppsGridView()->auto_scroll_timer_for_test()->IsRunning());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Enter the apps grid bubble should start scrolling up.
    generator->MoveMouseTo(top_of_the_bubble);
    EXPECT_TRUE(GetAppsGridView()->auto_scroll_timer_for_test()->IsRunning());
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);
}

// Verifies that hidden app list bubble view does not attempt to change its
// active page when app list model gets cleared.
TEST_F(AppListBubbleViewTest, HiddenAppListPageNotSetDuringShutdown) {
  // Show app list so AppListBubbleView gets created.
  AddAppItems(5);
  ShowAppList();

  DismissAppList();
  EXPECT_EQ(AppListBubblePage::kNone,
            GetAppListTestHelper()->GetBubbleView()->current_page_for_test());

  AppListModelProvider::Get()->ClearActiveModel();
  EXPECT_EQ(AppListBubblePage::kNone,
            GetAppListTestHelper()->GetBubbleView()->current_page_for_test());
}

// Regression test for https://crbug.com/1313140
TEST_F(AppListBubbleViewTest, CanOpenMessageCenterWithKeyboardShortcut) {
  // Add a notification so there's something to focus in the message center.
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id", u"Title", u"Message",
      ui::ImageModel(), /*display_source=*/std::u16string(), GURL(),
      message_center::NotifierId(), message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  // Message center starts closed.
  ASSERT_FALSE(IsNotificationBubbleShown());

  // Open the launcher and do a search.
  AddAppItems(1);
  ShowAppList();
  PressAndReleaseKey(ui::VKEY_A);

  // Search box has focus.
  views::Textfield* search_box_input = GetSearchBoxView()->search_box();
  ASSERT_TRUE(search_box_input->HasFocus());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Move focus to the message center notification area with Alt-Shift-N. The
  // message center will open and the app list will dismiss.
  PressAndReleaseKey(ui::VKEY_N, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  // Wait for the app list hide animation to finish.
  AppListBubbleView* view = GetBubblePresenter()->bubble_view_for_test();
  ui::LayerAnimationStoppedWaiter().Wait(view->layer());

  // Search box did not steal focus.
  EXPECT_FALSE(search_box_input->HasFocus());

  // Message center is still open.
  EXPECT_TRUE(IsNotificationBubbleShown());
}

}  // namespace
}  // namespace ash
