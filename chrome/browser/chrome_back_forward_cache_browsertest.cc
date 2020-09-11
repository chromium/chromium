// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

class ChromeBackForwardCacheBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBackForwardCacheBrowserTest() = default;
  ~ChromeBackForwardCacheBrowserTest() override = default;

  void SetUp() override {
    // Fake the BluetoothAdapter to say it's present.
    // Used in WebBluetooth test.
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
#if defined(OS_CHROMEOS)
    // In CHROMEOS build, even when |adapter_| object is released at TearDown()
    // it causes the test to fail on exit with an error indicating |adapter_| is
    // leaked.
    testing::Mock::AllowLeak(adapter_.get());
#endif

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(adapter_.get());
    adapter_.reset();
    InProcessBrowserTest::TearDown();
  }

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
  scoped_refptr<device::MockBluetoothAdapter> adapter_;

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
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // A is restored, B is stored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  EXPECT_EQ("rfh_a", content::EvalJs(rfh_a, "token"));

  // 4) Navigate forward.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

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
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

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
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // The page A(B) is restored.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ("rfh_a", content::EvalJs(rfh_a, "token"));
  EXPECT_EQ("rfh_b", content::EvalJs(rfh_b, "token"));
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, WebBluetooth) {
  // The test requires a mock Bluetooth adapter to perform a
  // WebBluetooth API call. To avoid conflicts with the default Bluetooth
  // adapter, e.g. Windows adapter, which is configured during Bluetooth
  // initialization, the mock adapter is configured in SetUp().

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

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       PermissionContextBase) {
  // HTTPS needed for GEOLOCATION permission
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.com", "/title1.html"));
  GURL url_b(https_server.GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(web_contents(), url_a));
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(web_contents(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  base::MockOnceCallback<void(ContentSetting)> callback;
  EXPECT_CALL(callback, Run(ContentSetting::CONTENT_SETTING_ASK));
  PermissionManagerFactory::GetForProfile(browser()->profile())
      ->RequestPermission(ContentSettingsType::GEOLOCATION, rfh_a, url_a,
                          /* user_gesture = */ true, callback.Get());

  delete_observer_rfh_a.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoesNotCacheIfPictureInPicture) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with picture-in-picture functionality.
  const base::FilePath::CharType picture_in_picture_page[] =
      FILE_PATH_LITERAL("media/picture-in-picture/window-size.html");
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(picture_in_picture_page));
  EXPECT_TRUE(content::NavigateToURL(web_contents(), test_page_url));

  // Execute picture-in-picture on the page.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  content::RenderFrameDeletedObserver deleted(current_frame_host());

  // Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));

  // The page uses Picture-in-Picture so it should be deleted.
  deleted.WaitUntilDeleted();
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoesNotCacheIfWebShare) {
  // HTTPS needed for WebShare permission.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.com", "/title1.html"));
  GURL url_b(https_server.GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));

  // Use the WebShare feature on the empty page.
  EXPECT_EQ("success", content::EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.share({title: 'the title'})
        .then(m => { resolve("success"); })
        .catch(error => { resolve(error.message); });
    });
  )"));

  content::RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // The page uses WebShare so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoesNotCacheIfWebNfc) {
  // HTTPS needed for WebNfc permission.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.com", "/title1.html"));
  GURL url_b(https_server.GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));

  // Use the WebNfc feature on the empty page.
  EXPECT_EQ("success", content::EvalJs(current_frame_host(), R"(
    const writer = new NDEFWriter();
    new Promise(async resolve => {
      try {
        await writer.write("Hello");
        resolve('success');
      } catch (error) {
        resolve(error.message);
      }
    });
  )"));

  content::RenderFrameDeletedObserver deleted(current_frame_host());

  // 2) Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // The page uses WebShare so it should be deleted.
  deleted.WaitUntilDeleted();

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
}
#endif

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       RestoresMixedContentSettings) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());
  GURL url_a(https_server.GetURL("a.com",
                                 "/content_setting_bubble/mixed_script.html"));
  GURL url_b(https_server.GetURL("b.com",
                                 "/content_setting_bubble/mixed_script.html"));

  // 1) Load page A that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  // Mixed content should be blocked at first.
  EXPECT_FALSE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                   ->IsRunningInsecureContentAllowed());

  // 2) Emulate link clicking on the mixed script bubble to allow mixed content
  // to run.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // 3) Wait for reload.
  observer.Wait();

  // Mixed content should no longer be blocked.
  EXPECT_TRUE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                  ->IsRunningInsecureContentAllowed());

  // 4) Navigate to page B, which should use a different SiteInstance and
  // resets the mixed content settings.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  // Mixed content should be blocked in the new page.
  EXPECT_FALSE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                   ->IsRunningInsecureContentAllowed());

  // 5) Go back to page A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  // Mixed content settings is restored, so it's no longer blocked.
  EXPECT_TRUE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                  ->IsRunningInsecureContentAllowed());
}
