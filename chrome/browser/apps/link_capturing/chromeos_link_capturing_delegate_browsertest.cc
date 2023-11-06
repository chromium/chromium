// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace apps {
namespace {

using ui_test_utils::BrowserChangeObserver;

// Test for ChromeOS-specific link capturing features. Features which are shared
// between ChromeOS and Windows/Mac/Linux are tested in
// WebAppLinkCapturingBrowserTest.
class ChromeOsLinkCapturingDelegateBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  void Navigate(Browser* source, const GURL& url, LinkTarget target) {
    ClickLinkAndWait(source->tab_strip_model()->GetActiveWebContents(), url,
                     target, /*rel=*/"");
  }

  Browser* NavigateExpectingNewBrowser(Browser* source,
                                       const GURL& url,
                                       LinkTarget target) {
    BrowserChangeObserver observer(nullptr,
                                   BrowserChangeObserver::ChangeType::kAdded);
    ClickLink(source->tab_strip_model()->GetActiveWebContents(), url, target);
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

// Verifies that target="_blank" links clicked in a web app window are link
// captured, when AppToAppLinkCapturing is enabled.
IN_PROC_BROWSER_TEST_F(ChromeOsLinkCapturingDelegateBrowserTest,
                       AppToAppLinkCaptureTargetBlank) {
  InstallTestWebApp();
  webapps::AppId target_app = InstallTestWebApp("www.foo.com", "/");

  Browser* app_browser = OpenTestWebApp();
  Browser* result_browser = NavigateExpectingNewBrowser(
      app_browser, https_server().GetURL("www.foo.com", "/launch"),
      LinkTarget::BLANK);

  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(result_browser, target_app));
}

// Verifies that target="_blank" links clicked in a web app window are link
// captured after a redirect, when AppToAppLinkCapturing is enabled.
IN_PROC_BROWSER_TEST_F(ChromeOsLinkCapturingDelegateBrowserTest,
                       AppToAppLinkCaptureTargetBlankWithRedirect) {
  InstallTestWebApp();
  webapps::AppId target_app = InstallTestWebApp("www.foo.com", "/");

  Browser* app_browser = OpenTestWebApp();
  GURL in_scope = https_server().GetURL("www.foo.com", "/launch");
  Browser* result_browser = NavigateExpectingNewBrowser(
      app_browser,
      https_server().GetURL("www.bar.com",
                            "/server-redirect?" + in_scope.spec()),
      LinkTarget::BLANK);

  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(result_browser, target_app));
}

// Verifies that link clicks using window.open clicked in a web app window are
// link captured, when AppToAppLinkCapturing is enabled.
IN_PROC_BROWSER_TEST_F(ChromeOsLinkCapturingDelegateBrowserTest,
                       AppToAppLinkCaptureWindowOpenTargetBlank) {
  InstallTestWebApp();
  webapps::AppId target_app = InstallTestWebApp("www.foo.com", "/");

  Browser* app_browser = OpenTestWebApp();

  std::string script = R"js(
    const link = document.createElement('a');
    link.onclick = () => {
      window.open('about:blank', 'target=_blank').location.href = '$1';
    }
    link.style = 'position: absolute; top: 0; left: 0; bottom: 0; right: 0;';
    document.body.appendChild(link);
  )js";
  auto* web_contents = app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      base::ReplaceStringPlaceholders(
          script, {https_server().GetURL("www.foo.com", "/launch").spec()},
          /*offsets=*/{})));

  BrowserChangeObserver observer(nullptr,
                                 BrowserChangeObserver::ChangeType::kAdded);
  content::SimulateMouseClick(web_contents,
                              blink::WebInputEvent::Modifiers::kNoModifiers,
                              blink::WebMouseEvent::Button::kLeft);
  Browser* result_browser = observer.Wait();

  EXPECT_TRUE(
      web_app::AppBrowserController::IsForWebApp(result_browser, target_app));
}

IN_PROC_BROWSER_TEST_F(ChromeOsLinkCapturingDelegateBrowserTest,
                       NoLinkCaptureAfterBrowserClick) {
  InstallTestWebApp();
  webapps::AppId target_app = InstallTestWebApp("www.foo.com", "/");

  Browser* app_browser = OpenTestWebApp();

  // Clicking an out-of-scope link should open in a normal browser.
  GURL out_of_scope = https_server().GetURL("www.bar.com", "/");
  Navigate(app_browser, out_of_scope, LinkTarget::BLANK);
  ASSERT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope);

  // From there, clicking an in-scope link should not app-to-app link capture.
  GURL in_scope = https_server().GetURL("www.foo.com", "/launch");
  Navigate(browser(), in_scope, LinkTarget::BLANK);
  ASSERT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      in_scope);
}

}  // namespace
}  // namespace apps
