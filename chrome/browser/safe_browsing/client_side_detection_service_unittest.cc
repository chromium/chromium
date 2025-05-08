// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/features/scam_detection.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::proto::ScamDetectionResponse;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace safe_browsing {

class ClientSidePhishingModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_FALSE(model_observer_);
      model_observer_ = observer;
    }
  }

  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_EQ(observer, model_observer_);
      model_observer_ = nullptr;
    }
  }

  // Notifies the model validation observer about the model file update.
  void NotifyModelFileUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_files_path) {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path)
                                .SetAdditionalFiles(additional_files_path)
                                .Build();
      model_observer_->OnModelUpdated(optimization_target, *model_metadata);
    }
  }

 private:
  // The observer that is registered to receive model validation optimzation
  // target events.
  raw_ptr<optimization_guide::OptimizationTargetModelObserver> model_observer_;
};

class ClientSideDetectionServiceTest
    : public testing::Test,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  ClientSideDetectionServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kSafeBrowsingRemoveCookiesInAuthRequests, {}}};
    if (ShouldEnableESBDailyPhishingLimit()) {
      base::FieldTrialParams params;
      params["kMaxReportsPerIntervalESB"] = "10";
      enabled_features.push_back(
          {kSafeBrowsingDailyPhishingReportsLimit, params});
    }
    if (ShouldEnableClientSideDetectionBrandAndPageIntent()) {
      enabled_features.push_back(
          {kClientSideDetectionBrandAndIntentForScamDetection, {}});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  bool ShouldEnableESBDailyPhishingLimit() { return get<0>(GetParam()); }
  bool ShouldEnableClientSideDetectionBrandAndPageIntent() {
    return get<1>(GetParam());
  }

 protected:
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    model_observer_tracker_ =
        std::make_unique<ClientSidePhishingModelObserverTracker>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    csd_service_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
  }

  void ValidateModel(
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_file_path) {
    model_observer_tracker_->NotifyModelFileUpdate(
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
        model_file_path, additional_file_path);
    task_environment_.RunUntilIdle();
  }

  void ReadModelAndTfLiteFiles() {
    base::FilePath model_file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
    model_file_path = model_file_path.AppendASCII("components")
                          .AppendASCII("test")
                          .AppendASCII("data")
                          .AppendASCII("safe_browsing")
                          .AppendASCII("client_model.pb");

    base::FilePath additional_files_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                           &additional_files_path);
    additional_files_path = additional_files_path.AppendASCII("components")
                                .AppendASCII("test")
                                .AppendASCII("data")
                                .AppendASCII("safe_browsing");

#if BUILDFLAG(IS_ANDROID)
    additional_files_path =
        additional_files_path.AppendASCII("visual_model_android.tflite");
#else
    additional_files_path =
        additional_files_path.AppendASCII("visual_model_desktop.tflite");
#endif
    ValidateModel(model_file_path, {additional_files_path});
  }

  bool SendClientReportPhishingRequest(const GURL& phishing_url,
                                       float score,
                                       const std::string& access_token) {
    std::unique_ptr<ClientPhishingRequest> request =
        std::make_unique<ClientPhishingRequest>(ClientPhishingRequest());
    request->set_url(phishing_url.spec());
    request->set_client_score(score);
    request->set_is_phishing(true);  // client thinks the URL is phishing.

    base::RunLoop run_loop;
    csd_service_->SendClientReportPhishingRequest(
        std::move(request),
        base::BindOnce(&ClientSideDetectionServiceTest::SendRequestDone,
                       base::Unretained(this), run_loop.QuitWhenIdleClosure()),
        access_token);
    phishing_url_ = phishing_url;
    run_loop.Run();  // Waits until callback is called.
    return is_phishing_;
  }

  void SetResponse(const GURL& url,
                   const std::string& response_data,
                   int net_error) {
    if (net_error != net::OK) {
      test_url_loader_factory_.AddResponse(
          url, network::mojom::URLResponseHead::New(), std::string(),
          network::URLLoaderCompletionStatus(net_error));
      return;
    }
    test_url_loader_factory_.AddResponse(url.spec(), response_data);
  }

  void SetClientReportPhishingResponse(const std::string& response_data,
                                       int net_error) {
    SetResponse(ClientSideDetectionService::GetClientReportUrl(
                    ClientSideDetectionService::kClientReportPhishingUrl),
                response_data, net_error);
  }

  bool AtPhishingReportLimit() { return csd_service_->AtPhishingReportLimit(); }

  std::deque<base::Time>& GetPhishingReportTimes() {
    return csd_service_->phishing_report_times_;
  }

  void TestCache() {
    auto& cache = csd_service_->cache_;
    EXPECT_TRUE(cache.find(GURL("http://first.url.com/")) == cache.end());

    base::Time now = base::Time::Now();
    base::Time time =
        now -
        base::Days(ClientSideDetectionService::kNegativeCacheIntervalDays) +
        base::Minutes(5);
    cache[GURL("http://first.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(false, time);

    time = now -
           base::Days(ClientSideDetectionService::kNegativeCacheIntervalDays) -
           base::Hours(1);
    cache[GURL("http://second.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(false, time);

    time = now -
           base::Minutes(
               ClientSideDetectionService::kPositiveCacheIntervalMinutes) -
           base::Minutes(5);
    cache[GURL("http://third.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(true, time);

    time = now -
           base::Minutes(
               ClientSideDetectionService::kPositiveCacheIntervalMinutes) +
           base::Minutes(5);
    cache[GURL("http://fourth.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(true, time);

    csd_service_->UpdateCache();

    // 3 elements should be in the cache, the first, third, and fourth.
    EXPECT_EQ(3U, cache.size());
    EXPECT_TRUE(cache.find(GURL("http://first.url.com/")) != cache.end());
    EXPECT_TRUE(cache.find(GURL("http://third.url.com/")) != cache.end());
    EXPECT_TRUE(cache.find(GURL("http://fourth.url.com/")) != cache.end());

    // While 3 elements remain, only the first and the fourth are actually
    // valid.
    bool is_phishing;
    EXPECT_TRUE(csd_service_->GetValidCachedResult(GURL("http://first.url.com"),
                                                   &is_phishing));
    EXPECT_FALSE(is_phishing);
    EXPECT_FALSE(csd_service_->GetValidCachedResult(
        GURL("http://third.url.com"), &is_phishing));
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://fourth.url.com"), &is_phishing));
    EXPECT_TRUE(is_phishing);
  }

  void SetupMockOptimizationGuideKeyedService() {
    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile_,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<testing::NiceMock<
                          MockOptimizationGuideKeyedService>>();
                    })));
  }

  optimization_guide::StreamingResponse CreateScamDetectionResponse(
      const std::string& brand,
      const std::string& intent,
      bool is_complete) {
    ScamDetectionResponse response;
    response.set_brand(brand);
    response.set_intent(intent);
    return optimization_guide::StreamingResponse{
        .response = AnyWrapProto(response), .is_complete = is_complete};
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ClientSideDetectionService> csd_service_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  testing::NiceMock<MockSession> session_;

 private:
  void SendRequestDone(
      base::OnceClosure continuation_callback,
      GURL phishing_url,
      bool is_phishing,
      std::optional<net::HttpStatusCode> response_code,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    std::move(continuation_callback).Run();
  }

  std::unique_ptr<base::FieldTrialList> field_trials_;

  GURL phishing_url_;
  bool is_phishing_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSideDetectionServiceTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ClientSideDetectionServiceTest, ServiceObjectDeletedBeforeCallbackDone) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_NE(csd_service_.get(), nullptr);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  csd_service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  base::RunLoop().RunUntilIdle();
}

TEST_P(ClientSideDetectionServiceTest, SendClientReportPhishingRequest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token;

  // Safe browsing is not enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  base::Time before = base::Time::Now();

  // Invalid response body from the server, but we will still track it as a
  // ping count.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  SetClientReportPhishingResponse("invalid proto response", net::OK);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);

  // Normal behavior with no access token.
  histogram_tester = std::make_unique<base::HistogramTester>();
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);

  // This request will fail, but not because of the cap, but because the network
  // failed, but we will still log the number of pings sent.
  histogram_tester = std::make_unique<base::HistogramTester>();
  EXPECT_FALSE(AtPhishingReportLimit());
  GURL second_url("http://b.com/");
  response.set_phishy(false);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  net::ERR_FAILED);
  EXPECT_FALSE(
      SendClientReportPhishingRequest(second_url, score, access_token));
  histogram_tester->ExpectUniqueSample(
      /*name=*/"SBClientPhishing.NetworkResult2",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);

  // We have sent 3 pings so far, which is the cap.
  EXPECT_TRUE(AtPhishingReportLimit());

  GURL third_url("http://c.com/");
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);

  // Although this is a normal behavior, we are capped in the number of pings,
  // so this will expect false.
  EXPECT_FALSE(SendClientReportPhishingRequest(third_url, score, access_token));

  base::Time after = base::Time::Now();

  // Check that we have recorded 3 requests within the correct time range. The
  // third_url is not recorded because the send was attempted while we are at
  // the limit.
  std::deque<base::Time>& report_times = GetPhishingReportTimes();
  EXPECT_EQ(3U, report_times.size());
  EXPECT_TRUE(AtPhishingReportLimit());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop_back();
    EXPECT_LE(before, time);
    EXPECT_GE(after, time);
  }

  // Only the first url should be in the cache.
  bool is_phishing;
  EXPECT_TRUE(csd_service_->GetValidCachedResult(url, &is_phishing));
  EXPECT_TRUE(is_phishing);
  bool is_second_url_phishing = false;
  EXPECT_FALSE(
      csd_service_->GetValidCachedResult(second_url, &is_second_url_phishing));
  EXPECT_FALSE(is_second_url_phishing);
  bool is_third_url_phishing = false;
  EXPECT_FALSE(
      csd_service_->GetValidCachedResult(third_url, &is_third_url_phishing));
  EXPECT_FALSE(is_third_url_phishing);
}

TEST_P(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithToken) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "fake access token";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional("Bearer " + access_token));
        // Cookies should be removed when token is set.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_P(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithoutToken) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            std::nullopt);
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_P(ClientSideDetectionServiceTest, GetNumReportTest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now));

  EXPECT_EQ(2, csd_service_->GetPhishingNumReports());
  EXPECT_FALSE(AtPhishingReportLimit());

  EXPECT_TRUE(csd_service_->AddPhishingReport(now));
  EXPECT_EQ(3, csd_service_->GetPhishingNumReports());
  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceTest,
       GetNumReportTestWhenPrefsPreloadedAndOverLimit) {
  // The current report limit is 3 as per
  // ClientSideDetectionService::kMaxReportsPerInterval.
  base::Value::List time_list;
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));

  profile_->GetPrefs()->SetList(prefs::kSafeBrowsingCsdPingTimestamps,
                                std::move(time_list));

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceTest,
       GetNumReportTestWhenPrefsPreloadedNotOverLimit) {
  // The current report limit is 3 as per
  // ClientSideDetectionService::kMaxReportsPerInterval.
  base::Value::List time_list;
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));
  time_list.Append(base::Value(base::Time::Now().InSecondsFSinceUnixEpoch()));

  profile_->GetPrefs()->SetList(prefs::kSafeBrowsingCsdPingTimestamps,
                                std::move(time_list));

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  EXPECT_FALSE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceTest, GetNumReportTestESB) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now - twenty_five_hours));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now));
  EXPECT_TRUE(csd_service_->AddPhishingReport(now));

  EXPECT_EQ(2, csd_service_->GetPhishingNumReports());
  // We have not quite hit the limit for both ESB and SSB users.
  EXPECT_FALSE(AtPhishingReportLimit());

  // Adding one more will hit the limit just for SSB users.
  EXPECT_TRUE(csd_service_->AddPhishingReport(now));

  EXPECT_EQ(3, csd_service_->GetPhishingNumReports());
  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit)) {
    EXPECT_FALSE(AtPhishingReportLimit());
  } else {
    EXPECT_TRUE(AtPhishingReportLimit());
  }

  // Adding 7 more to 10 reports total will hit the limit for ESB users as the
  // limit is predefined in this class.

  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit)) {
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
    EXPECT_TRUE(csd_service_->AddPhishingReport(now));
  } else {
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
    EXPECT_FALSE(csd_service_->AddPhishingReport(now));
  }

  EXPECT_TRUE(AtPhishingReportLimit());
}

TEST_P(ClientSideDetectionServiceTest, CacheTest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());
  ReadModelAndTfLiteFiles();

  TestCache();
}

TEST_P(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("10.1.2.3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("172.24.3.4"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.1.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fc00::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("fec0:1:2::3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:192.168.1.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("200.1.1.1"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("2001:0db8:ac10:fe01::"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("::ffff:23c5:281b"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress(address));
}

TEST_P(ClientSideDetectionServiceTest, IsLocalResource) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  net::IPAddress address;
  EXPECT_TRUE(csd_service_->IsLocalResource(address));

  // Create an IP address of invalid length
  uint8_t addr[5] = {0xFE, 0xDC, 0xBA, 0x98};
  address = net::IPAddress(addr);
  EXPECT_TRUE(csd_service_->IsLocalResource(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsLocalResource(address));
}

TEST_P(ClientSideDetectionServiceTest, TestModelFollowsPrefs) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  // Safe Browsing is not enabled.
  EXPECT_FALSE(csd_service_->enabled());

  // Safe Browsing is enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_TRUE(csd_service_->enabled());
}

TEST_P(ClientSideDetectionServiceTest,
       TestReceivingImageEmbedderUpdatesAfterResubscription) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  EXPECT_TRUE(csd_service_->IsSubscribedToImageEmbeddingModelUpdates());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(csd_service_->IsSubscribedToImageEmbeddingModelUpdates());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(csd_service_->IsSubscribedToImageEmbeddingModelUpdates());
}

TEST_P(ClientSideDetectionServiceTest, TestOnDeviceModelFetchSuccessCall) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // When the Enhanced Safe Browsing protection setting is enabled, the next to
  // functions will call from the delegate.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. We will then test
  // for all the possible waitable reasons, which should also not stop
  // observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kSafetyModelNotAvailable);

  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kLanguageDetectionModelNotAvailable);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);
}

TEST_P(ClientSideDetectionServiceTest, TestOnDeviceModelFetchFailureCall) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;

  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 0);

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  // Now that the delegate is observing, send `kTooManyRecentCrashes`
  // to the observer, which is not a waitable reason.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kTooManyRecentCrashes);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", false, 1);
}

TEST_P(ClientSideDetectionServiceTest, TestSessionCreationFailure) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // When the Enhanced Safe Browsing protection setting is enabled, the next to
  // functions will call from the delegate.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", false, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 0);
}

TEST_P(ClientSideDetectionServiceTest, TestSessionCreationSuccess) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.OnDeviceModelSessionAliveOnNewRequest", false, 1);
}

TEST_P(ClientSideDetectionServiceTest,
       TestSessionCreationSuccessWithAPreviousAliveSession) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);

  // We will expect a second time, but since the InquireOnDeviceModel function
  // wasn't finished and the future callback wasn't completed, we will remove
  // the "old" session and recreate a new one.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future2;
  csd_service_->InquireOnDeviceModel("", future2.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 2);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 2);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.OnDeviceModelSessionAliveOnNewRequest", false, 1);
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.OnDeviceModelSessionAliveOnNewRequest", true, 1);
}

TEST_P(ClientSideDetectionServiceTest, TestSessionExecutionFailure) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::unexpected(
                    OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            OptimizationGuideModelExecutionError::
                                ModelExecutionError::kGenericFailure)),
                /*provided_by_on_device=*/true, nullptr));
          })));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", false, 1);
}

TEST_P(ClientSideDetectionServiceTest,
       TestSessionExecutionSuccessButNotComplete) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/false)),
                /*provided_by_on_device=*/false));
          })));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);

  // Because the execution result isn't complete yet, we do not intend on
  // tracking the duration or success since we're still waiting. For the purpose
  // of the test, we do not complete the execution result to make sure that
  // they're not logged.
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", 0);
}

TEST_P(ClientSideDetectionServiceTest,
       TestSessionExecutionSuccessButFailedParsing) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // When the Enhanced Safe Browsing protection setting is enabled, the next to
  // functions will call from the delegate.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  optimization_guide::proto::DefaultResponse default_response;
  optimization_guide::StreamingResponse default_streaming_response{
      .response = AnyWrapProto(default_response), .is_complete = true};

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(default_streaming_response),
                /*provided_by_on_device=*/true));
          })));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", false, 1);
}

TEST_P(ClientSideDetectionServiceTest,
       TestSessionExecutionAndResponseParseSuccess) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // When the Enhanced Safe Browsing protection setting is enabled, the next to
  // functions will call from the delegate.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/true)),
                /*provided_by_on_device=*/false));
          })));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  base::test::TestFuture<std::optional<ScamDetectionResponse>> future;
  csd_service_->InquireOnDeviceModel("", future.GetCallback());

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSuccessfulResponseCallbackAlive", true, 1);
}

TEST_P(ClientSideDetectionServiceTest,
       TestExecutionSuccessButCallbackIsNotAlive) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // When the Enhanced Safe Browsing protection setting is enabled, the next to
  // functions will call from the delegate.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) { return nullptr; });

  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // And then send `kSuccess` to the observer, which will log the histogram.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 1);
  histogram_tester.ExpectTotalCount("SBClientPhishing.OnDeviceModelFetchTime",
                                    1);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            return std::make_unique<NiceMock<MockSession>>(&session_);
          }));

  EXPECT_CALL(session_, ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            callback.Run(OptimizationGuideModelStreamingExecutionResult(
                base::ok(CreateScamDetectionResponse("Google", "Search Engine",
                                                     /*is_complete=*/true)),
                /*provided_by_on_device=*/false));
          })));

  csd_service_->SetOnDeviceAvailabilityForTesting(true);

  // Create an empty callback.
  base::OnceCallback<void(std::optional<ScamDetectionResponse>)> host_callback;
  csd_service_->InquireOnDeviceModel("", std::move(host_callback));

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSessionCreationSuccess", true, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelSessionCreationTime", 1);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.OnDeviceModelExecutionDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelExecutionSuccess", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelResponseParseSuccess", true, 1);
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelSuccessfulResponseCallbackAlive", false,
      1);
}

TEST_P(ClientSideDetectionServiceTest,
       TestModelEligibilityReasonCheckAtFailedInquiry) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionBrandAndIntentForScamDetection)) {
    return;
  }

  base::HistogramTester histogram_tester;

  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_),
      model_observer_tracker_.get());

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  SetupMockOptimizationGuideKeyedService();

  // The below function is called by the delegate when calling
  // InquireOnDeviceModel but the on device model is not available yet.
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibility(_))
      .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature) {
        return optimization_guide::OnDeviceModelEligibilityReason::
            kModelToBeInstalled;
      });

  // We will start listening to the on device model when we enable ESB, so we
  // expect the call below.
  optimization_guide::OnDeviceModelAvailabilityObserver* availability_observer =
      nullptr;
  base::RunLoop run_loop_for_add_observer;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              AddOnDeviceModelAvailabilityChangeObserver(_, _))
      .WillOnce(testing::Invoke(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              optimization_guide::OnDeviceModelAvailabilityObserver* observer) {
            availability_observer = observer;
            run_loop_for_add_observer.Quit();
          }));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  run_loop_for_add_observer.Run();
  CHECK(availability_observer);

  // Now that the delegate is observing, send `kConfigNotAvailableForFeature`
  // first to the observer, which will not stop the observing. However, for the
  // purpose of this test, we will never fulfill the request to notify the
  // service class that the model installation is successful.
  availability_observer->OnDeviceModelAvailabilityChanged(
      optimization_guide::ModelBasedCapabilityKey::kScamDetection,
      optimization_guide::OnDeviceModelEligibilityReason::
          kConfigNotAvailableForFeature);

  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelDownloadSuccess", true, 0);

  // Although the default is set to false, we manually set to false in the
  // service class.
  csd_service_->SetOnDeviceAvailabilityForTesting(false);

  csd_service_->LogOnDeviceModelEligibilityReason();

  // We expect the histogram value for
  // SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure to be
  // kModelTobeInstalled as we set the EXPECT_CALL above when calling for
  // function GetOnDeviceModelEligibility within the optimization guide service,
  // which is called in the service delegate.
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.OnDeviceModelEligibilityReasonAtInquiryFailure",
      optimization_guide::OnDeviceModelEligibilityReason::kModelToBeInstalled,
      1);
}

}  // namespace safe_browsing
