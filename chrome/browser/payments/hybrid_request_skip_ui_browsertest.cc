// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {

class HybridRequestSkipUITest : public PlatformBrowserTest {
 public:
  HybridRequestSkipUITest()
      : gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPaymentRequestSkipToGPay);
  }
  ~HybridRequestSkipUITest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the fake "google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/");
    ASSERT_TRUE(https_server_.Start());

    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://google.com/",
                                 gpay_server_.GetURL("google.com", "/"));
    ServiceWorkerPaymentAppFactory::GetInstance()
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));

    ASSERT_TRUE(
        NavigateTo(https_server_.GetURL("/hybrid_request_skip_ui_test.html")));

    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool NavigateTo(const GURL& url) {
    return content::NavigateToURL(GetActiveWebContents(), url);
  }

  void InstallPaymentApp() {
    ASSERT_TRUE(NavigateTo(gpay_server_.GetURL("google.com", "/install.html")));
    EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "installPaymentApp()"));
  }

 protected:
  net::EmbeddedTestServer gpay_server_;
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, NothingRequested) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
      "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
      "\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(), "buy({apiVersion: 1})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, ShippingRequested_V1) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 1, requestShipping: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, ShippingRequested_V2) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":2},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 2, requestShipping: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, EmailRequested_V1) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
      "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
      "\"paymentrequest@chromium.org\",\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 1, requestEmail: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, EmailRequested_V2) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":2},\"shippingAddress\":null,"
      "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
      "\"paymentrequest@chromium.org\",\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 2, requestEmail: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, NameRequested_V1) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
      "null,\"shippingOption\":null,\"payerName\":\"Browser "
      "Test\",\"payerEmail\":null,\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 1, requestName: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, NameRequested_V2) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":\"BrowserTest\",\"payerEmail\":null,\"payerPhone\":null}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 2, requestName: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, PhoneRequested_V1) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
      "null,\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
      "\"payerPhone\":\"+1 234-567-8900\"}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 1, requestPhone: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, PhoneRequested_V2) {  // stuck
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":\"+1 "
      "234-567-8900\"}",
      content::EvalJs(GetActiveWebContents(),
                      "buy({apiVersion: 2, requestPhone: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, AllRequested_V1) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":{"
      "\"country\":\"CA\",\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"Browser "
      "Test\",\"payerEmail\":\"paymentrequest@chromium.org\",\"payerPhone\":\"+"
      "1 234-567-8900\"}",
      content::EvalJs(
          GetActiveWebContents(),
          "buy({apiVersion: 1, requestShipping: true, requestEmail: "
          "true, requestName: true, requestPhone: true})"));
}

IN_PROC_BROWSER_TEST_F(HybridRequestSkipUITest, AllRequested_V2) {
  EXPECT_EQ(
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":{\"country\":\"CA\",\"addressLine\":["
      "\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"BrowserTest\",\"payerEmail\":\"paymentrequest@chromium."
      "org\",\"payerPhone\":\"+1 234-567-8900\"}",
      content::EvalJs(
          GetActiveWebContents(),
          "buy({apiVersion: 2, requestShipping: true, requestEmail: "
          "true, requestName: true, requestPhone: true})"));
}

}  // namespace payments
