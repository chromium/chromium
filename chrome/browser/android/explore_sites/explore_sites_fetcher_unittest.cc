// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"

#include <map>

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace {
const char kAcceptLanguages[] = "en-US,en;q=0.5";
const char kTestData[] = "Any data.";
}  // namespace

namespace explore_sites {

class TestingDeviceDelegate : public ExploreSitesFetcher::DeviceDelegate {
 public:
  TestingDeviceDelegate() = default;
  float GetScaleFactorFromDevice() override { return 1.5; }
};

// TODO(freedjm): Add tests for the headers.
class ExploreSitesFetcherTest : public testing::Test {
 public:
  ExploreSitesFetcherTest();

  void SetUp() override;

  std::unique_ptr<ExploreSitesFetcher> CreateFetcher(bool disable_retry,
                                                     bool is_immediate_fetch);
  ExploreSitesRequestStatus RunFetcherWithNetError(net::Error net_error);
  ExploreSitesRequestStatus RunFetcherWithHttpError(
      net::HttpStatusCode http_error);
  ExploreSitesRequestStatus RunFetcherWithData(const std::string& response_data,
                                               std::string* data_received);
  ExploreSitesRequestStatus RunFetcherWithBackoffs(
      bool is_immediate_fetch,
      size_t num_of_backoffs,
      std::vector<base::TimeDelta> backoff_delays,
      std::vector<base::OnceCallback<void(void)>> respond_callbacks,
      std::string* data_received);

  void RespondWithData(const std::string& data);
  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);

  void SetUpExperimentOption(std::string option, std::string data) {
    base::FieldTrialParams params = {{option, data}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kExploreSites, params);
  }

  network::ResourceRequest last_resource_request;

  ExploreSitesRequestStatus last_status() const { return last_status_; }
  std::string last_data() const {
    return last_data_ ? *last_data_ : std::string();
  }
  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  const base::HistogramTester* histograms() const {
    return histogram_tester_.get();
  }

 private:
  ExploreSitesFetcher::Callback StoreResult();
  network::TestURLLoaderFactory::PendingRequest* GetLastPendingRequest();
  ExploreSitesRequestStatus RunFetcher(
      base::OnceCallback<void(void)> respond_callback,
      std::string* data_received);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  ExploreSitesRequestStatus last_status_;
  std::unique_ptr<std::string> last_data_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ExploreSitesFetcher::Callback ExploreSitesFetcherTest::StoreResult() {
  return base::BindLambdaForTesting(
      [&](ExploreSitesRequestStatus status, std::unique_ptr<std::string> data) {
        last_status_ = status;
        last_data_ = std::move(data);
      });
}

ExploreSitesFetcherTest::ExploreSitesFetcherTest()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

void ExploreSitesFetcherTest::SetUp() {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
        last_resource_request = request;
      }));
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithNetError(
    net::Error net_error) {
  std::string data_received;
  ExploreSitesRequestStatus status =
      RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                                base::Unretained(this), net_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());

  histograms()->ExpectUniqueSample("ExploreSites.FetcherNetErrorCode",
                                   -net_error, 1);
  histograms()->ExpectTotalCount("ExploreSites.FetcherHttpResponseCode", 0);

  return status;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithHttpError(
    net::HttpStatusCode http_error) {
  std::string data_received;
  ExploreSitesRequestStatus status =
      RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithHttpError,
                                base::Unretained(this), http_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());

  histograms()->ExpectUniqueSample("ExploreSites.FetcherNetErrorCode",
                                   -net::ERR_HTTP_RESPONSE_CODE_FAILURE, 1);
  histograms()->ExpectUniqueSample("ExploreSites.FetcherHttpResponseCode",
                                   http_error, 1);

  return status;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  ExploreSitesRequestStatus status =
      RunFetcher(base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                                base::Unretained(this), response_data),
                 data_received);
  return status;
}

void ExploreSitesFetcherTest::RespondWithNetError(int net_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  DCHECK(pending_requests_count > 0);
  network::URLLoaderCompletionStatus completion_status(net_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetLastPendingRequest()->request.url, completion_status,
      network::mojom::URLResponseHead::New(), std::string(),
      network::TestURLLoaderFactory::kMostRecentMatch);
}

void ExploreSitesFetcherTest::RespondWithHttpError(
    net::HttpStatusCode http_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  auto url_response_head = network::CreateURLResponseHead(http_error);
  DCHECK(pending_requests_count > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetLastPendingRequest()->request.url,
      network::URLLoaderCompletionStatus(net::OK), std::move(url_response_head),
      std::string(), network::TestURLLoaderFactory::kMostRecentMatch);
}

void ExploreSitesFetcherTest::RespondWithData(const std::string& data) {
  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetLastPendingRequest()->request.url.spec(), data, net::HTTP_OK,
      network::TestURLLoaderFactory::kMostRecentMatch);
}

network::TestURLLoaderFactory::PendingRequest*
ExploreSitesFetcherTest::GetLastPendingRequest() {
  network::TestURLLoaderFactory::PendingRequest* request =
      &(test_url_loader_factory_.pending_requests()->back());
  return request;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcher(
    base::OnceCallback<void(void)> respond_callback,
    std::string* data_received) {
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  std::unique_ptr<ExploreSitesFetcher> fetcher =
      CreateFetcher(true /* disable_retry*/, true /*is_immediate_fetch*/);

  std::move(respond_callback).Run();
  task_environment_.RunUntilIdle();

  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

ExploreSitesRequestStatus ExploreSitesFetcherTest::RunFetcherWithBackoffs(
    bool is_immediate_fetch,
    size_t num_of_backoffs,
    std::vector<base::TimeDelta> backoff_delays,
    std::vector<base::OnceCallback<void(void)>> respond_callbacks,
    std::string* data_received) {
  DCHECK_EQ(num_of_backoffs, backoff_delays.size());
  DCHECK(num_of_backoffs <= respond_callbacks.size() &&
         respond_callbacks.size() <= num_of_backoffs + 1);

  std::unique_ptr<ExploreSitesFetcher> fetcher =
      CreateFetcher(false /* disable_retry*/, is_immediate_fetch);

  std::move(respond_callbacks[0]).Run();
  task_environment_.RunUntilIdle();

  for (size_t i = 0; i < num_of_backoffs; ++i) {
    task_environment_.FastForwardBy(backoff_delays[i]);
    if (i + 1 <= respond_callbacks.size() - 1)
      std::move(respond_callbacks[i + 1]).Run();
    task_environment_.RunUntilIdle();
  }

  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

std::unique_ptr<ExploreSitesFetcher> ExploreSitesFetcherTest::CreateFetcher(
    bool disable_retry,
    bool is_immediate_fetch) {
  std::unique_ptr<ExploreSitesFetcher> fetcher =
      ExploreSitesFetcher::CreateForGetCatalog(
          is_immediate_fetch, "123", kAcceptLanguages, "zz",
          test_shared_url_loader_factory_, StoreResult());
  if (disable_retry)
    fetcher->disable_retry_for_testing();
  fetcher->SetDeviceDelegateForTest(std::make_unique<TestingDeviceDelegate>());
  fetcher->Start();
  return fetcher;
}

TEST_F(ExploreSitesFetcherTest, NetErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBlockedByAdministrator,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));

  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

TEST_F(ExploreSitesFetcherTest, HttpErrors) {
  EXPECT_EQ(ExploreSitesRequestStatus::kShouldSuspendBadRequest,
            RunFetcherWithHttpError(net::HTTP_BAD_REQUEST));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

TEST_F(ExploreSitesFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure, RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));
  EXPECT_EQ(kTestData, data);

  EXPECT_EQ(last_resource_request.url.spec(),
            "https://exploresites-pa.googleapis.com/v1/"
            "getcatalog?country_code=zz&version_token=123");
}

TEST_F(ExploreSitesFetcherTest, TestHeaders) {
  std::string data;
  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess,
            RunFetcherWithData(kTestData, &data));

  net::HttpRequestHeaders headers = last_resource_request.headers;
  std::string content_type;
  std::string languages;
  std::string scale_factor;
  bool success;

  success = headers.HasHeader("x-goog-api-key");
  EXPECT_TRUE(success);

  success = headers.HasHeader("X-Client-Version");
  EXPECT_TRUE(success);

  success = headers.GetHeader("X-Device-Scale-Factor", &scale_factor);
  EXPECT_TRUE(success);
  EXPECT_EQ(1.5, std::stof(scale_factor));

  success =
      headers.GetHeader(net::HttpRequestHeaders::kContentType, &content_type);
  EXPECT_TRUE(success);
  EXPECT_EQ("application/x-protobuf", content_type);

  success =
      headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage, &languages);
  EXPECT_TRUE(success);
  EXPECT_EQ(kAcceptLanguages, languages);

  // The finch header should not be set since the experiment is not on.
  success = headers.HasHeader("X-Goog-Chrome-Experiment-Tag");
  EXPECT_FALSE(success);
}

TEST_F(ExploreSitesFetcherTest, OneBackoffForImmediateFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::Milliseconds(initial_delay_ms)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(true /*is_immediate_fetch*/, 1u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, OneBackoffForBackgroundFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kBackgroundFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::Milliseconds(initial_delay_ms)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(false /*is_immediate_fetch*/, 1u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, TwoBackoffsForImmediateFetch) {
  std::string data;
  int initial_delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays = {
      base::Milliseconds(initial_delay_ms),
      base::Milliseconds(initial_delay_ms * 2)};
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                     base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithHttpError,
                     base::Unretained(this), net::HTTP_INTERNAL_SERVER_ERROR));
  respond_callbacks.push_back(
      base::BindOnce(&ExploreSitesFetcherTest::RespondWithData,
                     base::Unretained(this), kTestData));
  EXPECT_EQ(
      ExploreSitesRequestStatus::kSuccess,
      RunFetcherWithBackoffs(true /*is_immediate_fetch*/, 2u, backoff_delays,
                             std::move(respond_callbacks), &data));
  EXPECT_EQ(kTestData, data);
}

TEST_F(ExploreSitesFetcherTest, ExceedMaxBackoffsForImmediateFetch) {
  std::string data;
  int delay_ms =
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays;
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  for (int i = 0; i < ExploreSitesFetcher::kMaxFailureCountForImmediateFetch;
       ++i) {
    backoff_delays.push_back(base::Milliseconds(delay_ms));
    delay_ms *= 2;
    respond_callbacks.push_back(
        base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                       base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  }
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithBackoffs(
                true /*is_immediate_fetch*/,
                ExploreSitesFetcher::kMaxFailureCountForImmediateFetch,
                backoff_delays, std::move(respond_callbacks), &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, ExceedMaxBackoffsForBackgroundFetch) {
  std::string data;
  int delay_ms =
      ExploreSitesFetcher::kBackgroundFetchBackoffPolicy.initial_delay_ms;
  std::vector<base::TimeDelta> backoff_delays;
  std::vector<base::OnceCallback<void(void)>> respond_callbacks;
  for (int i = 0; i < ExploreSitesFetcher::kMaxFailureCountForBackgroundFetch;
       ++i) {
    backoff_delays.push_back(base::Milliseconds(delay_ms));
    delay_ms *= 2;
    respond_callbacks.push_back(
        base::BindOnce(&ExploreSitesFetcherTest::RespondWithNetError,
                       base::Unretained(this), net::ERR_INTERNET_DISCONNECTED));
  }
  EXPECT_EQ(ExploreSitesRequestStatus::kFailure,
            RunFetcherWithBackoffs(
                false /*is_immediate_fetch*/,
                ExploreSitesFetcher::kMaxFailureCountForBackgroundFetch,
                backoff_delays, std::move(respond_callbacks), &data));
  EXPECT_TRUE(data.empty());
}

TEST_F(ExploreSitesFetcherTest, RestartAsImmediateFetchIfNotYet) {
  // Start as background fetch.
  std::unique_ptr<ExploreSitesFetcher> fetcher =
      CreateFetcher(false /*disable_retry*/, false /*is_immediate_fetch*/);
  fetcher->Start();
  EXPECT_FALSE(fetcher->is_immediate_fetch());

  // Restart as immediate fetch.
  fetcher->RestartAsImmediateFetchIfNotYet();
  EXPECT_TRUE(fetcher->is_immediate_fetch());

  // Fail the request in order to trigger the backoff.
  RespondWithNetError(net::ERR_INTERNET_DISCONNECTED);
  task_environment()->RunUntilIdle();

  // Fast forward by the initial delay of the immediate fetch. The retry should
  // be triggered.
  task_environment()->FastForwardBy(base::Milliseconds(
      ExploreSitesFetcher::kImmediateFetchBackoffPolicy.initial_delay_ms));

  // Make the request succeeded.
  RespondWithData(kTestData);
  task_environment()->RunUntilIdle();

  EXPECT_EQ(ExploreSitesRequestStatus::kSuccess, last_status());
  EXPECT_EQ(kTestData, last_data());
}

}  // namespace explore_sites
