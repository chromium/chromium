// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/tpm/stub_install_attributes.h"
#endif

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::_;
using content::BrowserThread;

namespace safe_browsing {
namespace {

class MockModelLoader : public ModelLoader {
 public:
  explicit MockModelLoader(const std::string& model_name)
      : ModelLoader(base::Closure(), nullptr, model_name) {}
  ~MockModelLoader() override {}

  MOCK_METHOD1(ScheduleFetch, void(int64_t));
  MOCK_METHOD0(CancelFetcher, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModelLoader);
};

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(nullptr) {}

  ~MockClientSideDetectionService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

}  // namespace

class ClientSideDetectionServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    csd_service_.reset();
  }

  bool SendClientReportPhishingRequest(const GURL& phishing_url,
                                       float score) {
    ClientPhishingRequest* request = new ClientPhishingRequest();
    request->set_url(phishing_url.spec());
    request->set_client_score(score);
    request->set_is_phishing(true);  // client thinks the URL is phishing.
    base::RunLoop run_loop;
    csd_service_->SendClientReportPhishingRequest(
        request, false,
        base::Bind(&ClientSideDetectionServiceTest::SendRequestDone,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    phishing_url_ = phishing_url;
    run_loop.Run();  // Waits until callback is called.
    return is_phishing_;
  }

  bool SendClientReportMalwareRequest(const GURL& url) {
    std::unique_ptr<ClientMalwareRequest> request(new ClientMalwareRequest());
    request->set_url(url.spec());
    base::RunLoop run_loop;
    csd_service_->SendClientReportMalwareRequest(
        request.release(),
        base::Bind(&ClientSideDetectionServiceTest::SendMalwareRequestDone,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    phishing_url_ = url;
    run_loop.Run();  // Waits until callback is called.
    return is_malware_;
  }

  void SetModelFetchResponses() {
    // Set reponses for both models.
    test_url_loader_factory_.AddResponse(
        ModelLoader::kClientModelUrlPrefix +
            ModelLoader::FillInModelName(false, 0),
        "bogusmodel");
    test_url_loader_factory_.AddResponse(
        ModelLoader::kClientModelUrlPrefix +
            ModelLoader::FillInModelName(true, 0),
        "bogusmodel");
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

  void SetClientReportMalwareResponse(const std::string& response_data,
                                      int net_error) {
    SetResponse(ClientSideDetectionService::GetClientReportUrl(
                    ClientSideDetectionService::kClientReportMalwareUrl),
                response_data, net_error);
  }

  int GetNumReports(base::queue<base::Time>* report_times) {
    return csd_service_->GetNumReports(report_times);
  }

  base::queue<base::Time>& GetPhishingReportTimes() {
    return csd_service_->phishing_report_times_;
  }

  base::queue<base::Time>& GetMalwareReportTimes() {
    return csd_service_->malware_report_times_;
  }

  void SetCache(const GURL& gurl, bool is_phishing, base::Time time) {
    csd_service_->cache_[gurl] =
        std::make_unique<ClientSideDetectionService::CacheState>(is_phishing,
                                                                 time);
  }

  void TestCache() {
    auto& cache = csd_service_->cache_;
    base::Time now = base::Time::Now();
    base::Time time =
        now - base::TimeDelta::FromDays(
            ClientSideDetectionService::kNegativeCacheIntervalDays) +
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://first.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(false, time);

    time =
        now - base::TimeDelta::FromDays(
            ClientSideDetectionService::kNegativeCacheIntervalDays) -
        base::TimeDelta::FromHours(1);
    cache[GURL("http://second.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(false, time);

    time =
        now - base::TimeDelta::FromMinutes(
            ClientSideDetectionService::kPositiveCacheIntervalMinutes) -
        base::TimeDelta::FromMinutes(5);
    cache[GURL("http://third.url.com/")] =
        std::make_unique<ClientSideDetectionService::CacheState>(true, time);

    time =
        now - base::TimeDelta::FromMinutes(
            ClientSideDetectionService::kPositiveCacheIntervalMinutes) +
        base::TimeDelta::FromMinutes(5);
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
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://first.url.com"), &is_phishing));
    EXPECT_FALSE(is_phishing);
    EXPECT_FALSE(csd_service_->GetValidCachedResult(
        GURL("http://third.url.com"), &is_phishing));
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://fourth.url.com"), &is_phishing));
    EXPECT_TRUE(is_phishing);
  }

  void AddFeature(const std::string& name, double value,
                  ClientPhishingRequest* request) {
    ClientPhishingRequest_Feature* feature = request->add_feature_map();
    feature->set_name(name);
    feature->set_value(value);
  }

  void AddNonModelFeature(const std::string& name, double value,
                          ClientPhishingRequest* request) {
    ClientPhishingRequest_Feature* feature =
        request->add_non_model_feature_map();
    feature->set_name(name);
    feature->set_value(value);
  }

  void CheckConfirmedMalwareUrl(GURL url) {
    ASSERT_EQ(confirmed_malware_url_, url);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ClientSideDetectionService> csd_service_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
#endif

 private:
  void SendRequestDone(base::OnceClosure continuation_callback,
                       GURL phishing_url,
                       bool is_phishing) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    std::move(continuation_callback).Run();
  }

  void SendMalwareRequestDone(base::OnceClosure continuation_callback,
                              GURL original_url,
                              GURL malware_url,
                              bool is_malware) {
    ASSERT_EQ(phishing_url_, original_url);
    confirmed_malware_url_ = malware_url;
    is_malware_ = is_malware;
    std::move(continuation_callback).Run();
  }

  std::unique_ptr<base::FieldTrialList> field_trials_;

  GURL phishing_url_;
  GURL confirmed_malware_url_;
  bool is_phishing_;
  bool is_malware_;
};


TEST_F(ClientSideDetectionServiceTest, ServiceObjectDeletedBeforeCallbackDone) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);
  csd_service_->SetEnabledAndRefreshState(true);
  EXPECT_TRUE(csd_service_.get() != NULL);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  csd_service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ClientSideDetectionServiceTest, SendClientReportPhishingRequest) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);
  csd_service_->SetEnabledAndRefreshState(true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.

  base::Time before = base::Time::Now();

  // Invalid response body from the server.
  SetClientReportPhishingResponse("invalid proto response", net::OK);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score));

  // Normal behavior.
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score));

  // This request will fail
  GURL second_url("http://b.com/");
  response.set_phishy(false);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  net::ERR_FAILED);
  EXPECT_FALSE(SendClientReportPhishingRequest(second_url, score));

  base::Time after = base::Time::Now();

  // Check that we have recorded all 3 requests within the correct time range.
  base::queue<base::Time>& report_times = GetPhishingReportTimes();
  EXPECT_EQ(3U, report_times.size());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop();
    EXPECT_LE(before, time);
    EXPECT_GE(after, time);
  }

  // Only the first url should be in the cache.
  bool is_phishing;
  EXPECT_TRUE(csd_service_->IsInCache(url));
  EXPECT_TRUE(csd_service_->GetValidCachedResult(url, &is_phishing));
  EXPECT_TRUE(is_phishing);
  EXPECT_FALSE(csd_service_->IsInCache(second_url));
}

TEST_F(ClientSideDetectionServiceTest, SendClientReportMalwareRequest) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);
  csd_service_->SetEnabledAndRefreshState(true);
  GURL url("http://a.com/");

  base::Time before = base::Time::Now();
  // Invalid response body from the server.
  SetClientReportMalwareResponse("invalid proto response", net::OK);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // Missing bad_url.
  ClientMalwareResponse response;
  response.set_blacklist(true);
  SetClientReportMalwareResponse(response.SerializeAsString(), net::OK);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // Normal behavior.
  response.set_blacklist(true);
  response.set_bad_url("http://response-bad.com/");
  SetClientReportMalwareResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportMalwareRequest(url));
  CheckConfirmedMalwareUrl(GURL("http://response-bad.com/"));

  // This request will fail
  response.set_blacklist(false);
  SetClientReportMalwareResponse(response.SerializeAsString(), net::ERR_FAILED);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // Server blacklist decision is false, and response is successful
  response.set_blacklist(false);
  SetClientReportMalwareResponse(response.SerializeAsString(), net::OK);
  EXPECT_FALSE(SendClientReportMalwareRequest(url));

  // Check that we have recorded all 5 requests within the correct time range.
  base::Time after = base::Time::Now();
  base::queue<base::Time>& report_times = GetMalwareReportTimes();
  EXPECT_EQ(5U, report_times.size());

  // Check that the malware report limit was reached.
  EXPECT_TRUE(csd_service_->OverMalwareReportLimit());

  report_times = GetMalwareReportTimes();
  EXPECT_EQ(5U, report_times.size());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop();
    EXPECT_LE(before, time);
    EXPECT_GE(after, time);
  }
}

TEST_F(ClientSideDetectionServiceTest, GetNumReportTest) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);

  base::queue<base::Time>& report_times = GetPhishingReportTimes();
  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::TimeDelta::FromHours(25);
  report_times.push(now - twenty_five_hours);
  report_times.push(now - twenty_five_hours);
  report_times.push(now);
  report_times.push(now);

  EXPECT_EQ(2, GetNumReports(&report_times));
}

TEST_F(ClientSideDetectionServiceTest, CacheTest) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);

  TestCache();
}

TEST_F(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  SetModelFetchResponses();
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);

  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("10.1.2.3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("127.0.0.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("172.24.3.4"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("192.168.1.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fc00::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fec0::"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("fec0:1:2::3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("::1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("::ffff:192.168.1.1"));

  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("200.1.1.1"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("2001:0db8:ac10:fe01::"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress("::ffff:23c5:281b"));

  // If the address can't be parsed, the default is true.
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress("blah"));
}

TEST_F(ClientSideDetectionServiceTest, SetEnabledAndRefreshState) {
  // Check that the model isn't downloaded until the service is enabled.
  csd_service_ =
      ClientSideDetectionService::Create(test_shared_loader_factory_);
  EXPECT_FALSE(csd_service_->enabled());
  EXPECT_TRUE(csd_service_->model_loader_standard_->url_loader_.get() == NULL);

  // Use a MockClientSideDetectionService for the rest of the test, to avoid
  // the scheduling delay.
  MockClientSideDetectionService* service =
      new StrictMock<MockClientSideDetectionService>();
  // Inject mock loaders.
  MockModelLoader* loader_1 = new StrictMock<MockModelLoader>("model1");
  MockModelLoader* loader_2 = new StrictMock<MockModelLoader>("model2");
  service->model_loader_standard_.reset(loader_1);
  service->model_loader_extended_.reset(loader_2);
  csd_service_.reset(service);

  EXPECT_FALSE(csd_service_->enabled());
  // No calls expected yet.
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(loader_1);
  Mock::VerifyAndClearExpectations(loader_2);

  // Check that initial ScheduleFetch() calls are made.
  EXPECT_CALL(*loader_1,
              ScheduleFetch(
                  ClientSideDetectionService::kInitialClientModelFetchDelayMs));
  EXPECT_CALL(*loader_2,
              ScheduleFetch(
                  ClientSideDetectionService::kInitialClientModelFetchDelayMs));
  csd_service_->SetEnabledAndRefreshState(true);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(loader_1);
  Mock::VerifyAndClearExpectations(loader_2);

  // Check that enabling again doesn't request the model.
  csd_service_->SetEnabledAndRefreshState(true);
  // No calls expected.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(loader_1);
  Mock::VerifyAndClearExpectations(loader_2);

  // Check that disabling the service cancels pending requests.
  EXPECT_CALL(*loader_1, CancelFetcher());
  EXPECT_CALL(*loader_2, CancelFetcher());
  csd_service_->SetEnabledAndRefreshState(false);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(loader_1);
  Mock::VerifyAndClearExpectations(loader_2);

  // Check that disabling again doesn't request the model.
  csd_service_->SetEnabledAndRefreshState(false);
  // No calls expected.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(service);
  Mock::VerifyAndClearExpectations(loader_1);
  Mock::VerifyAndClearExpectations(loader_2);
}
}  // namespace safe_browsing
