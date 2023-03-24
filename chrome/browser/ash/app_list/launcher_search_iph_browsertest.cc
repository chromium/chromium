// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/assistant/assistant_test_api_impl.h"
#include "ash/app_list/views/launcher_search_iph_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
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
      search_box_view->GetViewByID(ash::LauncherSearchIphView::kViewId);
  if (!launcher_search_iph_view) {
    return false;
  }

  return launcher_search_iph_view->GetVisible();
}
}  // namespace

class AppListIphBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ash::Shell::Get()->assistant_controller()->SetAssistant(&test_service_);
    test_api_impl_.EnableAssistantAndWait();
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    test_api_impl_.SetAssistantEnabled(false);
  }

 private:
  ash::TestAssistantService test_service_;
  ash::AssistantTestApiImpl test_api_impl_;
};

class AppListIphBrowerTestWithDemoMode : public AppListIphBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_iph_feature_list_ =
        std::make_unique<feature_engagement::test::ScopedIphFeatureList>();
    scoped_iph_feature_list_->InitForDemo(
        feature_engagement::kIPHLauncherSearchHelpUiFeature);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  std::unique_ptr<feature_engagement::test::ScopedIphFeatureList>
      scoped_iph_feature_list_;
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

IN_PROC_BROWSER_TEST_F(AppListIphBrowerTestWithDemoMode, LauncherSearchIph) {
  raw_ptr<AppListClientImpl> client_impl = AppListClientImpl::GetInstance();
  client_impl->UpdateProfile();

  client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  raw_ptr<ash::SearchBoxView> search_box_view = ash::GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  // There is an async call for checking IPH trigger condition.
  ViewWaiter(search_box_view, ash::LauncherSearchIphView::kViewId).Run();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Dismiss the app list and show it again. IPH won't be shown this time. Note
  // that this is IPH demo mode behavior.
  client_impl->DismissView();
  ASSERT_FALSE(client_impl->GetAppListWindow());

  client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowerTestWithDemoMode,
                       LauncherSearchIphSearch) {
  raw_ptr<AppListClientImpl> client_impl = AppListClientImpl::GetInstance();
  client_impl->UpdateProfile();

  client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  raw_ptr<ash::SearchBoxView> search_box_view = ash::GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  // There is an async call for checking IPH trigger condition.
  ViewWaiter(search_box_view, ash::LauncherSearchIphView::kViewId).Run();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Do search and confirm that the IPH gets dismissed.
  ash::AppListTestApi().SimulateSearch(u"Test");
  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}

IN_PROC_BROWSER_TEST_F(AppListIphBrowerTestWithDemoMode,
                       LauncherSearchIphAssistant) {
  raw_ptr<AppListClientImpl> client_impl = AppListClientImpl::GetInstance();
  client_impl->UpdateProfile();

  client_impl->ShowAppList(ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  raw_ptr<ash::SearchBoxView> search_box_view = ash::GetSearchBoxView();
  ASSERT_TRUE(search_box_view);
  // There is an async call for checking IPH trigger condition.
  ViewWaiter(search_box_view, ash::LauncherSearchIphView::kViewId).Run();
  EXPECT_TRUE(IsLauncherSearchIphViewVisible());

  // Clicks Assistant button to open Assistant UI and confirm that IPH gets
  // dismissed.
  raw_ptr<views::ImageButton> assistant_button =
      search_box_view->assistant_button();
  ASSERT_TRUE(assistant_button);
  ui::test::EventGenerator event_generator(
      assistant_button->GetWidget()->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseToInHost(
      assistant_button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_FALSE(IsLauncherSearchIphViewVisible());
}
