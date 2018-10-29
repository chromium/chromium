// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_util.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace {

const char kHeader[] = "HTTP/1.1 200 OK\r\n";
const char kExampleURL[] = "http://example.com";

#if BUILDFLAG(ENABLE_PLUGINS)
void PluginsLoadedCallback(base::OnceClosure callback,
                           const std::vector<content::WebPluginInfo>& plugins) {
  std::move(callback).Run();
}
#endif

}  // namespace

class PDFIFrameNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetAlwaysOpenPdfExternallyForTests(bool always_open_pdf_externally) {
#if BUILDFLAG(ENABLE_PLUGINS)
    PluginPrefs::GetForTestingProfile(profile())
        ->SetAlwaysOpenPdfExternallyForTests(always_open_pdf_externally);
    ChromePluginServiceFilter* filter =
        ChromePluginServiceFilter::GetInstance();
    filter->RegisterResourceContext(profile(), profile()->GetResourceContext());
#endif
  }

  std::string GetHeaderWithMimeType(const std::string& mime_type) {
    return "HTTP/1.1 200 OK\r\n"
           "content-type: " +
           mime_type + "\r\n";
  }

  content::RenderFrameHost* subframe() { return subframe_; }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();

    // Load plugins.
    base::RunLoop run_loop;
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoadedCallback, run_loop.QuitClosure()));
    run_loop.Run();
#endif

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");

    feature_list_.InitAndEnableFeature(features::kClickToOpenPDFPlaceholder);
  }

  base::test::ScopedFeatureList feature_list_;
  content::RenderFrameHost* subframe_;
};

TEST_F(PDFIFrameNavigationThrottleTest, OnlyCreateThrottleForSubframes) {
  // Disable the PDF plugin to test main vs. subframes.
  SetAlwaysOpenPdfExternallyForTests(true);

  // Never create throttle for main frames.
  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), main_rfh());

  handle->CallWillProcessResponseForTesting(
      main_rfh(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)),
      false, net::ProxyServer::Direct());

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_EQ(nullptr, throttle);

  // Create a throttle for subframes.
  handle = content::NavigationHandle::CreateNavigationHandleForTesting(
      GURL(kExampleURL), subframe());

  handle->CallWillProcessResponseForTesting(
      subframe(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)),
      false, net::ProxyServer::Direct());

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_NE(nullptr, throttle);
}

TEST_F(PDFIFrameNavigationThrottleTest, InterceptPDFOnly) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);

  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), subframe());

  // Verify that we CANCEL for PDF mime type.
  std::string header = GetHeaderWithMimeType("application/pdf");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()), false,
      net::ProxyServer::Direct());

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse());

  // Verify that we PROCEED for other mime types.
  // Blank mime type
  handle->CallWillProcessResponseForTesting(
      subframe(), net::HttpUtil::AssembleRawHeaders(kHeader, strlen(kHeader)),
      false, net::ProxyServer::Direct());

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // HTML
  header = GetHeaderWithMimeType("text/html");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()), false,
      net::ProxyServer::Direct());

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());

  // PNG
  header = GetHeaderWithMimeType("image/png");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()), false,
      net::ProxyServer::Direct());

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
}

TEST_F(PDFIFrameNavigationThrottleTest, AllowPDFAttachments) {
  // Setup
  SetAlwaysOpenPdfExternallyForTests(true);

  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), subframe());

  // Verify that we PROCEED for PDF mime types with an attachment
  // content-disposition.
  std::string header =
      "HTTP/1.1 200 OK\r\n"
      "content-type: application/pdf\r\n"
      "content-disposition: attachment\r\n";
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()), false,
      net::ProxyServer::Direct());

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(PDFIFrameNavigationThrottleTest, CancelOnlyIfPDFViewerIsDisabled) {
  // Setup
  std::unique_ptr<content::NavigationHandle> handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(kExampleURL), subframe());

  std::string header = GetHeaderWithMimeType("application/pdf");
  handle->CallWillProcessResponseForTesting(
      subframe(),
      net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()), false,
      net::ProxyServer::Direct());

  // Test PDF Viewer enabled.
  SetAlwaysOpenPdfExternallyForTests(false);

  std::unique_ptr<content::NavigationThrottle> throttle =
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_EQ(nullptr, throttle);

  // Test PDF Viewer disabled.
  SetAlwaysOpenPdfExternallyForTests(true);

  throttle = PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle.get());

  ASSERT_NE(nullptr, throttle);
  ASSERT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillProcessResponse());
}
#endif
