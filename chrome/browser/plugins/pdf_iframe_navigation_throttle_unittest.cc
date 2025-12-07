// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pdf/common/constants.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/common/buildflags.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "net/http/http_util.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#endif

using testing::NiceMock;

namespace {

const char kExampleURL[] = "http://example.com";

}  // namespace

class PDFIFrameNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetAlwaysOpenPdfExternallyForTests(bool always_open_pdf_externally) {
#if BUILDFLAG(ENABLE_PLUGINS)
    PluginPrefs::GetForTestingProfile(profile())
        ->SetAlwaysOpenPdfExternallyForTests(always_open_pdf_externally);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->RegisterProfile(profile());
#endif
  }

  void LoadPlugins() {
#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->GetPlugins();
#endif
  }

  scoped_refptr<net::HttpResponseHeaders> GetHeaderWithMimeType(
      const std::string& mime_type) {
    std::string raw_response_headers =
        "HTTP/1.1 200 OK\r\n"
        "content-type: " +
        mime_type + "\r\n";
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(raw_response_headers));
  }

  content::RenderFrameHost* subframe() { return subframe_; }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->Init();
    plugin_service->SetFilter(ChromePluginServiceFilter::GetInstance());

    // Register a fake PDF Viewer plugin into our plugin service.
    content::WebPluginInfo info;
    info.path = base::FilePath(ChromeContentClient::kPDFExtensionPluginPath);
    info.mime_types.emplace_back(pdf::kPDFMimeType, "pdf",
                                 "Fake PDF description");
    plugin_service->RegisterInternalPlugin(info);
    plugin_service->GetPlugins();
#endif

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");
  }

  raw_ptr<content::RenderFrameHost, DanglingUntriaged> subframe_;
};

TEST_F(PDFIFrameNavigationThrottleTest, OnlyCreateThrottleForSubframes) {
  // Disable the PDF plugin to test main vs. subframes.
  SetAlwaysOpenPdfExternallyForTests(true);

  // Never create throttle for main frames.
  content::MockNavigationHandle handle(GURL(kExampleURL), main_rfh());
  handle.set_response_headers(GetHeaderWithMimeType(""));
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(0u, registry.throttles().size());

  // Create a throttle for subframes.
  handle.set_render_frame_host(subframe());
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
}

TEST_F(PDFIFrameNavigationThrottleTest, InterceptPDFOnly) {
  // Setup.
  SetAlwaysOpenPdfExternallyForTests(true);
  LoadPlugins();

  NiceMock<content::MockNavigationHandle> handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(GetHeaderWithMimeType("application/pdf"));
  NiceMock<content::MockNavigationThrottleRegistry> registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  // Verify that we CANCEL for PDF mime type.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  auto* throttle = static_cast<PDFIFrameNavigationThrottle*>(
      registry.throttles().back().get());
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse().action());

  // Verify that we PROCEED for other mime types.
  // Blank mime type
  handle.set_response_headers(GetHeaderWithMimeType(""));
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  // HTML
  handle.set_response_headers(GetHeaderWithMimeType("text/html"));
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());

  // PNG
  handle.set_response_headers(GetHeaderWithMimeType("image/png"));
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

TEST_F(PDFIFrameNavigationThrottleTest, AllowPDFAttachments) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);

  // Verify that we PROCEED for PDF mime types with an attachment
  // content-disposition.
  std::string raw_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "content-type: application/pdf\r\n"
      "content-disposition: attachment\r\n";
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  content::MockNavigationHandle handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(headers.get());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry.throttles().back()->WillProcessResponse().action());
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PDFIFrameNavigationThrottleTest, ProceedIfPDFViewerIsEnabled) {
  content::MockNavigationHandle handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(GetHeaderWithMimeType("application/pdf"));
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  SetAlwaysOpenPdfExternallyForTests(false);

  // First time should synchronously let the navigation proceed.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry.throttles().back()->WillProcessResponse().action());
  registry.throttles().clear();

  // Subsequent times should synchronously let the navigation proceed.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            registry.throttles().back()->WillProcessResponse().action());
}

TEST_F(PDFIFrameNavigationThrottleTest, CancelIfPDFViewerIsDisabled) {
  NiceMock<content::MockNavigationHandle> handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(GetHeaderWithMimeType("application/pdf"));
  NiceMock<content::MockNavigationThrottleRegistry> registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  SetAlwaysOpenPdfExternallyForTests(true);

  // First time should synchronously cancel the navigation.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            registry.throttles().back()->WillProcessResponse().action());
  registry.throttles().clear();

  // Subsequent times should also synchronously cancel the navigation.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            registry.throttles().back()->WillProcessResponse().action());
}

TEST_F(PDFIFrameNavigationThrottleTest, MetricsPDFLoadStatus) {
  const char kPdfLoadStatusMetric[] = "PDF.LoadStatus2";
  base::HistogramTester histograms;
  histograms.ExpectBucketCount(
      kPdfLoadStatusMetric, PDFLoadStatus::kLoadedIframePdfWithNoPdfViewer, 0);

  // Setup.
  SetAlwaysOpenPdfExternallyForTests(true);
  LoadPlugins();

  NiceMock<content::MockNavigationHandle> handle(GURL(kExampleURL), subframe());
  handle.set_response_headers(GetHeaderWithMimeType("application/pdf"));
  NiceMock<content::MockNavigationThrottleRegistry> registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  // Verify that we CANCEL for PDF mime type.
  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            registry.throttles().back()->WillProcessResponse().action());

  histograms.ExpectUniqueSample(kPdfLoadStatusMetric,
                                PDFLoadStatus::kLoadedIframePdfWithNoPdfViewer,
                                /*expected_bucket_count=*/1);
}
#endif
