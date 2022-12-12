// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"

namespace payments {

class EmptyParametersTest : public PaymentRequestPlatformBrowserTestBase {
 protected:
  EmptyParametersTest()
      : kylepay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    kylepay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.test/");
    ASSERT_TRUE(kylepay_server_.Start());

    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

  void SetDownloaderAndIgnorePortInOriginComparisonForTesting() {
    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        GetCSPCheckerForTests(), context->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://kylepay.test/",
                                 kylepay_server_.GetURL("kylepay.test", "/"));
    ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        GetActiveWebContents()->GetPrimaryMainFrame())
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));
  }

 private:
  net::EmbeddedTestServer kylepay_server_;
};

namespace {

IN_PROC_BROWSER_TEST_F(EmptyParametersTest, NoCrash) {
  NavigateTo("/empty_parameters_test.html");
  SetDownloaderAndIgnorePortInOriginComparisonForTesting();
  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "runTest()"));
}

}  // namespace
}  // namespace payments
