// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_test_api_impl.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/launcher_search_iph_view.h"
#include "ash/app_list/views/pagination_model_transition_waiter.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
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

constexpr char kIphConfigParamNameEventUsed[] = "event_used";
constexpr char kIphConfigParamNameEventTrigger[] = "event_trigger";
constexpr char kIphConfigParamNameEvent1[] = "event_1";
constexpr char kIphConfigParamNameAvailability[] = "availability";
constexpr char kIphConfigParamNameSessionRate[] = "session_rate";

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

void Click(views::View* view) {
  ASSERT_TRUE(view);

  ui::test::EventGenerator event_generator(
      view->GetWidget()->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseToInHost(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

std::string GenerateTestSuffix(const testing::TestParamInfo<bool>& info) {
  return info.param ? "tablet" : "clamshell";
}

}  // namespace

class AppListIphBrowserTest : public MixinBasedInProcessBrowserTest,
                              public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(GetParam());
    ash::Shell::Get()->assistant_controller()->SetAssistant(&test_service_);
    test_api_impl_.EnableAssistantAndWait();

    app_list_client_impl_ = AppListClientImpl::GetInstance();
    app_list_client_impl_->UpdateProfile();

    base::AddTagToTestResult(kScreenPlayTagName, kScreenPlayTagValue);

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

  void OpenAppListAndWaitForIphView() {
    OpenAppListForSearch();

    // There is an async call for checking IPH trigger condition.
    ViewWaiter(search_box_view(), ash::LauncherSearchIphView::ViewId::kSelf)
        .Run();
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

  ash::AppListToastView* GetAssistantLearnMoreToast() {
    if (IsClamshellModeTest()) {
      return static_cast<ash::AppListToastView*>(
          ash::GetAppListBubbleView()->GetViewByID(
              ash::AssistantViewID::kLearnMoreToast));
    }

    return static_cast<ash::AppListToastView*>(
        ash::GetAppListView()->GetViewByID(
            ash::AssistantViewID::kLearnMoreToast));
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

 private:
  ash::TestAssistantService test_service_;
  ash::AssistantTestApiImpl test_api_impl_;

  raw_ptr<AppListClientImpl> app_list_client_impl_ = nullptr;
};

class AppListIphBrowserTestWithTestConfig : public AppListIphBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::FieldTrialParams params;
    params[kIphConfigParamNameAvailability] = "any";
    params[kIphConfigParamNameSessionRate] = "any";
    params[kIphConfigParamNameEventUsed] =
        base::StringPrintf("name:%s;comparator:any;window:365;storage:365",
                           ash::LauncherSearchIphView::kIphEventNameChipClick);
    // Trigger event is not used for this test config. Note that a trigger event
    // gets incremented every time an IPH is shown.
    params[kIphConfigParamNameEventTrigger] =
        "name:IPH_LauncherSearchHelpUi_trigger;comparator:any;window:365;"
        "storage:365";
    params[kIphConfigParamNameEvent1] = base::StringPrintf(
        "name:%s;comparator:==0;window:365;storage:365",
        ash::LauncherSearchIphView::kIphEventNameAssistantClick);

    scoped_iph_feature_list_ =
        std::make_unique<feature_engagement::test::ScopedIphFeatureList>();
    scoped_iph_feature_list_->InitAndEnableFeaturesWithParameters(
        {base::test::FeatureRefAndParams(
            feature_engagement::kIPHLauncherSearchHelpUiFeature, params)});

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  std::unique_ptr<feature_engagement::test::ScopedIphFeatureList>
      scoped_iph_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTest,
                       LauncherSearchIphNotShownByDefault) {
  OpenAppListForSearch();
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig, LauncherSearchIph) {
  OpenAppListAndWaitForIphView();

  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
  if (IsClamshellModeTest()) {
    EXPECT_TRUE(search_box_view()->assistant_button()->GetBackground());
  }

  // IPH should be kept being shown as long as the trigger condition in the test
  // config matches.
  DismissAppList();
  OpenAppListAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       LauncherSearchIphSearch) {
  OpenAppListAndWaitForIphView();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH gets dismissed.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

using AppListIphBrowserTestWithTestConfigClamshell =
    AppListIphBrowserTestWithTestConfig;

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfigClamshell,
                       LauncherSearchIphAssistantButtonInSearchBox) {
  OpenAppListAndWaitForIphView();

  // Clicks Assistant button to open Assistant UI and confirm that IPH gets
  // dismissed.
  views::ImageButton* assistant_button = search_box_view()->assistant_button();
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig, ClickChip) {
  OpenAppListAndWaitForIphView();

  // Chip click is specified as EventUsed in the test config.
  base::UserActionTester user_action_tester;
  views::View* chip = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart);
  ASSERT_TRUE(chip);
  Click(chip);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kNotifyUsedEventUserActionName));

  EXPECT_EQ(u"Weather",
            app_list_client_impl()->search_controller()->get_query());
  EXPECT_TRUE(IsSearchPageActive());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig, ClickAssistant) {
  OpenAppListAndWaitForIphView();

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

  // Dismiss the app list and show it again. IPH won't be shown this time. Note
  // that this behavior is coming from the IPH test config.
  DismissAppList();
  OpenAppListForSearch();
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());

  if (IsClamshellModeTest()) {
    // Launcher search iph installs a background to an assistant button in the
    // search box. It should be removed if the iph gets dismissed.
    EXPECT_FALSE(search_box_view()->assistant_button()->GetBackground());
  }
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig,
                       NoIphWithoutAssistant) {
  // `AssistantTestApiImpl::SetAssistantEnabled` asserts that the value has
  // taken effect, i.e. we are sure that Assistant gets disabled after this
  // call.
  DisableAssistant();

  OpenAppListForSearch();

  // There is an async call for IPH to be shown. This test expects that IPH does
  // NOT get shown. But run `RunUntilIdle` as this test can get failed if we
  // starts showing an IPH for this case.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
  EXPECT_FALSE(search_box_view()->assistant_button()->GetBackground());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithTestConfig, ClickLink) {
  OpenAppListAndWaitForIphView();
  views::View* link_label = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kDescriptionLinkLabel);

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  Click(link_label);
  tab_added_waiter.Wait();
  EXPECT_EQ(GURL("https://www.google.com/"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// The bool param indicates if the AssistantLearnMore feature is enabled or not.
class AppListIphBrowserTestWithLearnMoreToast : public AppListIphBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::assistant::features::kEnableAssistantLearnMore);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTest,
                       NoAssistantLearnMoreToastFlagOff) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      ash::assistant::features::kEnableAssistantLearnMore));

  OpenAppList();

  views::ImageButton* assistant_button = search_box_view()->assistant_button();
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  // The learn more toast is shown in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::AppListToastView* learn_more_toast = GetAssistantLearnMoreToast();
  EXPECT_FALSE(learn_more_toast->GetVisible());
  EXPECT_FALSE(learn_more_toast->IsDrawn());
}

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithLearnMoreToast,
                       ShowAssistantLearnMoreToast) {
  OpenAppList();

  views::ImageButton* assistant_button = search_box_view()->assistant_button();
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  // The learn more toast is shown in the zero state view.
  ASSERT_TRUE(IsAssistantPageActive());
  ASSERT_TRUE(GetAssistantZeroStateView()->GetVisible());

  ash::AppListToastView* learn_more_toast = GetAssistantLearnMoreToast();
  EXPECT_TRUE(learn_more_toast->GetVisible());
  EXPECT_TRUE(learn_more_toast->IsDrawn());
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
                         AppListIphBrowserTestWithLearnMoreToast,
                         /*is_tablet_mode=*/testing::Bool(),
                         &GenerateTestSuffix);
