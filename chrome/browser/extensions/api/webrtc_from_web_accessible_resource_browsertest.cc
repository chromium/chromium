// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class WebRtcFromWebAccessibleResourceTest : public ExtensionApiTest {
 public:
  WebRtcFromWebAccessibleResourceTest() {}

  WebRtcFromWebAccessibleResourceTest(
      const WebRtcFromWebAccessibleResourceTest&) = delete;
  WebRtcFromWebAccessibleResourceTest& operator=(
      const WebRtcFromWebAccessibleResourceTest&) = delete;

  ~WebRtcFromWebAccessibleResourceTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("a.com", "127.0.0.1");
  }

 protected:
  GURL GetTestServerInsecureUrl(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);

    GURL::Replacements replace_host_and_scheme;
    replace_host_and_scheme.SetHostStr("a.com");
    replace_host_and_scheme.SetSchemeStr("http");
    url = url.ReplaceComponents(replace_host_and_scheme);

    return url;
  }

  void LoadTestExtension() {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
        "webrtc_from_web_accessible_resource")));
  }
};

// Verify that a chrome-extension:// web accessible URL can successfully access
// getUserMedia(), even if it is embedded in an insecure context.
IN_PROC_BROWSER_TEST_F(WebRtcFromWebAccessibleResourceTest,
                       GetUserMediaInWebAccessibleResourceSuccess) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadTestExtension();
  GURL url = GetTestServerInsecureUrl("/extensions/test_file.html?succeed");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager* request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents);
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_TRUE(permission_request_observer.request_shown());
}

// Verify that a chrome-extension:// web accessible URL will fail to access
// getUserMedia() if it is denied by the permission request, even if it is
// embedded in an insecure context.
IN_PROC_BROWSER_TEST_F(WebRtcFromWebAccessibleResourceTest,
                       GetUserMediaInWebAccessibleResourceFail) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadTestExtension();
  GURL url = GetTestServerInsecureUrl("/extensions/test_file.html?fail");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager* request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents);
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(catcher.GetNextResult());
  EXPECT_TRUE(permission_request_observer.request_shown());
}

}  // namespace extensions
