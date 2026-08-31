// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/web_payments_observer.h"

#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/payments/core/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

std::string Base64UrlEncodeString(std::string_view input) {
  std::string encoded;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

std::string CreateCResJson(std::string_view trans_status) {
  return base::StringPrintf(R"({"transStatus":"%s"})", trans_status.data());
}

}  // namespace

class WebPaymentsObserverBrowserTest : public PlatformBrowserTest {
 public:
  WebPaymentsObserverBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kThreeDSecureTelemetry);
  }

  ~WebPaymentsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SubmitForm(std::string_view method,
                  std::string_view name,
                  std::string_view value) {
    ASSERT_TRUE(chrome_test_utils::NavigateToURL(
        GetActiveWebContents(), embedded_test_server()->GetURL("/empty.html")));

    content::TestNavigationObserver nav_observer(GetActiveWebContents());
    ASSERT_TRUE(content::ExecJs(
        GetActiveWebContents(),
        content::JsReplace(
            R"(
              const form = document.createElement('form');
              form.method = $1;
              form.action = $2;
              const input = document.createElement('input');
              input.type = 'hidden';
              input.name = $3;
              input.value = $4;
              form.appendChild(input);
              document.body.appendChild(form);
              form.submit();
            )",
            method, embedded_test_server()->GetURL("/echoall"), name, value)));
    nav_observer.Wait();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest, ChallengeRequest) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SubmitForm("POST", "creq", "sample_challenge_request");

  histogram_tester.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
          kChallengeRequestName,
      true);
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest,
                       ChallengeResponse_Success) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SubmitForm("POST", "cres", Base64UrlEncodeString(CreateCResJson("Y")));

  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram, ThreeDSecureTransactionStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kSuccess));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest,
                       ChallengeResponse_EncryptedJWE) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  const std::string jwe =
      "eyJhbGciOiJSU0ExXzUiLCJlbmMiOiJBMTI4Q0JDLUhTMjU2In0."
      "UGhIOguFailaqAn0_"
      "JHAkWGqqDaioIGPBAEBPqsuTO4TXdzUAvnCxfndoAqudaZWYemmE00D."
      "AxY8DCtDaGlsbGljb3RoZQ."
      "KDlTtXchhZTGufMYmOYGS4HffxPSnvxxo63n2YsTyUQ."
      "7AEfFlyCLHHZmrientbOTw";
  SubmitForm("POST", "cres", jwe);

  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram,
      ThreeDSecureTransactionStatus::kJSONEncrypted, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kJSONEncrypted));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest,
                       MultipleFormFieldsIncludingCRes) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      GetActiveWebContents(), embedded_test_server()->GetURL("/empty.html")));

  content::TestNavigationObserver nav_observer(GetActiveWebContents());
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(),
                              content::JsReplace(
                                  R"(
            const form = document.createElement('form');
            form.method = 'POST';
            form.action = $1;

            const input1 = document.createElement('input');
            input1.type = 'hidden';
            input1.name = 'unrelated';
            input1.value = 'dummy';
            form.appendChild(input1);

            const input2 = document.createElement('input');
            input2.type = 'hidden';
            input2.name = 'cres';
            input2.value = $2;
            form.appendChild(input2);

            document.body.appendChild(form);
            form.submit();
          )",
                                  embedded_test_server()->GetURL("/echoall"),
                                  Base64UrlEncodeString(CreateCResJson("N")))));
  nav_observer.Wait();

  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram, ThreeDSecureTransactionStatus::kDenied, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kDenied));
}

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest,
                       UnrelatedFormSubmission) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SubmitForm("POST", "username", "john_doe");

  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverBrowserTest, GetNavigationIgnored) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SubmitForm("GET", "creq", "sample_challenge_request");

  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

class WebPaymentsObserverFeatureDisabledBrowserTest
    : public WebPaymentsObserverBrowserTest {
 public:
  WebPaymentsObserverFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        features::kThreeDSecureTelemetry);
  }
};

IN_PROC_BROWSER_TEST_F(WebPaymentsObserverFeatureDisabledBrowserTest,
                       NoTelemetryRecorded) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SubmitForm("POST", "creq", "sample_challenge_request");

  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
  EXPECT_TRUE(ukm_recorder
                  .GetEntriesByName(
                      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
                          kEntryName)
                  .empty());
}

}  // namespace payments
