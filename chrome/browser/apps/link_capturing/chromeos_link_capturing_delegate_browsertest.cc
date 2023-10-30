// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace apps {
namespace {

using ui_test_utils::BrowserChangeObserver;

// Test for ChromeOS-specific link capturing features. Features which are shared
// between ChromeOS and Windows/Mac/Linux are tested in
// WebAppLinkCapturingBrowserTest.
class ChromeOsLinkCapturingDelegateBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  Browser* NavigateExpectingNewBrowser(Browser* source,
                                       const GURL& url,
                                       LinkTarget target) {
    BrowserChangeObserver observer(nullptr,
                                   BrowserChangeObserver::ChangeType::kAdded);
    ClickLinkAndWait(source->tab_strip_model()->GetActiveWebContents(), url,
                     target, /*rel=*/"");
    return observer.Wait();
  }

 private:
  base::test::ScopedFeatureList features_{
      apps::features::kAppToAppLinkCapturing};
};

// Verifies that target="_self" links clicked in a web app window are link
// captured, when AppToAppLinkCapturing is enabled.
IN_PROC_BROWSER_TEST_F(ChromeOsLinkCapturingDelegateBrowserTest,
                       AppToAppLinkCaptureTargetSelf) {
  InstallTestWebApp();
  webapps::AppId target_app = InstallTestWebApp("www.foo.com", "/");

  Browser* app_browser = OpenTestWebApp();
  Browser* result_browser = NavigateExpectingNewBrowser(
      app_browser, https_server().GetURL("www.foo.com", "/launch"),
      LinkTarget::SELF);

  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(result_browser, target_app));
}

}  // namespace
}  // namespace apps
