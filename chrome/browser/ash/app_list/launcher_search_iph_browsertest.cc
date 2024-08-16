// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_test_api_impl.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/pagination_model_transition_waiter.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"
#include "ash/assistant/ui/main_stage/chip_view.h"
#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace {

constexpr char kScreenPlayTagName[] = "feature_id";
constexpr char kScreenPlayTagValue[] =
    "screenplay-3adcce6b-a470-48b0-9246-f6570c5cef34";

constexpr char kNotifyUsedEventUserActionName[] =
    "InProductHelp.NotifyUsedEvent.IPH_LauncherSearchHelpUi";
constexpr char kNotifyEventUserActionName[] =
    "InProductHelp.NotifyEvent.IPH_LauncherSearchHelpUi";

class ViewWaiter : public views::ViewObserver {
 public:
  ViewWaiter(views::View* observed_view, int view_id)
      : observed_view_(observed_view), view_id_(view_id) {}

  void Run() {
    if (observed_view_->GetViewByID(view_id_)) {
      return;
    }

    scoped_observation_.Observe(observed_view_);
    run_loop.Run();
  }

  void OnViewHierarchyChanged(
      views::View* observed_view,
      const views::ViewHierarchyChangedDetails& details) override {
    if (observed_view_->GetViewByID(view_id_)) {
      run_loop.Quit();
    }
  }

 private:
  raw_ptr<views::View> observed_view_;
  int view_id_;
  base::ScopedObservation<views::View, ViewWaiter> scoped_observation_{this};
  base::RunLoop run_loop;
};

class IphWaiter {
 public:
  IphWaiter() = default;
  ~IphWaiter() = default;

  void Run(feature_engagement::Tracker* tracker) {
    base::RunLoop run_loop;
    tracker->AddOnInitializedCallback(base::BindOnce(
        [](base::OnceClosure callback, bool success) {
          ASSERT_TRUE(success);
          std::move(callback).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }
};

bool IsLauncherSearchIphViewVisible() {
  ash::SearchBoxView* search_box_view = ash::GetSearchBoxView();
  if (!search_box_view) {
    return false;
  }

  views::View* launcher_search_iph_view =
      search_box_view->GetViewByID(ash::LauncherSearchIphView::ViewId::kSelf);
  if (!launcher_search_iph_view) {
    return false;
  }

  return launcher_search_iph_view->GetVisible();
}

std::string GenerateTestSuffix(const testing::TestParamInfo<bool>& info) {
  return info.param ? "tablet" : "clamshell";
}

ash::assistant::LauncherSearchIphQueryType GetQueryType(
    const std::u16string& query) {
  if (query == u"Weather") {
    return ash::assistant::LauncherSearchIphQueryType::kWeather;
  }
  if (query == u"5 ft in m") {
    return ash::assistant::LauncherSearchIphQueryType::kUnitConversion1;
  }
  if (query == u"90°F in C") {
    return ash::assistant::LauncherSearchIphQueryType::kUnitConversion2;
  }
  if (query == u"Hi in French") {
    return ash::assistant::LauncherSearchIphQueryType::kTranslation;
  }
  if (query == u"Define zenith") {
    return ash::assistant::LauncherSearchIphQueryType::kDefinition;
  }
  if (query == u"50+94/5") {
    return ash::assistant::LauncherSearchIphQueryType::kCalculation;
  }
  NOTREACHED();
}

}  // namespace

class AppListIphBrowserTest : public MixinBasedInProcessBrowserTest,
                              public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(GetParam());
    ash::Shell::Get()->assistant_controller()->SetAssistant(&test_service_);
    test_api_impl_.EnableAssistantAndWait();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());

    app_list_client_impl_ = AppListClientImpl::GetInstance();
    app_list_client_impl_->UpdateProfile();
    base::AddTagToTestResult(kScreenPlayTagName, kScreenPlayTagValue);

    tracker_ = feature_engagement::TrackerFactory::GetForBrowserContext(
        browser()->profile());

    scoped_iph_feature_list_.InitWithExistingFeatures(
        {feature_engagement::kIPHLauncherSearchHelpUiFeature});

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { DisableAssistant(); }

 protected:
  void DisableAssistant() { test_api_impl_.SetAssistantEnabled(false); }

  // Provides both `IsClamshellModeTest` and `IsTabletModeTest` to be able to
  // write `if(IsClamshellModeTest)` instead of `if(!IsTabletModeTest)` for
  // readbility.
  bool IsClamshellModeTest() const { return !GetParam(); }
  bool IsTabletModeTest() const { return GetParam(); }

  bool IsAppListVisible() const {
    return ash::AppListControllerImpl::Get()->IsVisible();
  }

  void OpenAppList() {
    ASSERT_FALSE(IsAppListVisible());

    if (IsClamshellModeTest()) {
      app_list_client_impl_->ShowAppList(ash::AppListShowSource::kSearchKey);

      // We dispatch mouse events to interact with UI. Wait animation completion
      // to reliably dispatch those events.
      ash::AppListTestApi().WaitForBubbleWindow(
          /*wait_for_opening_animation=*/true);
    } else {
      ash::AcceleratorController::Get()->PerformActionIfEnabled(
          ash::AcceleratorAction::kToggleAppList, {});

      // We dispatch mouse events to interact with UI. Wait animation completion
      // to reliably dispatch those events.
      ash::AppListTestApi().WaitForAppListShowAnimation(
          /*is_bubble_window=*/false);
    }

    ASSERT_TRUE(IsAppListVisible());
  }

  // Opens app list and activates the search box if necessary. The search box is
  // active by default in clamshell mode.
  void OpenAppListForSearch() {
    OpenAppList();

    if (IsTabletModeTest()) {
      ash::PaginationModelTransitionWaiter pagination_model_transition_waiter(
          GetFullscreenAppListContentsView()->pagination_model_for_testing());
      Click(search_box_view());
      pagination_model_transition_waiter.Wait();
    }

    ASSERT_TRUE(search_box_view());
    ASSERT_TRUE(search_box_view()->is_search_box_active());
  }

  void DismissAppList() {
    ASSERT_TRUE(IsAppListVisible());

    if (IsClamshellModeTest()) {
      app_list_client_impl()->DismissView();
    } else {
      // In tablet mode, dismiss the app list view by activating a Chrome
      // browser window for a better prod behavior simulation.
      // `AppListClientImpl::DismissView` can also dismiss the app list view.
      // But it puts a UI in a weird state.
      browser()->window()->Activate();
    }

    ASSERT_FALSE(IsAppListVisible());
  }

  void ClickAssistantButton() {
    if (IsTabletModeTest()) {
      ash::PaginationModelTransitionWaiter pagination_model_transition_waiter(
          GetFullscreenAppListContentsView()->pagination_model_for_testing());

      views::ImageButton* assistant_button =
          search_box_view()->assistant_button();
      ASSERT_TRUE(assistant_button);
      Click(assistant_button);

      pagination_model_transition_waiter.Wait();
      return;
    }

    views::ImageButton* assistant_button =
        search_box_view()->assistant_button();
    ASSERT_TRUE(assistant_button);
    Click(assistant_button);
  }

  void ClickAssistantIconAndWaitForIphView() {
    OpenAppList();

    // IPH should not show when open the Launcher.
    ASSERT_FALSE(IsLauncherSearchIphViewVisible());

    // Clicks Assistant button to open Assistant UI. IPH will show.
    ClickAssistantButton();

    // There is an async call for checking IPH trigger condition.
    ViewWaiter(search_box_view(), ash::LauncherSearchIphView::ViewId::kSelf)
        .Run();
    auto* launcher_search_iph_view = search_box_view()->GetViewByID(
        ash::LauncherSearchIphView::ViewId::kSelf);
    ash::ViewDrawnWaiter().Wait(launcher_search_iph_view);

    ASSERT_TRUE(IsLauncherSearchIphViewVisible());
  }

  ash::ContentsView* GetFullscreenAppListContentsView() {
    return ash::GetAppListView()->app_list_main_view()->contents_view();
  }

  ash::AssistantZeroStateView* GetAssistantZeroStateView() {
    if (IsClamshellModeTest()) {
      return static_cast<ash::AssistantZeroStateView*>(
          ash::GetAppListBubbleView()->GetViewByID(
              ash::AssistantViewID::kZeroStateView));
    }

    return static_cast<ash::AssistantZeroStateView*>(
        ash::GetAppListView()->GetViewByID(
            ash::AssistantViewID::kZeroStateView));
  }

  ash::LauncherSearchIphView* GetAssistantLauncherSearchIph() {
    if (IsClamshellModeTest()) {
      return static_cast<ash::LauncherSearchIphView*>(
          ash::GetAppListBubbleView()->GetViewByID(
              ash::AssistantViewID::kLauncherSearchIph));
    }

    return static_cast<ash::LauncherSearchIphView*>(
        ash::GetAppListView()->GetViewByID(
            ash::AssistantViewID::kLauncherSearchIph));
  }

  bool IsSearchPageActive() {
    if (IsClamshellModeTest()) {
      return ash::GetAppListBubbleView()->current_page_for_test() ==
             ash::AppListBubblePage::kSearch;
    } else {
      return GetFullscreenAppListContentsView()->IsShowingSearchResults();
    }
  }

  bool IsAssistantPageActive() {
    if (IsClamshellModeTest()) {
      return ash::GetAppListBubbleView()->current_page_for_test() ==
             ash::AppListBubblePage::kAssistant;
    } else {
      return GetFullscreenAppListContentsView()->IsShowingEmbeddedAssistantUI();
    }
  }

  AppListClientImpl* app_list_client_impl() { return app_list_client_impl_; }

  ash::SearchBoxView* search_box_view() { return ash::GetSearchBoxView(); }

  views::View* GetAssistantPageView() { return test_api_impl_.page_view(); }

  void Click(views::View* view) {
    ASSERT_TRUE(view);

    event_generator_->MoveMouseToInHost(
        view->GetBoundsInScreen().CenterPoint());
    event_generator_->ClickLeftButton();
  }

  void PressAndReleaseKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    event_generator_->PressAndReleaseKey(key_code, flags);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  raw_ptr<feature_engagement::Tracker> tracker_ = nullptr;

  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;

 private:
  ash::TestAssistantService test_service_;
  ash::AssistantTestApiImpl test_api_impl_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::HistogramTester histogram_tester_;

  raw_ptr<AppListClientImpl> app_list_client_impl_ = nullptr;
};

using AppListIphBrowserTestWithTestConfig = AppListIphBrowserTest;

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTest, LauncherSearchIphShownByDefault) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();

  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       NotShowIphWhenOpenForSearch) {
  IphWaiter().Run(tracker_);
  OpenAppListForSearch();

  // IPH should not show when open the Launcher.
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       NotShowIphWhenQueryChanges) {
  IphWaiter().Run(tracker_);
  OpenAppListForSearch();

  // IPH should not show when open the Launcher.
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH is not shown.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       ShowIphWhenClickAssistantButtonInSearchBox) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       DismissIphWhenQueryChanges) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH gets dismissed.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       DismissIphWhenQueryIsEmpty) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH gets dismissed.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  // Clear the search box query, the IPH is still not visible.
  ash::AppListTestApi().SimulateSearch(u"");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

using AppListIphBrowserTestWithTestConfigClamshell =
    AppListIphBrowserTestWithTestConfig;

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfigClamshell,
                       ShowAssistantPageWhenClickAssistantButtonInSearchBox) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Clicks Assistant button when the IPH is visible will open Assistant UI and
  // confirm that IPH gets dismissed.
  ClickAssistantButton();
  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfigClamshell,
                       RecordActionWhenClickAssistantButtonInSearchBox) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Confirm that a background is installed as we check that this gets removed
  // if IPH is not shown below.
  EXPECT_TRUE(search_box_view()->assistant_button()->GetBackground());

  // Confirm that assistant event is recorded. Make sure that the initial count
  // is 0 to distinguish this from other events.
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(kNotifyEventUserActionName));
  ClickAssistantButton();
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNotifyEventUserActionName));

  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfigClamshell,
                       NotShowIphAfterClickTwiceAssistantButtonInSearchBox) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  ClickAssistantButton();
  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  // Back() to dismiss Launcher.
  // Open Launcher and click Assistant button again. IPH won't be shown this
  // time. Note that this behavior is coming from the IPH config.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  OpenAppList();
  ClickAssistantButton();
  ash::ViewDrawnWaiter().Wait(GetAssistantPageView());
  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig, ClickChip) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Chip click is specified as EventUsed in the config.
  base::UserActionTester user_action_tester;
  auto* chip = static_cast<ash::ChipView*>(search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart));
  ASSERT_TRUE(chip);
  auto text = chip->GetText();
  Click(chip);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kNotifyUsedEventUserActionName));
  histogram_tester()->ExpectTotalCount(
      "Assistant.LauncherSearchIphQueryType.SearchBox", 1);
  histogram_tester()->ExpectBucketCount(
      "Assistant.LauncherSearchIphQueryType.SearchBox",
      static_cast<int>(GetQueryType(text)), 1);

  EXPECT_EQ(text, app_list_client_impl()->search_controller()->get_query());
  EXPECT_TRUE(IsSearchPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  // IPH should be kept being shown as long as the trigger condition in the IPH
  // config matches.
  DismissAppList();
  // Open Launcher again, will still show IPH.
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       ClickAssistantChipInIph) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
  if (IsClamshellModeTest()) {
    // Confirm that a background is installed as we check that this gets removed
    // if IPH is not shown below.
    EXPECT_TRUE(search_box_view()->assistant_button()->GetBackground());
  }

  // Confirm that assistant event is recorded. Make sure that the initial count
  // is 0 to distinguish this from other events.
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(kNotifyEventUserActionName));
  views::View* assistant_button = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kAssistant);
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNotifyEventUserActionName));

  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       NotShowIphAfterClickAssistantChipInIph) {
  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
  if (IsClamshellModeTest()) {
    // Confirm that a background is installed as we check that this gets removed
    // if IPH is not shown below.
    EXPECT_TRUE(search_box_view()->assistant_button()->GetBackground());
  }

  views::View* assistant_button = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kAssistant);
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  // Back() to dismiss Launcher.
  // Open Launcher and click Assistant button again. IPH won't be shown this
  // time. Note that this behavior is coming from the IPH config.
  if (IsTabletModeTest()) {
    ash::PaginationModelTransitionWaiter pagination_model_transition_waiter(
        GetFullscreenAppListContentsView()->pagination_model_for_testing());

    PressAndReleaseKey(ui::VKEY_ESCAPE);

    pagination_model_transition_waiter.Wait();
  } else {
    PressAndReleaseKey(ui::VKEY_ESCAPE);
    OpenAppList();
  }

  ClickAssistantButton();
  ash::ViewDrawnWaiter().Wait(GetAssistantPageView());
  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       RecordEntryExitPointsWhenClickAssistantChip) {
  auto duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  IphWaiter().Run(tracker_);
  ClickAssistantIconAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  views::View* assistant_button = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kAssistant);
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_TRUE(IsAssistantPageActive());
  histogram_tester()->ExpectTotalCount("Assistant.EntryPoint", 1);
  histogram_tester()->ExpectBucketCount("Assistant.EntryPoint",
                                        /*kLauncherSearchIphChip=*/13, 1);
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       NotShowIphWithoutAssistant) {
  // `AssistantTestApiImpl::SetAssistantEnabled` asserts that the value has
  // taken effect, i.e. we are sure that Assistant gets disabled after this
  // call.
  DisableAssistant();
  IphWaiter().Run(tracker_);

  OpenAppListForSearch();

  // There is an async call for IPH to be shown. This test expects that IPH does
  // NOT get shown. But run `RunUntilIdle` as this test can get failed if we
  // starts showing an IPH for this case.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
  EXPECT_FALSE(search_box_view()->assistant_button()->GetBackground());
}

class AppListIphBrowserTestAssistantZeroState
    : public AppListIphBrowserTestWithTestConfig {
 public:
  void SetUpOnMainThread() override {
    AppListIphBrowserTestWithTestConfig::SetUpOnMainThread();

    // Record the click event so that this test suite only tests the
    // AssistantPageView behaviors.
    tracker_->NotifyEvent("IPH_LauncherSearchHelpUi_assistant_click");
  }
};

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTest,
                       HasAssistantZeroStateIphByDefault) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      feature_engagement::kIPHLauncherSearchHelpUiFeature));

  tracker_->NotifyEvent("IPH_LauncherSearchHelpUi_assistant_click");
  OpenAppList();

  ClickAssistantButton();

  // The LauncherSearchIphView will show in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::LauncherSearchIphView* launcher_search_iph =
      GetAssistantLauncherSearchIph();
  EXPECT_TRUE(launcher_search_iph->GetVisible());
  EXPECT_TRUE(launcher_search_iph->IsDrawn());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestAssistantZeroState,
                       ShowAssistantZeroStateIph) {
  OpenAppList();

  ClickAssistantButton();

  // The LauncherSearchIphView is shown in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::LauncherSearchIphView* launcher_search_iph =
      GetAssistantLauncherSearchIph();
  EXPECT_TRUE(launcher_search_iph->GetVisible());
  EXPECT_TRUE(launcher_search_iph->IsDrawn());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestAssistantZeroState,
                       DismissAssistantPageAfterClickChip) {
  auto duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  OpenAppList();

  ClickAssistantButton();

  // The LauncherSearchIphView is shown in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::LauncherSearchIphView* launcher_search_iph =
      GetAssistantLauncherSearchIph();
  EXPECT_TRUE(launcher_search_iph->GetVisible());
  EXPECT_TRUE(launcher_search_iph->IsDrawn());

  // Chip click will redirect to the search page with query of the chip's text.
  auto* chip = static_cast<ash::ChipView*>(launcher_search_iph->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart));
  ASSERT_TRUE(chip);
  auto text = chip->GetText();
  Click(chip);

  histogram_tester()->ExpectTotalCount(
      "Assistant.LauncherSearchIphQueryType.AssistantPage", 1);
  histogram_tester()->ExpectBucketCount(
      "Assistant.LauncherSearchIphQueryType.AssistantPage",
      static_cast<int>(GetQueryType(text)), 1);

  EXPECT_EQ(text, app_list_client_impl()->search_controller()->get_query());
  EXPECT_TRUE(IsSearchPageActive());
  EXPECT_FALSE(IsAssistantPageActive());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestAssistantZeroState,
                       RecordExitPointsWhenClickAssistantChip) {
  auto duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  OpenAppList();

  ClickAssistantButton();
  histogram_tester()->ExpectTotalCount("Assistant.EntryPoint", 1);
  histogram_tester()->ExpectBucketCount("Assistant.EntryPoint",
                                        /*kLauncherSearchBoxIcon=*/9, 1);

  ash::LauncherSearchIphView* launcher_search_iph =
      GetAssistantLauncherSearchIph();
  EXPECT_TRUE(launcher_search_iph->GetVisible());
  EXPECT_TRUE(launcher_search_iph->IsDrawn());

  // Chip click will redirect to the search page with query of the chip's text.
  auto* chip = static_cast<ash::ChipView*>(launcher_search_iph->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart));
  ASSERT_TRUE(chip);
  Click(chip);
  histogram_tester()->ExpectTotalCount("Assistant.ExitPoint", 1);
  histogram_tester()->ExpectBucketCount("Assistant.ExitPoint",
                                        /*kLauncherSearchIphChip=*/13, 1);
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestAssistantZeroState,
                       CanOpenAssistantPageAfterClickChip) {
  auto duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  OpenAppList();

  ClickAssistantButton();

  // The LauncherSearchIphView is shown in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::LauncherSearchIphView* launcher_search_iph =
      GetAssistantLauncherSearchIph();
  EXPECT_TRUE(launcher_search_iph->GetVisible());
  EXPECT_TRUE(launcher_search_iph->IsDrawn());

  // Chip click will redirect to the search page with query of the chip's text.
  auto* chip = static_cast<ash::ChipView*>(launcher_search_iph->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart));
  ASSERT_TRUE(chip);
  auto text = chip->GetText();
  Click(chip);

  EXPECT_EQ(text, app_list_client_impl()->search_controller()->get_query());
  EXPECT_TRUE(IsSearchPageActive());
  EXPECT_FALSE(IsAssistantPageActive());

  // Press ESC key to trigger back action of Launcher.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_EQ(u"", app_list_client_impl()->search_controller()->get_query());
  EXPECT_FALSE(IsSearchPageActive());
  EXPECT_FALSE(IsAssistantPageActive());

  // Allow the `test_assistant_service` to finish StopActiveInteraction() call.
  base::RunLoop().RunUntilIdle();

  // Test after the back action, click the Assistant button will show the
  // `assistant_page`. (b/309551206)
  ClickAssistantButton();

  EXPECT_TRUE(IsAssistantPageActive());
  EXPECT_FALSE(IsSearchPageActive());
}

INSTANTIATE_TEST_SUITE_P(LauncherSearchIph,
                         AppListIphBrowserTest,
                         /*is_tablet_mode=*/testing::Bool(),
                         &GenerateTestSuffix);

INSTANTIATE_TEST_SUITE_P(LauncherSearchIph,
                         AppListIphBrowserTestWithTestConfig,
                         /*is_tablet=*/testing::Bool(),
                         &GenerateTestSuffix);

INSTANTIATE_TEST_SUITE_P(LauncherSearchIph,
                         AppListIphBrowserTestWithTestConfigClamshell,
                         /*is_tablet=*/testing::Values(false),
                         &GenerateTestSuffix);

INSTANTIATE_TEST_SUITE_P(LauncherSearchIph,
                         AppListIphBrowserTestAssistantZeroState,
                         /*is_tablet_mode=*/testing::Bool(),
                         &GenerateTestSuffix);
