// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/web_payments_observer.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/payments/core/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

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

class WebPaymentsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  ~WebPaymentsObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    observer_ = std::make_unique<WebPaymentsObserver>(web_contents());
  }

  void TearDown() override {
    observer_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void TriggerDidStartNavigation(
      bool is_post,
      bool is_form_submission,
      scoped_refptr<network::ResourceRequestBody> post_data,
      bool provide_navigation_entry = true) {
    testing::NiceMock<content::MockNavigationHandle> handle(web_contents());
    ON_CALL(handle, IsPost()).WillByDefault(testing::Return(is_post));
    handle.set_is_form_submission(is_form_submission);
    handle.set_post_data(post_data);

    observer_->DidStartNavigation(&handle);
  }

  void TriggerDidStartNavigationWithData(const std::string& body_string,
                                         bool is_post = true,
                                         bool is_form_submission = true) {
    scoped_refptr<network::ResourceRequestBody> post_data =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span(body_string));
    TriggerDidStartNavigation(is_post, is_form_submission, post_data);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kThreeDSecureTelemetry};
  std::unique_ptr<WebPaymentsObserver> observer_;
};

TEST_F(WebPaymentsObserverTest, FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(features::kThreeDSecureTelemetry);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test");
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

TEST_F(WebPaymentsObserverTest, NullNavigationHandle) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  observer_->DidStartNavigation(nullptr);
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

TEST_F(WebPaymentsObserverTest, NotPostRequest) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test",
                                    /*is_post=*/false,
                                    /*is_form_submission=*/true);
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

TEST_F(WebPaymentsObserverTest, NotFormSubmission) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=test&cres=test",
                                    /*is_post=*/true,
                                    /*is_form_submission=*/false);
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

TEST_F(WebPaymentsObserverTest, NullPostData) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            /*post_data=*/nullptr,
                            /*provide_navigation_entry=*/true);
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

TEST_F(WebPaymentsObserverTest, EmptyPostDataElements) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto post_data = base::MakeRefCounted<network::ResourceRequestBody>();
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            post_data);
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

TEST_F(WebPaymentsObserverTest, NonBytesPostDataElement) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto post_data = base::MakeRefCounted<network::ResourceRequestBody>();
  post_data->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("/dummy/path")),
                             /*offset=*/0, /*length=*/100,
                             /*expected_modification_time=*/base::Time());
  TriggerDidStartNavigation(/*is_post=*/true, /*is_form_submission=*/true,
                            post_data);
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

TEST_F(WebPaymentsObserverTest, UnrelatedFormData) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("username=foo&password=bar");
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

TEST_F(WebPaymentsObserverTest, ChallengeRequestTelemetry) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TriggerDidStartNavigationWithData("creq=sample_challenge_request");
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

TEST_F(WebPaymentsObserverTest, ChallengeResponseTelemetry_ValidStatuses) {
  const struct {
    const char* status_char;
    ThreeDSecureTransactionStatus expected_status;
  } kTestCases[] = {
      {"Y", ThreeDSecureTransactionStatus::kSuccess},
      {"N", ThreeDSecureTransactionStatus::kDenied},
      {"U", ThreeDSecureTransactionStatus::kCouldNotBePerformed},
      {"A", ThreeDSecureTransactionStatus::kAttemptsProcessingPerformed},
      {"C", ThreeDSecureTransactionStatus::kChallengeRequired},
      {"D", ThreeDSecureTransactionStatus::kChallengeRequiredDecoupled},
      {"R", ThreeDSecureTransactionStatus::kRejected},
      {"I", ThreeDSecureTransactionStatus::kInformationalOnly},
      {"S", ThreeDSecureTransactionStatus::kChallengeUsingSPC},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::string post_data =
        "cres=" + Base64UrlEncodeString(CreateCResJson(test_case.status_char));
    TriggerDidStartNavigationWithData(post_data);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        test_case.expected_status, 1);
    histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        entries[0],
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
            kChallengeResponseName,
        static_cast<int64_t>(test_case.expected_status));
    EXPECT_TRUE(ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
                            kEntryName)
                    .empty());
  }
}

TEST_F(WebPaymentsObserverTest, ChallengeResponseTelemetry_UnknownStatuses) {
  const std::string kInvalidJsons[] = {
      CreateCResJson("X"),          // Unrecognized status character
      CreateCResJson(""),           // Empty status
      CreateCResJson("YY"),         // Multi-character status
      R"({"otherField":"value"})",  // Missing transStatus
      R"({"transStatus":123})",     // Non-string transStatus
      R"({"transStatus":true})",    // Boolean transStatus
      "not_a_valid_json_string",    // Invalid JSON
  };

  for (const auto& json : kInvalidJsons) {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::string post_data = "cres=" + Base64UrlEncodeString(json);
    TriggerDidStartNavigationWithData(post_data);
    histogram_tester.ExpectUniqueSample(kChallengeResponseHistogram,
                                        ThreeDSecureTransactionStatus::kUnknown,
                                        1);
    histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        entries[0],
        ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
            kChallengeResponseName,
        static_cast<int64_t>(ThreeDSecureTransactionStatus::kUnknown));
    EXPECT_TRUE(ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::Payments_ThreeDSecure_ChallengeRequest::
                            kEntryName)
                    .empty());
  }
}

TEST_F(WebPaymentsObserverTest, ChallengeResponseTelemetry_EncryptedJWE) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // RFC 7516 JSON Web Encryption (JWE) compact serialization string:
  // BASE64URL(Protected Header).BASE64URL(Encrypted Key).BASE64URL(IV).
  // BASE64URL(Ciphertext).BASE64URL(Authentication Tag)
  const std::string jwe =
      "eyJhbGciOiJSU0ExXzUiLCJlbmMiOiJBMTI4Q0JDLUhTMjU2In0."
      "UGhIOguFailaqAn0_JHAkWGqqDaioIGPBAEBPqsuTO4TXdzUAvnCxfndoAqudaZWYemmE00D"
      "a_z4GQ0_gevmUhBpKt_xwwAiExoDZWoiK0gyTFsPXWGxCpElSRS8uvPTOG42XuBpLOCAcNc"
      "pq7uLqtuSTFaQO59bVDiGDQu_ly2OOVC38nJLgy1hyGQDxacWm8smLXyhnxxx0IKndihQiA"
      "mRmVMwGMXhz945mlPdkZ87X6li4BOFcT5qP036znVV03cvHiKTiN75qqbYHyKnYkhFlvMQ"
      "J59tATzx87Pn9lPlVOECQeaTdHodTfZuoOH88MgU10PWCf252WVGYFs6UUCBKjg."
      "AxY8DCtDaGlsbGljb3RoZQ."
      "KDlTtXchhZTGufMYmOYGS4HffxPSnvxxo63n2YsTyUQ."
      "7AEfFlyCLHHZmrientbOTw";
  std::string post_data = "cres=" + jwe;
  TriggerDidStartNavigationWithData(post_data);
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

TEST_F(WebPaymentsObserverTest,
       ChallengeResponseTelemetry_InvalidBase64WithoutDots) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::string post_data = "cres=invalid!char#without%dots";
  TriggerDidStartNavigationWithData(post_data);
  histogram_tester.ExpectUniqueSample(
      kChallengeResponseHistogram, ThreeDSecureTransactionStatus::kUnknown, 1);
  histogram_tester.ExpectTotalCount(kChallengeRequestHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0],
      ukm::builders::Payments_ThreeDSecure_ChallengeResponse::
          kChallengeResponseName,
      static_cast<int64_t>(ThreeDSecureTransactionStatus::kUnknown));
  EXPECT_TRUE(
      ukm_recorder
          .GetEntriesByName(
              ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName)
          .empty());
}

TEST_F(WebPaymentsObserverTest,
       ChallengeResponseTelemetry_PercentEncodedPadding) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // {"transStatus":"Y"} Base64Url-encoded with padding is
  // eyJ0cmFuc1N0YXR1cyI6IlkifQ== Standard form submissions encode '=' as '%3D'.
  std::string post_data = "cres=eyJ0cmFuc1N0YXR1cyI6IlkifQ%3D%3D";
  TriggerDidStartNavigationWithData(post_data);
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

class WebPaymentsObserverFuzzTest {
 public:
  void ThreeDSecureTelemetryChallengePostData(
      const std::string& post_data_str) {
    testing::NiceMock<content::MockNavigationHandle> handle;
    ON_CALL(handle, IsPost()).WillByDefault(testing::Return(true));
    handle.set_is_form_submission(true);
    scoped_refptr<network::ResourceRequestBody> post_data =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span(post_data_str));
    handle.set_post_data(post_data);

    observer_.DidStartNavigation(&handle);
  }

  void ThreeDSecureTelemetryChallengeResponseValue(
      const std::string& json_str) {
    testing::NiceMock<content::MockNavigationHandle> handle;
    ON_CALL(handle, IsPost()).WillByDefault(testing::Return(true));
    handle.set_is_form_submission(true);
    scoped_refptr<network::ResourceRequestBody> post_data =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span("cres=" + json_str));
    handle.set_post_data(post_data);

    observer_.DidStartNavigation(&handle);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kThreeDSecureTelemetry};
  WebPaymentsObserver observer_{nullptr};
};

FUZZ_TEST_F(WebPaymentsObserverFuzzTest,
            ThreeDSecureTelemetryChallengePostData);
FUZZ_TEST_F(WebPaymentsObserverFuzzTest,
            ThreeDSecureTelemetryChallengeResponseValue);

}  // namespace payments
