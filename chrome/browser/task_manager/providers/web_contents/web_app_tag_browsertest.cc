// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::Contains;
using testing::Property;

namespace task_manager {

class WebAppTagWebAppTest : public web_app::WebAppControllerBrowserTest {
 protected:
  Browser* LaunchBrowserForWebAppInTabAndWait(const webapps::AppId& app_id,
                                              const GURL& observe_url) {
    ui_test_utils::UrlLoadObserver url_observer(
        observe_url, content::NotificationService::AllSources());
    Browser* browser = LaunchBrowserForWebAppInTab(app_id);
    url_observer.Wait();
    return browser;
  }

  const std::vector<WebContentsTag*>& tracked_tags() const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

  void NavigateToUrlAndWait(Browser* browser, const GURL& url) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();

    {
      content::TestNavigationObserver observer(web_contents);
      NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
      ui_test_utils::NavigateToURL(&params);
      observer.WaitForNavigationFinished();
    }
  }
};

IN_PROC_BROWSER_TEST_F(WebAppTagWebAppTest, WebAppTaskCreatedForTab) {
  EXPECT_EQ(1U, tracked_tags().size());

  const GURL start_url =
      https_server()->GetURL("app.com", "/google/google.html");
  const webapps::AppId app_id = InstallPWA(start_url);

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  EXPECT_EQ(1U, tracked_tags().size());

  Browser* browser = LaunchBrowserForWebAppInTabAndWait(app_id, start_url);
  ASSERT_TRUE(browser);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing.
  task_manager.StartObserving();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, task_manager.tasks().size());

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Google")));
}

IN_PROC_BROWSER_TEST_F(WebAppTagWebAppTest, WebAppTaskCreatedForStandalone) {
  EXPECT_EQ(1U, tracked_tags().size());

  const GURL start_url =
      https_server()->GetURL("app.com", "/google/google.html");
  const webapps::AppId app_id = InstallPWA(start_url);

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  EXPECT_EQ(1U, tracked_tags().size());

  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  ASSERT_TRUE(browser);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing.
  task_manager.StartObserving();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, task_manager.tasks().size());

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Google")));
}

IN_PROC_BROWSER_TEST_F(WebAppTagWebAppTest, TabNavigatedAwayNotWebAppTask) {
  EXPECT_EQ(1U, tracked_tags().size());

  const GURL start_url =
      https_server()->GetURL("app.com", "/google/google.html");
  const webapps::AppId app_id = InstallPWA(start_url);

  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  EXPECT_EQ(1U, tracked_tags().size());

  Browser* browser = LaunchBrowserForWebAppInTabAndWait(app_id, start_url);
  ASSERT_TRUE(browser);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing.
  task_manager.StartObserving();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, task_manager.tasks().size());

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Google")));

  const GURL not_app_url =
      https_server()->GetURL("notapp.com", "/google/google.html");

  NavigateToUrlAndWait(browser, not_app_url);

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"Tab: Google")));
}

class WebAppTagIsolatedWebAppTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 protected:
  webapps::AppId InstallIsolatedWebApp() {
    server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    web_app::IsolatedWebAppUrlInfo url_info =
        InstallDevModeProxyIsolatedWebApp(server_->GetOrigin());
    return url_info.app_id();
  }

  const std::vector<WebContentsTag*>& tracked_tags() const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

  void NavigateToUrlAndWait(Browser* browser, const GURL& url) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();

    {
      content::TestNavigationObserver observer(web_contents);
      NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
      ui_test_utils::NavigateToURL(&params);
      observer.WaitForNavigationFinished();
    }
  }

  net::EmbeddedTestServer* server() { return server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(WebAppTagIsolatedWebAppTest, IsolatedWebAppTaskCreated) {
  EXPECT_EQ(1U, tracked_tags().size());

  webapps::AppId app_id = InstallIsolatedWebApp();
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  EXPECT_EQ(1U, tracked_tags().size());

  Browser* browser = LaunchWebAppBrowserAndWait(app_id);

  ASSERT_TRUE(browser);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing.
  task_manager.StartObserving();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, task_manager.tasks().size());

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Isolated Web App")));
}

IN_PROC_BROWSER_TEST_F(WebAppTagIsolatedWebAppTest,
                       IsolatedWebAppTaskTitleFallbackToAppName) {
  EXPECT_EQ(1U, tracked_tags().size());

  webapps::AppId app_id = InstallIsolatedWebApp();
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  EXPECT_EQ(1U, tracked_tags().size());

  Browser* browser = LaunchWebAppBrowserAndWait(app_id);

  ASSERT_TRUE(browser);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing.
  task_manager.StartObserving();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, task_manager.tasks().size());

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Isolated Web App")));

  GURL iwa_url =
      browser->tab_strip_model()->GetActiveWebContents()->GetLastCommittedURL();
  ASSERT_TRUE(iwa_url.SchemeIs(chrome::kIsolatedAppScheme));

  GURL empty_title_url =
      url::Origin::Create(iwa_url).GetURL().Resolve("/empty_title.html");

  NavigateToUrlAndWait(browser, empty_title_url);

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(task_manager.tasks(),
              Contains(Property(&Task::title, u"App: Simple Isolated App")));
}

}  // namespace task_manager
