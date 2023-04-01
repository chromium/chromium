// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/assistant/assistant_test_api_impl.h"
#include "ash/app_list/views/launcher_search_iph_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace {
class ViewWaiter : public views::ViewObserver {
 public:
  ViewWaiter(raw_ptr<views::View> observed_view, int view_id)
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
  raw_ptr<ash::SearchBoxView> search_box_view = ash::GetSearchBoxView();
  if (!search_box_view) {
    return false;
  }

  raw_ptr<views::View> launcher_search_iph_view =
      search_box_view->GetViewByID(ash::LauncherSearchIphView::ViewId::kSelf);
  if (!launcher_search_iph_view) {
    return false;
  }

  return launcher_search_iph_view->GetVisible();
}

void Click(raw_ptr<views::View> view) {
  ASSERT_TRUE(view);

  ui::test::EventGenerator event_generator(
      view->GetWidget()->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseToInHost(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

}  // namespace

class AppListIphBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ash::Shell::Get()->assistant_controller()->SetAssistant(&test_service_);
    test_api_impl_.EnableAssistantAndWait();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { DisableAssistant(); }

  void DisableAssistant() { test_api_impl_.SetAssistantEnabled(false); }

 private:
  ash::TestAssistantService test_service_;
  ash::AssistantTestApiImpl test_api_impl_;
};

class AppListIphBrowserTestWithDemoMode : public AppListIphBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_iph_feature_list_ =
        std::make_unique<feature_engagement::test::ScopedIphFeatureList>();
    scoped_iph_feature_list_->InitForDemo(
        feature_engagement::kIPHLauncherSearchHelpUiFeature);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  void OpenAppList() {
    ASSERT_TRUE(!app_list_client_impl_);
    ASSERT_TRUE(!search_box_view_);

    app_list_client_impl_ = AppListClientImpl::GetInstance();
    app_list_client_impl_->UpdateProfile();

    app_list_client_impl_->ShowAppList(ash::AppListShowSource::kSearchKey);
    // We dispatch mouse events to interact with UI. Wait animation completion
    // to reliably dispatch those events.
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/true);
    search_box_view_ = ash::GetSearchBoxView();
    ASSERT_TRUE(search_box_view_);
  }

  void OpenAppListWithIph() {
    OpenAppList();

    // There is an async call for checking IPH trigger condition.
    ViewWaiter(search_box_view_, ash::LauncherSearchIphView::ViewId::kSelf)
        .Run();
    ASSERT_TRUE(IsLauncherSearchIphViewVisible());
  }

  raw_ptr<AppListClientImpl> app_list_client_impl() {
    return app_list_client_impl_;
  }

  raw_ptr<ash::SearchBoxView> search_box_view() { return search_box_view_; }

 protected:
  std::unique_ptr<feature_engagement::test::ScopedIphFeatureList>
      scoped_iph_feature_list_;

 private:
  raw_ptr<AppListClientImpl> app_list_client_impl_ = nullptr;
  raw_ptr<ash::SearchBoxView> search_box_view_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTest,
                       LauncherSearchIphNotShownByDefault) {
  raw_ptr<AppListClientImpl> client_impl = AppListClientImpl::GetInstance();
  client_impl->UpdateProfile();

  client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode, LauncherSearchIph) {
  OpenAppListWithIph();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());
  EXPECT_TRUE(search_box_view()->assistant_button()->GetBackground());

  // Dismiss the app list and show it again. IPH won't be shown this time. Note
  // that this is IPH demo mode behavior.
  app_list_client_impl()->DismissView();
  ASSERT_FALSE(app_list_client_impl()->GetAppListWindow());

  app_list_client_impl()->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
  // Launcher search iph installs a background to assistant button. It should be
  // removed if the iph gets dismissed.
  EXPECT_FALSE(search_box_view()->assistant_button()->GetBackground());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode,
                       LauncherSearchIphSearch) {
  OpenAppListWithIph();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH gets dismissed.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode,
                       LauncherSearchIphAssistant) {
  OpenAppListWithIph();

  // Clicks Assistant button to open Assistant UI and confirm that IPH gets
  // dismissed.
  raw_ptr<views::ImageButton> assistant_button =
      search_box_view()->assistant_button();
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode, ClickChip) {
  OpenAppListWithIph();

  raw_ptr<views::View> chip = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kChipStart);
  ASSERT_TRUE(chip);
  Click(chip);

  EXPECT_EQ(u"Weather",
            app_list_client_impl()->search_controller()->get_query());
  EXPECT_EQ(ash::AppListBubblePage::kSearch,
            ash::GetAppListBubbleView()->current_page_for_test());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode, ClickAssistant) {
  OpenAppListWithIph();

  raw_ptr<views::View> assistant_button = search_box_view()->GetViewByID(
      ash::LauncherSearchIphView::ViewId::kAssistant);
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_EQ(ash::AppListBubblePage::kAssistant,
            ash::GetAppListBubbleView()->current_page_for_test());
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowserTestWithDemoMode,
                       NoIphWithoutAssistant) {
  // `AssistantTestApiImpl::SetAssistantEnabled` asserts that the value has
  // taken effect, i.e. we are sure that Assistant gets disabled after this
  // call.
  DisableAssistant();

  OpenAppList();

  // There is an async call for IPH to be shown. This test expects that IPH does
  // NOT get shown. But run `RunUntilIdle` as this test can get failed if we
  // starts showing an IPH for this case.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
  EXPECT_FALSE(search_box_view()->assistant_button()->GetBackground());
}

// The bool param indicates if the AssistantLearnMore feature is enabled or not.
class AppListIphBrowserTestWithLearnMoreToast
    : public AppListIphBrowserTestWithDemoMode,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_iph_feature_list_ =
        std::make_unique<feature_engagement::test::ScopedIphFeatureList>();
    if (GetParam()) {
      scoped_iph_feature_list_->InitAndEnableFeatures(
          {ash::assistant::features::kEnableAssistantLearnMore});
    }
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_P(AppListIphBrowserTestWithLearnMoreToast,
                       ShowAssistantLearnMoreToast) {
  OpenAppList();

  auto* assistant_button = search_box_view()->assistant_button();
  ASSERT_TRUE(assistant_button);
  Click(assistant_button);

  EXPECT_EQ(ash::AppListBubblePage::kAssistant,
            ash::GetAppListBubbleView()->current_page_for_test());

  ash::AssistantZeroStateView* zero_state_view =
      static_cast<ash::AssistantZeroStateView*>(
          ash::GetAppListBubbleView()->GetViewByID(
              ash::AssistantViewID::kZeroStateView));
  ASSERT_TRUE(zero_state_view->GetVisible());

  ash::AppListToastView* learn_more_toast = static_cast<ash::AppListToastView*>(
      ash::GetAppListBubbleView()->GetViewByID(
          ash::AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  if (GetParam()) {
    ASSERT_TRUE(learn_more_toast->GetVisible());
    ASSERT_TRUE(learn_more_toast->IsDrawn());
  } else {
    ASSERT_FALSE(learn_more_toast->GetVisible());
    ASSERT_FALSE(learn_more_toast->IsDrawn());
  }
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AppListIphBrowserTestWithLearnMoreToast,
                         /*values=*/testing::Bool());
