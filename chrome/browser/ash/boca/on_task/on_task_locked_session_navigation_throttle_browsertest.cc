// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NotNull;

namespace ash {
namespace {

class OnTaskLockedSessionNavigationThrottleBrowserTest
    : public InProcessBrowserTest {
 protected:
  OnTaskLockedSessionNavigationThrottleBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();
    ASSERT_TRUE(embedded_test_server()->Start());
    system_web_app_manager_ =
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile());

    // Set up OnTask session for testing purposes. Especially needed to ensure
    // newly created tabs are not deleted.
    GetOnTaskSessionManager()->OnSessionStarted("test_session",
                                                ::boca::UserIdentity());
  }

  void TearDownOnMainThread() override {
    system_web_app_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return FindSystemWebAppBrowser(profile(), SystemWebAppType::BOCA);
  }

  boca::OnTaskSessionManager* GetOnTaskSessionManager() {
    BocaManager* const boca_manager =
        BocaManagerFactory::GetInstance()->GetForProfile(profile());
    return boca_manager->GetOnTaskSessionManagerForTesting();
  }

  Profile* profile() { return browser()->profile(); }

  boca::OnTaskSystemWebAppManagerImpl* system_web_app_manager() {
    return system_web_app_manager_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<boca::OnTaskSystemWebAppManagerImpl> system_web_app_manager_;
};

IN_PROC_BROWSER_TEST_F(OnTaskLockedSessionNavigationThrottleBrowserTest,
                       AllowFormSubmission) {
  // Launch OnTask SWA.
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager()->LaunchSystemWebAppAsync(
      launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  ASSERT_TRUE(boca_app_browser->IsLockedForOnTask());

  // Set up window tracker to track the app window. This is needed to activate
  // the navigation throttle.
  const SessionID window_id = boca_app_browser->session_id();
  system_web_app_manager()->SetWindowTrackerForSystemWebAppWindow(
      window_id, /*observers=*/{});

  // Open and submit form. Verify the form was submitted by looking at the
  // visible URL (in case the navigation has not been committed yet).
  const GURL form_url(embedded_test_server()->GetURL("/form.html"));
  system_web_app_manager()->CreateBackgroundTabWithUrl(
      window_id, form_url, ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  auto* const tab_strip_model = boca_app_browser->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 2);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      boca_app_browser,
      GURL("javascript:document.getElementById('form').submit()")));
  EXPECT_NE(tab_strip_model->GetActiveWebContents()->GetVisibleURL(), form_url);
}

}  // namespace
}  // namespace ash
