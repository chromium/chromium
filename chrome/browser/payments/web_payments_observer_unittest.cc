// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/web_payments_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/payments/core/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

constexpr char kChallengeRequestHistogram[] =
    "Payments.ThreeDSecure.ChallengeRequest";
constexpr char kChallengeResponseHistogram[] =
    "Payments.ThreeDSecure.ChallengeResponse";

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

  // Returns the UKM source ID of the next page of the navigation.
  ukm::SourceId TriggerDidStartNavigation(
      bool is_post,
      bool is_form_submission,
      scoped_refptr<network::ResourceRequestBody> post_data,
      bool provide_navigation_entry = true) {
    testing::NiceMock<content::MockNavigationHandle> handle(web_contents());
    ON_CALL(handle, IsPost()).WillByDefault(testing::Return(is_post));
    handle.set_is_form_submission(is_form_submission);
    handle.set_post_data(post_data);

    observer_->DidStartNavigation(&handle);
    return handle.GetNextPageUkmSourceId();
  }

  // Returns the UKM source ID of the next page of the navigation.
  ukm::SourceId TriggerDidStartNavigationWithData(
      const std::string& body_string,
      bool is_post = true,
      bool is_form_submission = true) {
    scoped_refptr<network::ResourceRequestBody> post_data =
        network::ResourceRequestBody::CreateFromCopyOfBytes(
            base::as_byte_span(body_string));
    return TriggerDidStartNavigation(is_post, is_form_submission, post_data);
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

TEST_F(WebPaymentsObserverTest, RecordsTelemetryFor3DSFormSubmission) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId next_page_source_id =
      TriggerDidStartNavigationWithData("creq=sample_challenge_request");
  histogram_tester.ExpectUniqueSample(kChallengeRequestHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kChallengeResponseHistogram, 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Payments_ThreeDSecure_ChallengeRequest::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, next_page_source_id);
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

}  // namespace payments
