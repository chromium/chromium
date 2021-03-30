// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics_constants.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

namespace {

// hash for std::unordered_map.
struct FeatureHash {
  size_t operator()(base::Feature feature) const {
    return base::FastHash(feature.name);
  }
};

// compare operator for std::unordered_map.
struct FeatureEqualOperator {
  bool operator()(base::Feature feature1, base::Feature feature2) const {
    return std::strcmp(feature1.name, feature2.name) == 0;
  }
};
}  // namespace

class ChromeBackForwardCacheBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBackForwardCacheBrowserTest() = default;
  ~ChromeBackForwardCacheBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    histogram_tester_ = std::make_unique<base::HistogramTester>();
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

    EnableFeatureAndSetParams(features::kBackForwardCache,
                              "TimeToLiveInBackForwardCacheInSeconds", "3600");
    EnableFeatureAndSetParams(features::kBackForwardCache, "enable_same_site",
                              "true");
    // Allow BackForwardCache for all devices regardless of their memory.
    DisableFeature(features::kBackForwardCacheMemoryControls);

    SetupFeaturesAndParameters();

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  void SetupFeaturesAndParameters() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    for (const auto& feature_param : features_with_params_) {
      enabled_features.emplace_back(feature_param.first, feature_param.second);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features_);
  }

  void EnableFeatureAndSetParams(base::Feature feature,
                                 std::string param_name,
                                 std::string param_value) {
    features_with_params_[feature][param_name] = param_value;
  }

  void DisableFeature(base::Feature feature) {
    disabled_features_.push_back(feature);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unordered_map<base::Feature,
                     std::map<std::string, std::string>,
                     FeatureHash,
                     FeatureEqualOperator>
      features_with_params_;
  std::vector<base::Feature> disabled_features_;

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
    const ndef = new NDEFReader();
    new Promise(async resolve => {
      try {
        await ndef.write("Hello");
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

class MetricsChromeBackForwardCacheBrowserTest
    : public ChromeBackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  MetricsChromeBackForwardCacheBrowserTest() = default;
  ~MetricsChromeBackForwardCacheBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set BufferTimerDelayMillis to a high number so that metrics update on the
    // renderer won't be sent to the browser by the periodic upload.
    EnableFeatureAndSetParams(
        page_load_metrics::kPageLoadMetricsTimerDelayFeature,
        "BufferTimerDelayMillis", "100000");
    ChromeBackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_P(MetricsChromeBackForwardCacheBrowserTest,
                       FirstInputDelay) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL(
      (GetParam() == "SameSite") ? "a.com" : "b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  internal::kHistogramFirstContentfulPaint),
              testing::IsEmpty());

  // 1) Navigate to url1.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url1));
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Simulate mouse click. FirstInputDelay won't get updated immediately.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));
  // Run arbitrary script and run tasks in the brwoser to ensure the input is
  // processed in the renderer.
  EXPECT_TRUE(content::ExecJs(rfh_a, "var foo = 42;"));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);

  // 2) Immediately navigate to url2.
  if (GetParam() == "CrossSiteRendererInitiated") {
    EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents(), url2));
  } else {
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url2));
  }
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  content::FetchHistogramsFromChildProcesses();
  if (GetParam() != "CrossSiteBrowserInitiated" ||
      rfh_a->GetProcess() == current_frame_host()->GetProcess()) {
    // - For "SameSite" case, since the old and new RenderFrame share a process,
    // the metrics update will be sent to the browser during commit and won't
    // get ignored, successfully updating the FirstInputDelay histogram.
    // - For "CrossSiteRendererInitiated" case, FirstInputDelay was sent when
    // the renderer-initiated navigation started on the old frame.
    // - For "CrossSiteBrowserInitiated" case, if the old and new RenderFrame
    // share a process, the metrics update will be sent to the browser during
    // commit and won't get ignored, successfully updating the histogram.
    histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 1);
  } else {
    // Note that in some cases the metrics might flakily get updated in time,
    // before the browser changed the current RFH. So, we can neither expect it
    // to be 0 all the time or 1 all the time.
    // TODO(crbug.com/1150242): Support updating metrics consistently on
    // cross-RFH cross-process navigations.
  }
}

std::vector<std::string> MetricsChromeBackForwardCacheBrowserTestValues() {
  return {"SameSite", "CrossSiteRendererInitiated",
          "CrossSiteBrowserInitiated"};
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsChromeBackForwardCacheBrowserTest,
    testing::ValuesIn(MetricsChromeBackForwardCacheBrowserTestValues()));
