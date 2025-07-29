// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/keep_alive_request_tracker.h"

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using RequestStageType = ChromeKeepAliveRequestTracker::RequestStageType;
using RequestType = ChromeKeepAliveRequestTracker::RequestType;
using testing::IsNull;
using testing::NotNull;

constexpr char kRequestCategoryPrefix[] = "test-prefix";
constexpr char kTestUrl[] = "https://example.com";

std::string GetUrlWithCategory(std::string_view category) {
  return base::StrCat({kTestUrl, "?category=", category});
}

network::URLLoaderCompletionStatus CreateCompletionStatus(
    int error_code,
    int extended_error_code) {
  network::URLLoaderCompletionStatus status{error_code};
  status.extended_error_code = extended_error_code;
  return status;
}

MATCHER_P(IsRequestTrackerCreated,
          expected,
          "ChromeKeepAliveRequestTracker is created") {
  return expected ? arg != nullptr : arg == nullptr;
}

class ChromeKeepAliveRequestTrackerTestBase : public testing::Test {
 public:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;

  ChromeKeepAliveRequestTrackerTestBase() {
    static const FeaturesType enabled_features = {
        {page_load_metrics::features::kBeaconLeakageLogging,
         {{"category_prefix", kRequestCategoryPrefix}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
  ~ChromeKeepAliveRequestTrackerTestBase() override = default;

 protected:
  network::ResourceRequest CreateRequest(std::string_view url) {
    network::ResourceRequest request;

    request.url = GURL(url);
    request.attribution_reporting_eligibility =
        network::mojom::AttributionReportingEligibility::kEmpty;
    request.keepalive = true;
    request.keepalive_token = base::UnguessableToken::Create();
    request.is_fetch_later_api = IsFetchLaterRequest();
    request.attribution_reporting_eligibility =
        IsAttributionReportingEligibleRequest()
            ? network::mojom::AttributionReportingEligibility::kTrigger
            : network::mojom::AttributionReportingEligibility::kUnset;

    return request;
  }

  std::unique_ptr<ChromeKeepAliveRequestTracker> CreateTracker(
      const network::ResourceRequest& request) const {
    return ChromeKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
        request, GetUkmSourceId(),
        /*is_context_detached_callback=*/base::BindRepeating([]() {
          return false;
        }));
  }

  virtual ukm::SourceId GetUkmSourceId() const {
    return ukm::AssignNewSourceId();
  }
  virtual bool IsFetchLaterRequest() const { return false; }
  virtual bool IsAttributionReportingEligibleRequest() const { return false; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A type to support parameterized testing for the category of the request.
struct CategoryTestCase {
  std::string test_case;
  std::string category;
  bool expected;
};

// A type to support parameterized testing for the request type of the request.
struct RequestTypeTestCase {
  std::string test_case;
  RequestType request_type;
};

const RequestTypeTestCase kRequestTypeTestCases[] = {
    {"Fetch", RequestType::kFetch},
    {"Attribution", RequestType::kAttribution},
    {"FetchLater", RequestType::kFetchLater},
};

class MaybeCreateKeepAliveRequestTrackerForCategoryTest
    : public ChromeKeepAliveRequestTrackerTestBase,
      public testing::WithParamInterface<
          std::tuple<CategoryTestCase, RequestTypeTestCase>> {
 protected:
  // ChromeKeepAliveRequestTrackerTestBase overrides:
  bool IsFetchLaterRequest() const override {
    return std::get<1>(GetParam()).request_type == RequestType::kFetchLater;
  }
  bool IsAttributionReportingEligibleRequest() const override {
    return std::get<1>(GetParam()).request_type == RequestType::kAttribution;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MaybeCreateKeepAliveRequestTrackerForCategoryTest,
    testing::Combine(testing::ValuesIn<CategoryTestCase>({
                         {"EmptyCategory", "", false},
                         {"OutOfRangeCategory", "out-of-range-category", false},
                         {"ValidCategory1", "test-prefix1", true},
                         {"ValidCategory2", "test-prefix200", true},
                     }),
                     testing::ValuesIn(kRequestTypeTestCases)),
    [](const testing::TestParamInfo<
        std::tuple<CategoryTestCase, RequestTypeTestCase>>& info) {
      return std::get<0>(info.param).test_case + "_" +
             std::get<1>(info.param).test_case;
    });

TEST_P(MaybeCreateKeepAliveRequestTrackerForCategoryTest, WithCategory) {
  auto url_category = std::get<0>(GetParam()).category;
  auto request = CreateRequest(GetUrlWithCategory(url_category));

  auto tracker = CreateTracker(request);

  EXPECT_THAT(tracker,
              IsRequestTrackerCreated(std::get<0>(GetParam()).expected));
}

class ChromeKeepAliveRequestTrackerTest
    : public ChromeKeepAliveRequestTrackerTestBase,
      public content::KeepAliveRequestUkmMatcher,
      public testing::WithParamInterface<RequestTypeTestCase> {
 protected:
  ChromeKeepAliveRequestTrackerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  void TearDown() override { ukm_recorder_.reset(); }

  void FastForwardBy(const base::TimeDelta& delta) {
    task_environment_.FastForwardBy(delta);
  }

  // A small time step useful for testing the passage of time.
  static base::TimeDelta one_time_step() { return base::Seconds(1) / 15; }

  // KeepAliveRequestUkmMatcher overrides:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }

  // ChromeKeepAliveRequestTrackerTestBase overrides:
  ukm::SourceId GetUkmSourceId() const override {
    return ukm_recorder_->GetNewSourceID();
  }
  bool IsFetchLaterRequest() const override {
    return GetParam().request_type == RequestType::kFetchLater;
  }
  bool IsAttributionReportingEligibleRequest() const override {
    return GetParam().request_type == RequestType::kAttribution;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeKeepAliveRequestTrackerTest,
    testing::ValuesIn(kRequestTypeTestCases),
    [](const testing::TestParamInfo<RequestTypeTestCase>& info) {
      return info.param.test_case;
    });

TEST_P(ChromeKeepAliveRequestTrackerTest, NotLogForNonValidCategoryRequest) {
  auto request = CreateRequest(kTestUrl);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, IsNull());
  }

  ExpectNoUkm();
}

TEST_P(ChromeKeepAliveRequestTrackerTest, LogUkmInDestructor) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix10"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/10,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kLoaderCreated,
                  /*previous_stage=*/std::nullopt, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm({"TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, RequestStarted) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix20"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/20,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kRequestStarted,
                  RequestStageType::kLoaderCreated, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, OneRedirect) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix30"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kFirstRedirectReceived);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/30,
                  /*num_redirects=*/1,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kFirstRedirectReceived,
                  RequestStageType::kRequestStarted, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm({"TimeDelta.RequestStarted",
                                "TimeDelta.FirstRedirectReceived",
                                "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, TwoRedirects) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix40"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kFirstRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kSecondRedirectReceived);
  }

  ExpectCommonUkm(
      GetParam().request_type,
      /*category_id=*/40,
      /*num_redirects=*/2,
      /*num_retries=*/0,
      /*is_context_detached=*/false, RequestStageType::kSecondRedirectReceived,
      RequestStageType::kFirstRedirectReceived, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.FirstRedirectReceived",
       "TimeDelta.SecondRedirectReceived", "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, ThreeRedirects) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix50"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kFirstRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kSecondRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(
        RequestStageType::kThirdOrLaterRedirectReceived);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/50,
                  /*num_redirects=*/3,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kThirdOrLaterRedirectReceived,
                  RequestStageType::kSecondRedirectReceived,
                  *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.FirstRedirectReceived",
       "TimeDelta.SecondRedirectReceived",
       "TimeDelta.ThirdOrLaterRedirectReceived", "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, ResponseReceivedAfterRequestStarted) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix60"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kResponseReceived);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/60,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kResponseReceived,
                  RequestStageType::kRequestStarted, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm({"TimeDelta.RequestStarted",
                                "TimeDelta.ResponseReceived",
                                "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, ResponseReceivedAfterTwoRedirects) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix70"));

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kFirstRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kSecondRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kResponseReceived);
  }

  ExpectCommonUkm(
      GetParam().request_type,
      /*category_id=*/70,
      /*num_redirects=*/2,
      /*num_retries=*/0,
      /*is_context_detached=*/false, RequestStageType::kResponseReceived,
      RequestStageType::kSecondRedirectReceived, *request.keepalive_token);
  ExpectTimeSortedTimeDeltaUkm(

      {"TimeDelta.RequestStarted", "TimeDelta.FirstRedirectReceived",
       "TimeDelta.SecondRedirectReceived", "TimeDelta.ResponseReceived",
       "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, RequestFailedAfterRequestStarted) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix80"));
  auto failed_status =
      CreateCompletionStatus(/*error_code=*/25, /*extended_error_code=*/1);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestFailed,
                                failed_status);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/80,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kRequestFailed,
                  RequestStageType::kRequestStarted, *request.keepalive_token,
                  failed_status.error_code, failed_status.extended_error_code);
  ExpectTimeSortedTimeDeltaUkm({"TimeDelta.RequestStarted",
                                "TimeDelta.RequestFailed",
                                "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, LoaderCompleted) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix90"));
  auto status =
      CreateCompletionStatus(/*error_code=*/net::OK, /*extended_error_code=*/0);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kResponseReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kLoaderCompleted, status);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/90,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kLoaderCompleted,
                  RequestStageType::kResponseReceived, *request.keepalive_token,
                  /*failed_error_code=*/std::nullopt,
                  /*failed_extended_error_code=*/std::nullopt,
                  status.error_code, status.extended_error_code);
  ExpectTimeSortedTimeDeltaUkm(

      {"TimeDelta.RequestStarted", "TimeDelta.ResponseReceived",
       "TimeDelta.LoaderCompleted", "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, LoaderCompletedWithError) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix100"));
  auto failed_status =
      CreateCompletionStatus(/*error_code=*/15, /*extended_error_code=*/5);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kLoaderCompleted,
                                failed_status);
  }

  ExpectCommonUkm(GetParam().request_type,
                  /*category_id=*/100,
                  /*num_redirects=*/0,
                  /*num_retries=*/0,
                  /*is_context_detached=*/false,
                  RequestStageType::kLoaderCompleted,
                  RequestStageType::kRequestStarted, *request.keepalive_token,
                  /*failed_error_code=*/std::nullopt,
                  /*failed_extended_error_code=*/std::nullopt,
                  failed_status.error_code, failed_status.extended_error_code);
  ExpectTimeSortedTimeDeltaUkm(

      {"TimeDelta.RequestStarted", "TimeDelta.LoaderCompleted",
       "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, RequestRetriedAfterRequestFailed) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix110"));
  auto failed_status = CreateCompletionStatus(/*error_code=*/net::ERR_TIMED_OUT,
                                              /*extended_error_code=*/1);
  auto retried_status =
      CreateCompletionStatus(/*error_code=*/net::ERR_TIMED_OUT,
                             /*extended_error_code=*/1);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestFailed,
                                failed_status);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestRetried,
                                retried_status);
  }

  ExpectCommonUkm(
      GetParam().request_type,
      /*category_id=*/110,
      /*num_redirects=*/0,
      /*num_retries=*/1,
      /*is_context_detached=*/false, RequestStageType::kRequestRetried,
      RequestStageType::kRequestFailed, *request.keepalive_token,
      /*failed_error_code=*/failed_status.error_code,
      /*failed_extended_error_code=*/failed_status.extended_error_code,
      /*completed_error_code=*/std::nullopt,
      /*completed_extended_error_code=*/std::nullopt,
      /*retried_error_code=*/retried_status.error_code,
      /*retried_extended_error_code=*/retried_status.extended_error_code);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.RequestFailed",
       "TimeDelta.RequestRetried", "TimeDelta.EventLogged"});
}

TEST_P(ChromeKeepAliveRequestTrackerTest, RequestRetriedAfterTwoRedirects) {
  auto request = CreateRequest(GetUrlWithCategory("test-prefix120"));
  auto failed_status = CreateCompletionStatus(/*error_code=*/net::ERR_FAILED,
                                              /*extended_error_code=*/2);
  auto retried_status = CreateCompletionStatus(/*error_code=*/net::ERR_FAILED,
                                               /*extended_error_code=*/2);

  {
    auto tracker = CreateTracker(request);
    ASSERT_THAT(tracker, NotNull());
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestStarted);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kFirstRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kSecondRedirectReceived);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestFailed,
                                failed_status);
    FastForwardBy(one_time_step());
    tracker->AdvanceToNextStage(RequestStageType::kRequestRetried,
                                retried_status);
  }

  ExpectCommonUkm(
      GetParam().request_type,
      /*category_id=*/120,
      /*num_redirects=*/2,
      /*num_retries=*/1,
      /*is_context_detached=*/false, RequestStageType::kRequestRetried,
      RequestStageType::kRequestFailed, *request.keepalive_token,
      /*failed_error_code=*/failed_status.error_code,
      /*failed_extended_error_code=*/failed_status.extended_error_code,
      /*completed_error_code=*/std::nullopt,
      /*completed_extended_error_code=*/std::nullopt,
      /*retried_error_code=*/retried_status.error_code,
      /*retried_extended_error_code=*/retried_status.extended_error_code);
  ExpectTimeSortedTimeDeltaUkm(
      {"TimeDelta.RequestStarted", "TimeDelta.FirstRedirectReceived",
       "TimeDelta.SecondRedirectReceived", "TimeDelta.RequestFailed",
       "TimeDelta.RequestRetried", "TimeDelta.EventLogged"});
}

}  // namespace
