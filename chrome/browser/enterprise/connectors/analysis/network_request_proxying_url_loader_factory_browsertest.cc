// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/network_request_proxying_url_loader_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/pending_binary_upload_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr std::string_view kRequestBody = "sensitive=data";

constexpr char kNetworkRequestPolicy[] = R"({
  "audit": {
    "tab_domain": ["foo.com"],
    "request_domain": ["bar.org"]
  },
  "tags": ["dlp"]
})";

// Submits a form whose body is `kRequestBody` to $1.
constexpr char kSubmitFormScript[] = R"(
  const form = document.createElement('form');
  form.method = 'POST';
  form.action = $1;
  const input = document.createElement('input');
  input.name = 'sensitive';
  input.value = 'data';
  form.appendChild(input);
  document.body.appendChild(form);
  form.submit();
)";

// Adds an iframe loading $1 and waits for it to load.
constexpr char kAddIframeScript[] = R"(
  new Promise(resolve => {
    const iframe = document.createElement('iframe');
    iframe.onload = () => resolve(true);
    iframe.src = $1;
    document.body.appendChild(iframe);
  });
)";

class NetworkRequestProxyingURLLoaderFactoryBrowserTest
    : public InProcessBrowserTest {
 public:
  NetworkRequestProxyingURLLoaderFactoryBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableAuditOnlyNetworkRequestConnector);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    safe_browsing::CloudBinaryUploadServiceFactory::GetInstance()
        ->SetTestingFactory(
            browser()->GetProfile(),
            base::BindRepeating(&test::PendingBinaryUploadService::Create));
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));

    // The policy is set before any page used by tests is loaded since it only
    // applies to URLLoaderFactory instances created after it's set.
    test::SetAnalysisConnector(browser()->GetProfile()->GetPrefs(),
                               AnalysisConnector::NETWORK_REQUEST,
                               kNetworkRequestPolicy);
  }

  void TearDownOnMainThread() override {
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  test::PendingBinaryUploadService* upload_service() {
    return test::PendingBinaryUploadService::GetForProfile(
        browser()->GetProfile());
  }

  GURL TabUrl() {
    return embedded_test_server()->GetURL("foo.com", "/title1.html");
  }

  GURL RequestUrl() {
    return embedded_test_server()->GetURL("bar.org", "/echo");
  }

  // Sends a request from `frame` and waits for its response. "no-cors" is used
  // since the test server doesn't allow cross-origin requests, which doesn't
  // prevent requests from being sent.
  void SendRequest(const content::ToRenderFrameHost& frame,
                   std::string_view method,
                   const GURL& url) {
    std::string script =
        method == "GET"
            ? content::JsReplace(
                  "fetch($1, {mode: 'no-cors'}).then(() => true)", url)
            : content::JsReplace(
                  "fetch($1, {method: $2, mode: 'no-cors', body: $3})"
                  ".then(() => true)",
                  url, method, kRequestBody);
    EXPECT_EQ(true, content::EvalJs(frame, script));
  }

  // Checks that a single scan was started for a request to `request_url` made
  // from a tab at `tab_url`, then completes it.
  void ExpectSingleScanAndComplete(const GURL& request_url,
                                   const GURL& tab_url) {
    base::test::TestFuture<RequestHandlerResult> future;
    auto scan_completed_callback = NetworkRequestProxyingURLLoaderFactory::
        SetScanCompletedCallbackForTesting(
            future.GetRepeatingCallback<const RequestHandlerResult&>());

    ASSERT_EQ(1u, upload_service()->requests().size());
    std::unique_ptr<BinaryUploadRequest> request =
        std::move(upload_service()->requests()[0]);
    upload_service()->requests().clear();

    const ContentAnalysisRequest& analysis_request =
        request->content_analysis_request();
    EXPECT_EQ(AnalysisConnector::NETWORK_REQUEST,
              analysis_request.analysis_connector());
    EXPECT_EQ(kDmToken, analysis_request.device_token());
    EXPECT_FALSE(analysis_request.blocking());
    ASSERT_EQ(1, analysis_request.tags_size());
    EXPECT_EQ("dlp", analysis_request.tags(0));
    EXPECT_EQ(request_url.spec(), analysis_request.request_data().url());
    EXPECT_EQ(tab_url.spec(), analysis_request.request_data().tab_url());
    EXPECT_EQ(request_url.spec(),
              analysis_request.request_data().destination());

    base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
        data_future;
    request->GetRequestData(data_future.GetCallback());
    EXPECT_EQ(ScanRequestUploadResult::kSuccess,
              data_future.Get<ScanRequestUploadResult>());
    EXPECT_EQ(kRequestBody.size(),
              data_future.Get<BinaryUploadRequest::Data>().size);

    request->FinishRequest(ScanRequestUploadResult::kSuccess,
                           ContentAnalysisResponse());
    RequestHandlerResult result = future.Take();
    EXPECT_TRUE(result.complies);
    EXPECT_EQ(FinalContentAnalysisResult::SUCCESS, result.final_result);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NetworkRequestProxyingURLLoaderFactoryBrowserTest,
                       ScansFetchPostRequest) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), TabUrl()));

  SendRequest(web_contents(), "POST", RequestUrl());

  ExpectSingleScanAndComplete(RequestUrl(), TabUrl());
}

IN_PROC_BROWSER_TEST_F(NetworkRequestProxyingURLLoaderFactoryBrowserTest,
                       ScansFormSubmission) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), TabUrl()));

  content::TestNavigationObserver observer(web_contents());
  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace(kSubmitFormScript, RequestUrl())));
  observer.Wait();

  // The navigation isn't affected by the scan. The "/echo" handler responds
  // with the body of the request.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(RequestUrl(), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(std::string(kRequestBody),
            content::EvalJs(web_contents(), "document.body.textContent"));

  ExpectSingleScanAndComplete(RequestUrl(), TabUrl());
}

// The policy applies based on the URL of the tab, so requests from child
// frames are scanned even if the child frame's URL doesn't match the policy.
IN_PROC_BROWSER_TEST_F(NetworkRequestProxyingURLLoaderFactoryBrowserTest,
                       ScansRequestFromChildFrame) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), TabUrl()));
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace(kAddIframeScript, embedded_test_server()->GetURL(
                                               "baz.net", "/title2.html"))));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents(), 0);
  ASSERT_TRUE(child_frame);

  SendRequest(child_frame, "POST", RequestUrl());

  ExpectSingleScanAndComplete(RequestUrl(), TabUrl());
}

IN_PROC_BROWSER_TEST_F(NetworkRequestProxyingURLLoaderFactoryBrowserTest,
                       DoesNotScanOtherRequests) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), TabUrl()));

  // Only POST requests are scanned.
  SendRequest(web_contents(), "GET", RequestUrl());

  // The request URL doesn't match the policy.
  SendRequest(web_contents(), "POST",
              embedded_test_server()->GetURL("baz.net", "/echo"));

  // Scans start synchronously when requests go through the proxy, so they
  // would have started by the time responses are received.
  EXPECT_TRUE(upload_service()->requests().empty());
}

IN_PROC_BROWSER_TEST_F(NetworkRequestProxyingURLLoaderFactoryBrowserTest,
                       DoesNotScanNonMatchingTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("other.com", "/title1.html")));

  SendRequest(web_contents(), "POST", RequestUrl());

  EXPECT_TRUE(upload_service()->requests().empty());
}

}  // namespace enterprise_connectors
