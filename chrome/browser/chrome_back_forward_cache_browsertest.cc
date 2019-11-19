// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class ChromeBackForwardCacheBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBackForwardCacheBrowserTest() = default;
  ~ChromeBackForwardCacheBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // At the chrome layer, an outstanding request to /favicon.ico is made. It is
  // made by the renderer on behalf of the browser process. It counts as an
  // outstanding request, which prevents the page from entering the
  // BackForwardCache, as long as it hasn't resolved.
  //
  // There are no real way to wait for this to complete. Not waiting would make
  // the test potentially flaky. To prevent this, the no-favicon.html page is
  // used, the image is not loaded from the network.
  GURL GetURL(const std::string& host) {
    return embedded_test_server()->GetURL(
        host, "/back_forward_cache/no-favicon.html");
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    // For using WebBluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kBackForwardCache,
        {
            // Set a very long TTL before expiration (longer than the test
            // timeout) so tests that are expecting deletion don't pass when
            // they shouldn't.
            {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
        });

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBackForwardCacheBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(content::ExecJs(rfh_a, "token = 'rfh_a'"));

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));
  content::RenderFrameHost* rfh_b = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_TRUE(content::ExecJs(rfh_b, "token = 'rfh_b'"));

  // A is frozen in the BackForwardCache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Navigate back.
  web_contents()->GetController().GoBack();
  content::WaitForLoadStop(web_contents());

  // A is restored, B is stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  EXPECT_EQ("rfh_a", content::EvalJs(rfh_a, "token"));

  // 4) Navigate forward.
  web_contents()->GetController().GoForward();
  content::WaitForLoadStop(web_contents());

  // A is stored, B is restored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  EXPECT_EQ("rfh_b", content::EvalJs(rfh_b, "token"));
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, BasicIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(content::ExecJs(rfh_a, "token = 'rfh_a'"));

  // 2) Add an iframe B.
  EXPECT_TRUE(content::ExecJs(rfh_a, R"(
    let url = new URL(location.href);
    url.hostname = 'b.com';
    let iframe = document.createElement('iframe');
    iframe.url = url;
    document.body.appendChild(iframe);
  )"));
  content::WaitForLoadStop(web_contents());

  content::RenderFrameHost* rfh_b = nullptr;
  for (content::RenderFrameHost* rfh : web_contents()->GetAllFrames()) {
    if (rfh != rfh_a)
      rfh_b = rfh;
  }
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_TRUE(content::ExecJs(rfh_a, "token = 'rfh_a'"));
  EXPECT_TRUE(content::ExecJs(rfh_b, "token = 'rfh_b'"));

  // 2) Navigate to C.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("c.com")));

  // A and B are frozen. The page A(B) is stored in the BackForwardCache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // 3) Navigate back.
  web_contents()->GetController().GoBack();
  content::WaitForLoadStop(web_contents());

  // The page A(B) is restored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ("rfh_a", content::EvalJs(rfh_a, "token"));
  EXPECT_EQ("rfh_b", content::EvalJs(rfh_b, "token"));
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, WebBluetooth) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // WebBluetooth requires HTTPS.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());
  GURL url(https_server.GetURL("a.com", "/back_forward_cache/no-favicon.html"));

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  content::BackForwardCacheDisabledTester tester;

  EXPECT_EQ("device not found", content::EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.bluetooth.requestDevice({
        filters: [
          { services: [0x1802, 0x1803] },
        ]
      })
      .then(() => resolve("device found"))
      .catch(() => resolve("device not found"))
    });
  )"));
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      current_frame_host()->GetProcess()->GetID(),
      current_frame_host()->GetRoutingID(), "WebBluetooth"));

  ASSERT_TRUE(embedded_test_server()->Start());
  content::RenderFrameDeletedObserver delete_observer(current_frame_host());
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));
  delete_observer.WaitUntilDeleted();
}
