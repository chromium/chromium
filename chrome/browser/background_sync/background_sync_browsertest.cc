// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

// Scripts run by this test are defined in
// chrome/test/data/background_sync/background_sync_browsertest.js.

// URL of the test helper page that helps drive these tests.
const char kHelperPage[] = "/background_sync/background_sync_browsertest.html";
const char kTagName[] = "test";
const char kSuccessfulOperationPrefix[] = "ok - ";

}  // namespace

class BackgroundSyncBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundSyncBrowserTest() = default;

  BackgroundSyncBrowserTest(const BackgroundSyncBrowserTest&) = delete;
  BackgroundSyncBrowserTest& operator=(const BackgroundSyncBrowserTest&) =
      delete;

  ~BackgroundSyncBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &BackgroundSyncBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    SetUpBrowser(browser());
  }

  void SetUpBrowser(Browser* browser) {
    // Load the helper page that helps drive these tests.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, https_server_->GetURL(kHelperPage)));

    // Register the Service Worker that's required for Background Sync. The
    // behaviour without an activated worker is covered by layout tests.
    {
      std::string script_result;
      ASSERT_TRUE(RunScript("RegisterServiceWorker()", &script_result));
      ASSERT_EQ("ok - service worker registered", script_result);
    }
  }

  // ---------------------------------------------------------------------------
  // Helper functions.

  // Runs the |script| in the current tab and writes the output to |*result|.
  bool RunScript(const std::string& script, std::string* result) {
    *result = content::EvalJs(browser()
                                  ->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetPrimaryMainFrame(),
                              script)
                  .ExtractString();
    return true;
  }

  // Intercepts all requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().query() == "syncreceived") {
      time_when_sync_event_received_ = base::Time::Now();
    }

    // The default handlers will take care of this request.
    return nullptr;
  }

  std::string BuildScriptString(const std::string& function,
                                const std::string& argument) {
    return base::StringPrintf("%s('%s');", function.c_str(), argument.c_str());
  }

  std::string BuildExpectedResult(const std::string& tag,
                                  const std::string& action) {
    return base::StringPrintf("%s%s %s", kSuccessfulOperationPrefix,
                              tag.c_str(), action.c_str());
  }

  bool HasTag(const std::string& tag) {
    std::string script_result;
    EXPECT_TRUE(RunScript(BuildScriptString("hasTag", tag), &script_result));
    return script_result == BuildExpectedResult(tag, "found");
  }

 protected:
  base::Time time_when_sync_event_received_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, VerifyShutdownBehavior) {
  EXPECT_FALSE(HasTag(kTagName));

  chrome::CloseAllBrowsers();
  chrome::AttemptExit();
  RunUntilBrowserProcessQuits();

  EXPECT_LE(time_when_sync_event_received_, base::Time::Now());
}
