// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/availability/availability_prober.h"

#include <cmath>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kTestUrl("https://test.com");
const base::TimeDelta kCacheRevalidateAfter(base::TimeDelta::FromDays(1));
}  // namespace

class TestDelegate : public AvailabilityProber::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() = default;

  bool ShouldSendNextProbe() override { return should_send_next_probe_; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    return net_error == net::OK && head &&
           head->headers->response_code() == net::HTTP_OK;
  }

  void set_should_send_next_probe(bool should_send_next_probe) {
    should_send_next_probe_ = should_send_next_probe;
  }

 private:
  bool should_send_next_probe_ = true;
};

class TestAvailabilityProber : public AvailabilityProber {
 public:
  TestAvailabilityProber(
      AvailabilityProber::Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      const AvailabilityProber::ClientName name,
      const GURL& url,
      const HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const size_t max_cache_entries,
      base::TimeDelta revalidate_cache_after,
      const base::TickClock* tick_clock,
      const base::Clock* clock)
      : AvailabilityProber(delegate,
                           url_loader_factory,
                           pref_service,
                           name,
                           url,
                           http_method,
                           headers,
                           retry_policy,
                           timeout_policy,
                           traffic_annotation,
                           max_cache_entries,
                           revalidate_cache_after,
                           tick_clock,
                           clock) {}
};

class AvailabilityProberTest : public testing::Test {
 public:
  AvailabilityProberTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        test_delegate_(),
        test_prefs_() {}

  void SetUp() override {
    AvailabilityProber::RegisterProfilePrefs(test_prefs_.registry());
  }

  std::unique_ptr<AvailabilityProber> NewProber() {
    return NewProberWithPolicies(AvailabilityProber::RetryPolicy(),
                                 AvailabilityProber::TimeoutPolicy());
  }

  std::unique_ptr<AvailabilityProber> NewProberWithRetryPolicy(
      const AvailabilityProber::RetryPolicy& retry_policy) {
    return NewProberWithPolicies(retry_policy,
                                 AvailabilityProber::TimeoutPolicy());
  }

  std::unique_ptr<AvailabilityProber> NewProberWithPolicies(
      const AvailabilityProber::RetryPolicy& retry_policy,
      const AvailabilityProber::TimeoutPolicy& timeout_policy) {
    return NewProberWithPoliciesAndDelegate(&test_delegate_, retry_policy,
                                            timeout_policy);
  }

  std::unique_ptr<AvailabilityProber> NewProberWithPoliciesAndDelegate(
      AvailabilityProber::Delegate* delegate,
      const AvailabilityProber::RetryPolicy& retry_policy,
      const AvailabilityProber::TimeoutPolicy& timeout_policy) {
    net::HttpRequestHeaders headers;
    headers.SetHeader("X-Testing", "Hello world");
    std::unique_ptr<TestAvailabilityProber> prober =
        std::make_unique<TestAvailabilityProber>(
            delegate, test_shared_loader_factory_, &test_prefs_,
            AvailabilityProber::ClientName::kLitepages, kTestUrl,
            AvailabilityProber::HttpMethod::kGet, headers, retry_policy,
            timeout_policy, TRAFFIC_ANNOTATION_FOR_TESTS, 1,
            kCacheRevalidateAfter, task_environment_.GetMockTickClock(),
            task_environment_.GetMockClock());
    prober->SetOnCompleteCallback(base::BindRepeating(
        &AvailabilityProberTest::OnProbeComplete, base::Unretained(this)));
    return prober;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void MakeResponseAndWait(net::HttpStatusCode http_status,
                           net::Error net_error) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    ASSERT_EQ(request->request.url.host(), kTestUrl.host());
    ASSERT_EQ(request->request.url.scheme(), kTestUrl.scheme());

    auto head = network::CreateURLResponseHead(http_status);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         "content", status);
    RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  void VerifyNoRequests() {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  }

  void VerifyRequest(bool expect_random_guid = false) {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

    std::string testing_header;
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    request->request.headers.GetHeader("X-Testing", &testing_header);

    EXPECT_EQ(testing_header, "Hello world");
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags, net::LOAD_DISABLE_CACHE);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kOmit);
    if (expect_random_guid) {
      EXPECT_NE(request->request.url, kTestUrl);
      EXPECT_TRUE(request->request.url.query().find("guid=") !=
                  std::string::npos);
      EXPECT_EQ(request->request.url.query().length(),
                5U /* len("guid=") */ + 36U /* len(hex guid with hyphens) */);
      // We don't check for the randomness of successive GUIDs on the assumption
      // base::GenerateGUID() is always correct.
    } else {
      EXPECT_EQ(request->request.url, kTestUrl);
    }
  }

  void OnProbeComplete(bool success) { callback_result_ = success; }

  base::Optional<bool> callback_result() { return callback_result_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestDelegate test_delegate_;
  TestingPrefServiceSimple test_prefs_;
  base::Optional<bool> callback_result_;
};

TEST_F(AvailabilityProberTest, OK) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.NumAttemptsBeforeSuccess.Litepages", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.ResponseCode.Litepages", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

TEST_F(AvailabilityProberTest, OK_Callback) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  EXPECT_TRUE(callback_result().has_value());
  EXPECT_TRUE(callback_result().value());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.ResponseCode.Litepages", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

TEST_F(AvailabilityProberTest, MultipleStart) {
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // Calling |SendNowIfInactive| many times should result in only one url
  // request, which is verified in |VerifyRequest|.
  prober->SendNowIfInactive(false);
  prober->SendNowIfInactive(false);
  prober->SendNowIfInactive(false);
  VerifyRequest();
}

TEST_F(AvailabilityProberTest, NetworkChangeStartsProber) {
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  RunUntilIdle();

  EXPECT_TRUE(prober->is_active());
}

TEST_F(AvailabilityProberTest, NetworkConnectionShardsCache) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // The different type of cellular networks shouldn't make a difference.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  RunUntilIdle();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_2G);
  RunUntilIdle();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  // Switching to WIFI does make a difference.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  RunUntilIdle();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
}

TEST_F(AvailabilityProberTest, CacheMaxSize) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  FastForward(base::TimeDelta::FromSeconds(1));

  // Change the connection type and report a new probe result.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  RunUntilIdle();

  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  FastForward(base::TimeDelta::FromSeconds(1));

  // Then, flip back to the original connection type. The old probe status
  // should not be persisted since the max cache size for testing is 1.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
}

TEST_F(AvailabilityProberTest, CacheAutoRevalidation) {
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward until just before revalidation time.
  FastForward(kCacheRevalidateAfter - base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward the rest of the way and check the prober is active again.
  FastForward(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());
}

TEST_F(AvailabilityProberTest, PersistentCache) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Create a new prober instance and verify the cached probe result is used.
  prober = NewProber();
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  // Fast forward past the cache revalidation and check that the revalidation
  // time was also persisted.
  FastForward(kCacheRevalidateAfter);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.ResponseCode.Litepages", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::OK), 1);
}

#if defined(OS_ANDROID)
TEST_F(AvailabilityProberTest, StartInForeground) {
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  prober->SendNowIfInactive(true);
  EXPECT_TRUE(prober->is_active());
}

TEST_F(AvailabilityProberTest, DoesntCallSendInForegroundIfInactive) {
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_FALSE(prober->is_active());
}
#endif

TEST_F(AvailabilityProberTest, NetError) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 4);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", false, 1);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 4);
}

TEST_F(AvailabilityProberTest, NetError_Callback) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  EXPECT_TRUE(callback_result().has_value());
  EXPECT_FALSE(callback_result().value());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 4);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", false, 1);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 4);
}

TEST_F(AvailabilityProberTest, HttpError) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 4);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.ResponseCode.Litepages", net::HTTP_NOT_FOUND, 4);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::OK), 4);
}

TEST_F(AvailabilityProberTest, TimeUntilSuccess) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  FastForward(base::TimeDelta::FromMilliseconds(11000));

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectTotalCount(
      "Availability.Prober.TimeUntilFailure2.Litepages", 0);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.TimeUntilSuccess2.Litepages", 11000, 1);
}

TEST_F(AvailabilityProberTest, TimeUntilFailure) {
  base::HistogramTester histogram_tester;

  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();

  FastForward(base::TimeDelta::FromMilliseconds(11000));

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectTotalCount(
      "Availability.Prober.TimeUntilSuccess2.Litepages", 0);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.TimeUntilFailure2.Litepages", 11000, 1);
}

TEST_F(AvailabilityProberTest, RandomGUID) {
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.use_random_urls = true;
  retry_policy.max_retries = 0;

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest(true /* expect_random_guid */);

  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(AvailabilityProberTest, RetryLinear) {
  base::HistogramTester histogram_tester;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = AvailabilityProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 1);
  histogram_tester.ExpectTotalCount("Availability.Prober.FinalState.Litepages",
                                    0);

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 2);
  histogram_tester.ExpectTotalCount("Availability.Prober.FinalState.Litepages",
                                    0);

  // Second retry should be another 1000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 3);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", false, 1);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 3);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.NumAttemptsBeforeSuccess.Litepages", 0);
}

TEST_F(AvailabilityProberTest, RetryThenSucceed) {
  base::HistogramTester histogram_tester;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = AvailabilityProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 1);
  histogram_tester.ExpectTotalCount("Availability.Prober.FinalState.Litepages",
                                    0);

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 2);
  histogram_tester.ExpectTotalCount("Availability.Prober.FinalState.Litepages",
                                    0);

  // Second retry should be another 1000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     false, 2);
  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.FinalState.Litepages", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.NumAttemptsBeforeSuccess.Litepages", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.ResponseCode.Litepages", net::HTTP_OK, 1);
  histogram_tester.ExpectBucketCount("Availability.Prober.NetError.Litepages",
                                     std::abs(net::ERR_FAILED), 2);
  histogram_tester.ExpectBucketCount("Availability.Prober.NetError.Litepages",
                                     std::abs(net::OK), 1);
  histogram_tester.ExpectTotalCount("Availability.Prober.NetError.Litepages",
                                    3);
}

TEST_F(AvailabilityProberTest, RetryExponential) {
  base::HistogramTester histogram_tester;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = AvailabilityProber::Backoff::kExponential;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithRetryPolicy(retry_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Second retry should be another 2000ms later and be the final one.
  FastForward(base::TimeDelta::FromMilliseconds(1999));
  VerifyNoRequests();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 3);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_FAILED), 3);
}

TEST_F(AvailabilityProberTest, TimeoutLinear) {
  base::HistogramTester histogram_tester;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 1;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(10);

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.backoff = AvailabilityProber::Backoff::kLinear;
  timeout_policy.base_timeout = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithPolicies(retry_policy, timeout_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // First attempt.
  prober->SendNowIfInactive(false);
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Fast forward to the start of the next attempt.
  FastForward(base::TimeDelta::FromMilliseconds(10));

  // Second attempt should have the same timeout.
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 2);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_TIMED_OUT), 2);
}

TEST_F(AvailabilityProberTest, TimeoutExponential) {
  base::HistogramTester histogram_tester;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 1;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(10);

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.backoff = AvailabilityProber::Backoff::kExponential;
  timeout_policy.base_timeout = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober =
      NewProberWithPolicies(retry_policy, timeout_policy);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  // First attempt.
  prober->SendNowIfInactive(false);
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // Fast forward to the start of the next attempt.
  FastForward(base::TimeDelta::FromMilliseconds(10));

  // Second attempt should have a 2s timeout.
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1999));
  VerifyRequest();
  FastForward(base::TimeDelta::FromMilliseconds(1));
  VerifyNoRequests();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.Litepages", false, 2);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.ResponseCode.Litepages", 0);
  histogram_tester.ExpectUniqueSample("Availability.Prober.NetError.Litepages",
                                      std::abs(net::ERR_TIMED_OUT), 2);
}

TEST_F(AvailabilityProberTest, DelegateStopsFirstProbe) {
  base::HistogramTester histogram_tester;
  TestDelegate delegate;
  delegate.set_should_send_next_probe(false);

  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = AvailabilityProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober = NewProberWithPoliciesAndDelegate(
      &delegate, retry_policy, AvailabilityProber::TimeoutPolicy());
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());
  VerifyNoRequests();

  histogram_tester.ExpectTotalCount("Availability.Prober.DidSucceed.Litepages",
                                    0);
  histogram_tester.ExpectTotalCount("Availability.Prober.FinalState.Litepages",
                                    0);
}

TEST_F(AvailabilityProberTest, DelegateStopsRetries) {
  TestDelegate delegate;

  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff = AvailabilityProber::Backoff::kLinear;
  retry_policy.base_interval = base::TimeDelta::FromMilliseconds(1000);

  std::unique_ptr<AvailabilityProber> prober = NewProberWithPoliciesAndDelegate(
      &delegate, retry_policy, AvailabilityProber::TimeoutPolicy());
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED);
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  // First retry.
  FastForward(base::TimeDelta::FromMilliseconds(999));
  VerifyNoRequests();
  delegate.set_should_send_next_probe(false);
  FastForward(base::TimeDelta::FromMilliseconds(1));

  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
  VerifyNoRequests();
}

TEST_F(AvailabilityProberTest, CacheEntryAge) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->SendNowIfInactive(false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.CacheEntryAge.Litepages", 0, 1);

  FastForward(base::TimeDelta::FromHours(24));
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());

  histogram_tester.ExpectBucketCount(
      "Availability.Prober.CacheEntryAge.Litepages", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Availability.Prober.CacheEntryAge.Litepages", 24, 1);
  histogram_tester.ExpectTotalCount(
      "Availability.Prober.CacheEntryAge.Litepages", 2);
}

TEST_F(AvailabilityProberTest, Repeating) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);

  prober->RepeatedlyProbe(base::TimeDelta::FromSeconds(1), false);
  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  FastForward(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(prober->is_active());

  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());
}

TEST_F(AvailabilityProberTest, ReportExternalFailure_WhileIdle) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  prober->ReportExternalFailureAndRetry();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());

  VerifyRequest();
  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     true, 1);
  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.AfterReportedFailure.Litepages", true, 1);
}

TEST_F(AvailabilityProberTest, ReportExternalFailure_WhileActive) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AvailabilityProber> prober = NewProber();
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_FALSE(prober->is_active());

  prober->SendNowIfInactive(false);
  EXPECT_EQ(prober->LastProbeWasSuccessful(), base::nullopt);
  EXPECT_TRUE(prober->is_active());
  VerifyRequest();

  prober->ReportExternalFailureAndRetry();
  EXPECT_FALSE(prober->LastProbeWasSuccessful().value());
  EXPECT_TRUE(prober->is_active());
  VerifyRequest();

  MakeResponseAndWait(net::HTTP_OK, net::OK);
  EXPECT_TRUE(prober->LastProbeWasSuccessful().value());
  EXPECT_FALSE(prober->is_active());

  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     true, 1);
  histogram_tester.ExpectBucketCount("Availability.Prober.DidSucceed.Litepages",
                                     false, 1);
  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.AfterReportedFailure.Litepages", true, 1);
}
