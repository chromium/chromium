// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/note_taking/note_taking_helper.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/note_taking_client.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

using ui_test_utils::BrowserChangeObserver;

class NoteTakingHelperBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAshForceEnableStylusTools);
  }

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  Profile* profile() { return browser()->profile(); }
};

IN_PROC_BROWSER_TEST_F(NoteTakingHelperBrowserTest, LaunchWebApp) {
  base::HistogramTester histogram_tester;
  // Install a web app with a note_taking_new_note_url.
  GURL new_note_url("http://some.url/new-note");
  auto app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("http://some.url"));
  app_info->scope = GURL("http://some.url");
  app_info->title = u"Web App 2";
  app_info->note_taking_new_note_url = new_note_url;
  app_info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(app_info));
  ASSERT_EQ(helper()->GetAvailableApps(profile()).size(), 1u);

  BrowserChangeObserver observer(nullptr,
                                 BrowserChangeObserver::ChangeType::kAdded);
  NoteTakingClient::GetInstance()->CreateNote();
  Browser* app_browser = observer.Wait();

  ASSERT_TRUE(app_browser->tab_strip_model()->GetActiveWebContents());
  GURL url = app_browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  ASSERT_EQ(new_note_url, url);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(NoteTakingHelper::LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(NoteTakingHelper::LaunchResult::WEB_APP_SUCCESS), 1);
}

IN_PROC_BROWSER_TEST_F(NoteTakingHelperBrowserTest, LaunchHardcodedWebApp) {
  GURL app_url("https://yielding-large-chef.glitch.me/");
  // Install a default-allowed web app corresponding to ID of
  // |NoteTakingHelper::kNoteTakingWebAppIdTest|.
  auto app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  app_info->title = u"Default Allowed Web App";
  app_info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(app_info));
  ASSERT_EQ(app_id, NoteTakingHelper::kNoteTakingWebAppIdTest);

  // Fire a "Create Note" action and check the app is launched.
  BrowserChangeObserver observer(nullptr,
                                 BrowserChangeObserver::ChangeType::kAdded);
  NoteTakingClient::GetInstance()->CreateNote();
  Browser* app_browser = observer.Wait();

  ASSERT_TRUE(app_browser->tab_strip_model()->GetActiveWebContents());
  GURL url = app_browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  ASSERT_EQ(app_url, url);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
}

}  // namespace
}  // namespace ash
