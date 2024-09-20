// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
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
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include <tuple>
#include <variant>

#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "pdf/pdf_features.h"

namespace {
struct ChromeBackForwardCacheBrowserWithEmbedPdfTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<std::tuple<std::string_view, bool>>& i)
      const {
    return std::string(std::get<1>(i.param) ? "oopif_" : "guestview_") +
           std::string(std::get<0>(i.param));
  }
};
}  // namespace
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {
constexpr std::array<std::string_view, 2>
    kChromeBackForwardCacheBrowserWithEmbedTestValues = {"embed", "object"};
}  // namespace

class ChromeBackForwardCacheBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBackForwardCacheBrowserTest() = default;

  ChromeBackForwardCacheBrowserTest(const ChromeBackForwardCacheBrowserTest&) =
      delete;
  ChromeBackForwardCacheBrowserTest& operator=(
      const ChromeBackForwardCacheBrowserTest&) = delete;

  ~ChromeBackForwardCacheBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  GURL GetURL(const std::string& host) {
    return embedded_test_server()->GetURL(host, "/title1.html");
  }

  virtual std::vector<base::test::FeatureRefAndParams>
  GetEnabledFeaturesAndParams() const {
    return content::GetDefaultEnabledBackForwardCacheFeaturesForTesting();
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() const {
    return content::GetDefaultDisabledBackForwardCacheFeaturesForTesting(
        {// Entry to the cache can be slow during testing and cause
         // flakiness.
         features::kBackForwardCacheEntryTimeout});
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    // For using WebBluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    SetupFeaturesAndParameters();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void SetupFeaturesAndParameters() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeaturesAndParams(), GetDisabledFeatures());
    vmodule_switches_.InitWithSwitches("back_forward_cache_impl=1");
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  logging::ScopedVmoduleSwitches vmodule_switches_;
};

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // A is frozen in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Navigate back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // A is restored, B is stored.
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Navigate forward.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // A is stored, B is restored.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest, BasicIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("a.com")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Add an iframe B.
  EXPECT_TRUE(content::ExecJs(rfh_a.get(), R"(
    let url = new URL(location.href);
    url.hostname = 'b.com';
    let iframe = document.createElement('iframe');
    iframe.url = url;
    document.body.appendChild(iframe);
  )"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  content::RenderFrameHost* rfh_b = nullptr;
  rfh_a->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (rfh != rfh_a.get())
      rfh_b = rfh;
  });
  EXPECT_TRUE(rfh_b);
  content::RenderFrameHostWrapper rfh_b_wrapper(rfh_b);

  // 2) Navigate to C.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("c.com")));
  content::RenderFrameHostWrapper rfh_c(current_frame_host());

  // A and B are frozen. The page A(B) is stored in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_EQ(rfh_b_wrapper->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Navigate back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // The page A(B) is restored and C is frozen.
  EXPECT_EQ(rfh_c->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       PermissionContextBase) {
  // HTTPS needed for GEOLOCATION permission
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.test", "/title1.html"));
  GURL url_b(https_server.GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(web_contents(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  base::MockOnceCallback<void(blink::mojom::PermissionStatus)> callback;
  EXPECT_CALL(callback, Run(blink::mojom::PermissionStatus::ASK));
  browser()
      ->profile()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          rfh_a.get(),
          content::PermissionRequestDescription(
              blink::PermissionType::GEOLOCATION, /* user_gesture = */ true),
          callback.Get());

  // Ensure |rfh_a| is evicted from the cache because it is not allowed to
  // service the GEOLOCATION permission request.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
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
  content::RenderFrameHostWrapper rfh(current_frame_host());

  // Execute picture-in-picture on the page.
  ASSERT_EQ(true, content::EvalJs(web_contents(), "enterPictureInPicture();"));

  // Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), GetURL("b.com")));

  // The page uses Picture-in-Picture so it must be evicted from the cache and
  // deleted.
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoesNotCacheIfWebShare) {
  // HTTPS needed for WebShare permission.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.test", "/title1.html"));
  GURL url_b(https_server.GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Use the WebShare feature on the empty page.
  EXPECT_EQ("success", content::EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      navigator.share({title: 'the title'})
        .then(m => { resolve("success"); })
        .catch(error => { resolve(error.message); });
    });
  )"));

  // 2) Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // The page uses WebShare so it must be evicted from the cache and deleted.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
}

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoesNotCacheIfWebNfc) {
  // HTTPS needed for WebNfc permission.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL("a.test", "/title1.html"));
  GURL url_b(https_server.GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

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

  // 2) Navigate away.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // The page uses WebNfc so it must be evicted from the cache and deleted.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       RestoresMixedContentSettings) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server.Start());
  GURL url_a(https_server.GetURL("a.test",
                                 "/content_setting_bubble/mixed_script.html"));
  GURL url_b(https_server.GetURL("b.test",
                                 "/content_setting_bubble/mixed_script.html"));

  // 1) Load page A that has mixed content.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  // Mixed content should be blocked at first.
  EXPECT_FALSE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                   ->IsRunningInsecureContentAllowed(*current_frame_host()));

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
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Mixed content should no longer be blocked.
  EXPECT_TRUE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                  ->IsRunningInsecureContentAllowed(*current_frame_host()));

  // 4) Navigate to page B, which should use a different SiteInstance and
  // resets the mixed content settings.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  // Mixed content should be blocked in the new page.
  EXPECT_FALSE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                   ->IsRunningInsecureContentAllowed(*current_frame_host()));

  // 5) A is stored in BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 6) Go back to page A.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  // Mixed content settings is restored, so it's no longer blocked.
  EXPECT_TRUE(MixedContentSettingsTabHelper::FromWebContents(web_contents())
                  ->IsRunningInsecureContentAllowed(*current_frame_host()));
}

class MetricsChromeBackForwardCacheBrowserTest
    : public ChromeBackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  MetricsChromeBackForwardCacheBrowserTest() = default;
  ~MetricsChromeBackForwardCacheBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/40188113): This test used an experiment param (which no
    // longer exists) to suppress the metrics send timer. If and when the test
    // is re-enabled, it should be updated to use a different mechanism.
    ChromeBackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Flaky https://crbug.com/1224780
IN_PROC_BROWSER_TEST_P(MetricsChromeBackForwardCacheBrowserTest,
                       DISABLED_FirstInputDelay) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL(
      (GetParam() == "SameSite") ? "a.com" : "b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  internal::kHistogramFirstContentfulPaint),
              testing::IsEmpty());

  // 1) Navigate to url1.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url1));
  content::RenderFrameHostWrapper rfh_url1(current_frame_host());

  // Simulate mouse click. FirstInputDelay won't get updated immediately.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));
  // Run arbitrary script and run tasks in the browser to ensure the input is
  // processed in the renderer.
  EXPECT_TRUE(content::ExecJs(rfh_url1.get(), "var foo = 42;"));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);

  // 2) Immediately navigate to url2.
  if (GetParam() == "CrossSiteRendererInitiated") {
    EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents(), url2));
  } else {
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url2));
  }

  // Ensure |rfh_url1| is cached.
  EXPECT_EQ(rfh_url1->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  content::FetchHistogramsFromChildProcesses();
  if (GetParam() != "CrossSiteBrowserInitiated" ||
      rfh_url1.get()->GetProcess() == current_frame_host()->GetProcess()) {
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
    // TODO(crbug.com/40157795): Support updating metrics consistently on
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
    testing::ValuesIn(MetricsChromeBackForwardCacheBrowserTestValues()),
    [](const testing::TestParamInfo<std::string>& i) { return i.param; });

// Ensure that BackForwardCache RenderFrameHosts are shown in the Task Manager.
IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       ShowMainFrameInTaskManager) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  const std::u16string expected_url_a_active_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, u"Title Of Awesomeness");
  const std::u16string expected_url_a_cached_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX, u"http://a.com/");

  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  const std::u16string expected_url_b_active_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, u"Title Of More Awesomeness");
  const std::u16string expected_url_b_cached_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX, u"http://b.com/");

  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());

  // 1) Navigate to |url_a|.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to |url_b|.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_b));
  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // 3) Verify |url_a| is in the BackForwardCache.
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Ensure both tabs show up in Task Manager.
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_b_active_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_cached_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::ElementsAre(expected_url_b_active_title,
                                     expected_url_a_cached_title));

  // 5) Navigate back to |url_a|.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  // 6) Verify |url_b| is in the BackForwardCache.
  ASSERT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 7) Ensure both tabs show up in Task Manager.
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_active_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_b_cached_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::ElementsAre(expected_url_a_active_title,
                                     expected_url_b_cached_title));
}

// Ensure that BackForwardCache cross-site subframes are shown in the Task
// Manager.
IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       ShowCrossSiteOOPIFInTaskManager) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page on a.com with cross-site iframes on b.com and c.com.
  GURL url_a(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  const std::u16string expected_url_a_cached_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX, u"http://a.com/");
  const std::u16string expected_url_a_cached_subframe_b_title =
      l10n_util::GetStringFUTF16(
          IDS_TASK_MANAGER_BACK_FORWARD_CACHE_SUBFRAME_PREFIX,
          u"http://b.com/");
  const std::u16string expected_url_a_cached_subframe_c_title =
      l10n_util::GetStringFUTF16(
          IDS_TASK_MANAGER_BACK_FORWARD_CACHE_SUBFRAME_PREFIX,
          u"http://c.com/");

  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  const std::u16string expected_url_b_active_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, u"Title Of More Awesomeness");

  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());

  // 1) Navigate to |url_a|.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to |url_b|.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // 3) Verify |url_a| is in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Ensure the subframe tasks for |url_a| show up in Task Manager.
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_b_active_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_cached_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_cached_subframe_b_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_cached_subframe_c_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::UnorderedElementsAre(
                  expected_url_b_active_title, expected_url_a_cached_title,
                  expected_url_a_cached_subframe_b_title,
                  expected_url_a_cached_subframe_c_title));
}

// Ensure that BackForwardCache same-site subframes are not shown in the Task
// Manager.
IN_PROC_BROWSER_TEST_F(ChromeBackForwardCacheBrowserTest,
                       DoNotShowSameSiteSubframeInTaskManager) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page on a.com with an a.com iframe.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  const std::u16string expected_url_a_cached_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_BACK_FORWARD_CACHE_PREFIX, u"http://a.com/");

  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  const std::u16string expected_url_b_active_title = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, u"Title Of More Awesomeness");

  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());

  // 1) Navigate to |url_a|.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // 2) Navigate to |url_b|.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));

  // 3) Verify |url_a| is in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Ensure that only one task for |url_a| shows up in Task Manager.
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_b_active_title);
  task_manager::browsertest_util::WaitForTaskManagerRows(
      1, expected_url_a_cached_title);
  EXPECT_THAT(tester->GetWebContentsTaskTitles(),
              ::testing::ElementsAre(expected_url_b_active_title,
                                     expected_url_a_cached_title));
}

class ChromeBackForwardCacheBrowserWithEmbedTestBase
    : public ChromeBackForwardCacheBrowserTest {
 public:
  ChromeBackForwardCacheBrowserWithEmbedTestBase() = default;
  ~ChromeBackForwardCacheBrowserWithEmbedTestBase() override = default;

  static std::string GetSrcAttributeForTag(const std::string_view& tag) {
    return tag == "embed" ? "src" : "data";
  }

  void SetUpOnMainThread() override {
    ChromeBackForwardCacheBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location) {
    content::FetchHistogramsFromChildProcesses();
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    base::Bucket expected_blocklisted(sample, 1);

    EXPECT_THAT(histogram_tester_->GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "BlocklistedFeature"),
                testing::Contains(expected_blocklisted))
        << location.ToString();

    EXPECT_THAT(histogram_tester_->GetAllSamples(
                    "BackForwardCache.AllSites.HistoryNavigationOutcome."
                    "BlocklistedFeature"),
                testing::Contains(expected_blocklisted))
        << location.ToString();
  }
};

class ChromeBackForwardCacheBrowserWithEmbedTest
    : public ChromeBackForwardCacheBrowserWithEmbedTestBase,
      public ::testing::WithParamInterface<std::string_view> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBackForwardCacheBrowserWithEmbedTest,
    testing::ValuesIn(kChromeBackForwardCacheBrowserWithEmbedTestValues),
    [](const testing::TestParamInfo<std::string_view>& i) {
      return std::string(i.param);
    });

#if BUILDFLAG(ENABLE_PDF)
class ChromeBackForwardCacheBrowserWithEmbedPdfTest
    : public ChromeBackForwardCacheBrowserWithEmbedTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string_view, bool>> {
 public:
  void SetUpOnMainThread() override {
    ChromeBackForwardCacheBrowserWithEmbedTestBase::SetUpOnMainThread();

    if (UseOopif()) {
      factory_ = std::make_unique<pdf::TestPdfViewerStreamManagerFactory>();
    }
  }

  const std::string_view& html_tag() const { return std::get<0>(GetParam()); }

  bool UseOopif() const { return std::get<1>(GetParam()); }

  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager(
      content::WebContents* contents) {
    CHECK(UseOopif());
    return factory_->GetTestPdfViewerStreamManager(contents);
  }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeaturesAndParams()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        ChromeBackForwardCacheBrowserWithEmbedTestBase::
            GetEnabledFeaturesAndParams();
    if (UseOopif()) {
      enabled.push_back({chrome_pdf::features::kPdfOopif, {}});
    }
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    std::vector<base::test::FeatureRef> disabled =
        ChromeBackForwardCacheBrowserWithEmbedTestBase::GetDisabledFeatures();
    if (!UseOopif()) {
      disabled.push_back(chrome_pdf::features::kPdfOopif);
    }
    return disabled;
  }

  void ExpectNotRestoredReason(base::Location location) {
    // Reasons to fail caching pages embedding the PDF viewer. For OOPIF PDF
    // viewer, caching is disabled because it's contains a plugin. For GuestView
    // PDF viewer, the PDF viewer contains an inner WebContents. These values
    // should be kept in sync with BackForwardCacheMetrics::NotRestoredReason.
    static constexpr uint8_t kReasonBlocklistedFeatures = 7;
    static constexpr uint8_t kReasonHaveInnerContents = 32;

    content::FetchHistogramsFromChildProcesses();
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(
        UseOopif() ? kReasonBlocklistedFeatures : kReasonHaveInnerContents);
    base::Bucket expected_not_restored(sample, 1);

    EXPECT_THAT(histogram_tester_->GetAllSamples(
                    "BackForwardCache.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                testing::Contains(expected_not_restored))
        << location.ToString();

    EXPECT_THAT(histogram_tester_->GetAllSamples(
                    "BackForwardCache.AllSites.HistoryNavigationOutcome."
                    "NotRestoredReason"),
                testing::Contains(expected_not_restored))
        << location.ToString();
  }

 private:
  // `factory_` is necessary to create a `pdf::TestPdfViewerStreamManager`
  // instance whenever a PDF loads.
  std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory> factory_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBackForwardCacheBrowserWithEmbedPdfTest,
    testing::Combine(
        testing::ValuesIn(kChromeBackForwardCacheBrowserWithEmbedTestValues),
        testing::Bool()),
    ChromeBackForwardCacheBrowserWithEmbedPdfTestPassToString());
#endif  // BUILDFLAG(ENABLE_PDF)

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ChromeBackForwardCacheBrowserWithEmbedTestNoTestingConfig
    : public ChromeBackForwardCacheBrowserWithEmbedTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeBackForwardCacheBrowserWithEmbedTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBackForwardCacheBrowserWithEmbedTestNoTestingConfig,
    testing::ValuesIn(kChromeBackForwardCacheBrowserWithEmbedTestValues),
    [](const testing::TestParamInfo<std::string_view>& i) {
      return std::string(i.param);
    });

#if BUILDFLAG(ENABLE_PDF)
class ChromeBackForwardCacheBrowserWithEmbedPdfTestNoTestingConfig
    : public ChromeBackForwardCacheBrowserWithEmbedPdfTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeBackForwardCacheBrowserWithEmbedPdfTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeBackForwardCacheBrowserWithEmbedPdfTestNoTestingConfig,
    testing::Combine(
        testing::ValuesIn(kChromeBackForwardCacheBrowserWithEmbedTestValues),
        testing::Bool()),
    ChromeBackForwardCacheBrowserWithEmbedPdfTestPassToString());
#endif  // BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_P(
    ChromeBackForwardCacheBrowserWithEmbedTestNoTestingConfig,
    DoesNotCachePageWithEmbeddedPlugin) {
  const auto tag = GetParam();
  const auto page_with_plugin =
      base::StrCat({"/back_forward_cache/page_with_", tag, "_plugin.html"});

  // Navigate to A, a page with embedded Pepper plugin.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("a.com", page_with_plugin)));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Navigate to B.
  bool will_change_rfh =
      rfh_a->ShouldChangeRenderFrameHostOnSameSiteNavigation();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is NOT stored in the BackForwardCache.
  if (will_change_rfh) {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  // Navigate back to A.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));
  // Verify A is not restored from BackForwardCache due to |kContainsPlugins|.
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kContainsPlugins,
      FROM_HERE);
}

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_P(
    ChromeBackForwardCacheBrowserWithEmbedPdfTestNoTestingConfig,
    DoesNotCachePageWithEmbeddedPdf) {
  const auto tag = html_tag();
  const auto page_with_pdf =
      base::StrCat({"/back_forward_cache/page_with_", tag, "_pdf.html"});

  // Navigate to A, a page with embedded PDF.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", page_with_pdf)));
  if (UseOopif()) {
    ASSERT_TRUE(GetTestPdfViewerStreamManager(web_contents())
                    ->WaitUntilPdfLoadedInFirstChild());
  } else {
    pdf_extension_test_util::EnsurePDFHasLoadedOptions options{
        .pdf_element = std::string(tag)};
    ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoadedWithOptions(
        web_contents(), options));
  }
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Navigate to B.
  bool will_change_rfh =
      rfh_a->ShouldChangeRenderFrameHostOnSameSiteNavigation();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is NOT stored in the BackForwardCache.
  if (will_change_rfh) {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  // Navigate back to A.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));

  // Verify A is not restored from BackForwardCache.
  ExpectNotRestoredReason(FROM_HERE);
}

// Flaky: crbug.com/40935990
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DoesNotCachePageWithEmbeddedPdfAppendedOnPageLoaded \
  DISABLED_DoesNotCachePageWithEmbeddedPdfAppendedOnPageLoaded
#else
#define MAYBE_DoesNotCachePageWithEmbeddedPdfAppendedOnPageLoaded \
  DoesNotCachePageWithEmbeddedPdfAppendedOnPageLoaded
#endif
IN_PROC_BROWSER_TEST_P(ChromeBackForwardCacheBrowserWithEmbedPdfTest,
                       MAYBE_DoesNotCachePageWithEmbeddedPdfAppendedOnPageLoaded) {
  const auto tag = html_tag();

  // Navigate to A.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());
  //  Embed a PDF into A, and wait until PDF is loaded.
  EXPECT_EQ("success",
            content::EvalJs(rfh_a.get(), content::JsReplace(
                                             R"(
    new Promise(async resolve => {
      let el = document.createElement($1);
      el.type = 'application/pdf';
      el[$2] = '/pdf/test.pdf';
      el.onload = e => resolve("success");
      document.body.append(el);
    });
  )",
                                             tag, GetSrcAttributeForTag(tag))));
  if (UseOopif()) {
    // Wait for the PDF to fully load.
    ASSERT_TRUE(GetTestPdfViewerStreamManager(web_contents())
                    ->WaitUntilPdfLoadedInFirstChild());
  }

  // Navigate to B.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is NOT stored in the BackForwardCache.
  if (content::WillSameSiteNavigationChangeRenderFrameHosts(
          /*is_main_frame=*/true)) {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  //  Navigate back to A.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));

  // Verify A is not restored from BackForwardCache.
  ExpectNotRestoredReason(FROM_HERE);
}
#endif  // BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_P(ChromeBackForwardCacheBrowserWithEmbedTest,
                       DoesCachePageWithEmbeddedHtml) {
  const auto tag = GetParam();
  const auto page_with_html =
      base::StrCat({"/back_forward_cache/page_with_", tag, "_html.html"});

  // Navigate to A, a page with embedded HTML.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", page_with_html)));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());

  // Navigate to B.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is stored in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

#if BUILDFLAG(ENABLE_PDF)
// Flaky: crbug.com/40935990
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DoesNotCachePageWithEmbeddedHtmlMutatedIntoPdf \
  DISABLED_DoesNotCachePageWithEmbeddedHtmlMutatedIntoPdf
#else
#define MAYBE_DoesNotCachePageWithEmbeddedHtmlMutatedIntoPdf \
  DoesNotCachePageWithEmbeddedHtmlMutatedIntoPdf
#endif
IN_PROC_BROWSER_TEST_P(ChromeBackForwardCacheBrowserWithEmbedPdfTest,
                       MAYBE_DoesNotCachePageWithEmbeddedHtmlMutatedIntoPdf) {
  const auto tag = html_tag();
  const auto page_with_html =
      base::StrCat({"/back_forward_cache/page_with_", tag, "_html.html"});

  // Navigate to A, a page with embedded HTML.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", page_with_html)));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());
  //  Mutate the embed into PDF, and wait until PDF is loaded.
  EXPECT_EQ("success",
            content::EvalJs(rfh_a.get(), content::JsReplace(
                                             R"(
    new Promise(async resolve => {
      let el = document.getElementById($1);
      el.type = 'application/pdf';
      el[$2] = '/pdf/test.pdf';
      el.onload = e => resolve("success");
    });
  )",
                                             tag, GetSrcAttributeForTag(tag))));
  if (UseOopif()) {
    // Wait for the PDF to fully load.
    ASSERT_TRUE(GetTestPdfViewerStreamManager(web_contents())
                    ->WaitUntilPdfLoadedInFirstChild());
  }

  bool will_change_rfh =
      rfh_a->ShouldChangeRenderFrameHostOnSameSiteNavigation();
  // Navigate to B.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is NOT stored in the BackForwardCache.
  if (will_change_rfh) {
    EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_NE(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  // Navigate back to A.
  ASSERT_TRUE(content::HistoryGoBack(web_contents()));

  // Verify A is not restored from BackForwardCache.
  ExpectNotRestoredReason(FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(ChromeBackForwardCacheBrowserWithEmbedPdfTest,
                       DoesCachePageWithEmbeddedPdfMutatedIntoHtml) {
  const auto tag = html_tag();
  const auto page_with_pdf =
      base::StrCat({"/back_forward_cache/page_with_", tag, "_pdf.html"});

  // Navigate to A, a page with embedded PDF.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", page_with_pdf)));
  content::RenderFrameHostWrapper rfh_a(current_frame_host());
  //  Mutate the embed into HTML, and wait until HTML is loaded.
  EXPECT_EQ("success",
            content::EvalJs(rfh_a.get(), content::JsReplace(
                                             R"(
    new Promise(async resolve => {
      let el = document.getElementById($1);
      el.type = 'text/html';
      el[$2] = '/title1.html';
      el.onload = e => resolve("success");
    });
  )",
                                             tag, GetSrcAttributeForTag(tag))));

  // Navigate to B.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // Verify A is stored in the BackForwardCache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}
#endif  // BUILDFLAG(ENABLE_PDF)
