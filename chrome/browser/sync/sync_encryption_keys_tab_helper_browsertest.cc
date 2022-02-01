// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class SyncEncryptionKeysTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  SyncEncryptionKeysTabHelperBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        prerender_helper_(base::BindRepeating(
            &SyncEncryptionKeysTabHelperBrowserTest::web_contents,
            base::Unretained(this))) {}

  ~SyncEncryptionKeysTabHelperBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  bool HasEncryptionKeysApi(content::RenderFrameHost* rfh) {
    auto* tab_helper =
        SyncEncryptionKeysTabHelper::FromWebContents(web_contents());
    return tab_helper->HasEncryptionKeysApiForTesting(rfh);
  }

 private:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override the sign-in URL so that it includes correct port from the test
    // server.
    command_line->AppendSwitchASCII(
        ::switches::kGaiaUrl,
        https_server()->GetURL("accounts.google.com", "/").spec());

    // Ignore cert errors so that the sign-in URL can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->StartAcceptingConnections();
  }

  net::EmbeddedTestServer https_server_;
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that chrome.setSyncEncryptionKeys() doesn't work in prerendering.
// If it is called in prerendering, it triggers canceling the prerendering
// and EncryptionKeyApi is not bound.
IN_PROC_BROWSER_TEST_F(SyncEncryptionKeysTabHelperBrowserTest,
                       ShouldNotBindEncryptionKeysApiInPrerendering) {
  base::HistogramTester histogram_tester;
  const GURL signin_url =
      https_server()->GetURL("accounts.google.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), signin_url));
  // EncryptionKeysApi is created for the primary page.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetMainFrame()));

  const GURL prerendering_url =
      https_server()->GetURL("accounts.google.com", "/simple.html");

  int host_id = prerender_helper().AddPrerender(prerendering_url);
  auto* prerendered_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  // EncryptionKeysApi is also created for prerendering since it's a main frame
  // as well.
  EXPECT_TRUE(HasEncryptionKeysApi(prerendered_frame_host));

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  const char kSetEncryptionKeysScript[] =
      "chrome.setSyncEncryptionKeys("
      "() => {console.log('setSyncEncryptionKeys:Done');}, \"\","
      "[new ArrayBuffer()], 0);";

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("setSyncEncryptionKeys:Done");

  // Calling setSyncEncryptionKeys() in the prerendered page triggers canceling
  // the prerendering since it's a associated interface and the default
  // policy is `MojoBinderAssociatedPolicy::kCancel`.
  std::ignore =
      content::ExecJs(prerendered_frame_host, kSetEncryptionKeysScript);
  host_observer.WaitForDestroyed();
  EXPECT_EQ(0u, console_observer.messages().size());
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderCancelledInterface.SpeculationRule",
      4 /*PrerenderCancelledInterface::kSyncEncryptionKeysExtension*/, 1);

  prerender_helper().NavigatePrimaryPage(prerendering_url);
  // Ensure that loading `prerendering_url` is not activated from prerendering.
  EXPECT_FALSE(host_observer.was_activated());
  // Ensure that the main frame has EncryptionKeysApi.
  EXPECT_TRUE(HasEncryptionKeysApi(web_contents()->GetMainFrame()));

  // Calling setSyncEncryptionKeys() in the primary page works and it gets
  // the callback by setSyncEncryptionKeys().
  EXPECT_TRUE(content::ExecJs(web_contents()->GetMainFrame(),
                              kSetEncryptionKeysScript));
  console_observer.Wait();
  EXPECT_EQ(1u, console_observer.messages().size());
}

}  // namespace
