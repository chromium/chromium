// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::_;
using content::BrowserThread;

namespace safe_browsing {

class ClientSideDetectionServiceTest : public testing::Test {
 public:
  ClientSideDetectionServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    feature_list_.InitAndEnableFeature(
        kSafeBrowsingRemoveCookiesInAuthRequests);
  }

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

  bool OverPhishingReportLimit() {
    return csd_service_->OverPhishingReportLimit();
  }

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
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://first.url.com"), &is_phishing));
    EXPECT_FALSE(is_phishing);
    EXPECT_FALSE(csd_service_->GetValidCachedResult(
        GURL("http://third.url.com"), &is_phishing));
    EXPECT_TRUE(csd_service_->GetValidCachedResult(
        GURL("http://fourth.url.com"), &is_phishing));
    EXPECT_TRUE(is_phishing);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ClientSideDetectionService> csd_service_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  base::test::ScopedFeatureList feature_list_;

 private:
  void SendRequestDone(base::OnceClosure continuation_callback,
                       GURL phishing_url,
                       bool is_phishing) {
    ASSERT_EQ(phishing_url, phishing_url_);
    is_phishing_ = is_phishing;
    std::move(continuation_callback).Run();
  }

  std::unique_ptr<base::FieldTrialList> field_trials_;

  GURL phishing_url_;
  bool is_phishing_;
};


TEST_F(ClientSideDetectionServiceTest, ServiceObjectDeletedBeforeCallbackDone) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_NE(csd_service_.get(), nullptr);
  // We delete the client-side detection service class even though the callbacks
  // haven't run yet.
  csd_service_.reset();
  // Waiting for the callbacks to run should not crash even if the service
  // object is gone.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ClientSideDetectionServiceTest, SendClientReportPhishingRequest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token;

  // Safe browsing is not enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  base::Time before = base::Time::Now();

  // Invalid response body from the server.
  SetClientReportPhishingResponse("invalid proto response", net::OK);
  EXPECT_FALSE(SendClientReportPhishingRequest(url, score, access_token));

  // Normal behavior with no access token.
  ClientPhishingResponse response;
  response.set_phishy(true);
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));

  // This request will fail
  GURL second_url("http://b.com/");
  response.set_phishy(false);
  SetClientReportPhishingResponse(response.SerializeAsString(),
                                  net::ERR_FAILED);
  EXPECT_FALSE(
      SendClientReportPhishingRequest(second_url, score, access_token));

  base::Time after = base::Time::Now();

  // Check that we have recorded all 5 requests within the correct time range.
  std::deque<base::Time>& report_times = GetPhishingReportTimes();
  EXPECT_EQ(5U, report_times.size());
  EXPECT_TRUE(OverPhishingReportLimit());
  while (!report_times.empty()) {
    base::Time time = report_times.back();
    report_times.pop_back();
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

TEST_F(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithToken) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "fake access token";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::string out;
        EXPECT_TRUE(request.headers.GetHeader(
            net::HttpRequestHeaders::kAuthorization, &out));
        EXPECT_EQ(out, "Bearer " + access_token);
        // Cookies should be removed when token is set.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_F(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithoutToken) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));
  csd_service_->SetURLLoaderFactoryForTesting(test_shared_loader_factory_);

  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  GURL url("http://a.com/");
  float score = 0.4f;  // Some random client score.
  std::string access_token = "";
  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::string out;
        EXPECT_FALSE(request.headers.GetHeader(
            net::HttpRequestHeaders::kAuthorization, &out));
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));
  SetClientReportPhishingResponse(response.SerializeAsString(), net::OK);
  EXPECT_TRUE(SendClientReportPhishingRequest(url, score, access_token));
}

TEST_F(ClientSideDetectionServiceTest, GetNumReportTest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));

  base::Time now = base::Time::Now();
  base::TimeDelta twenty_five_hours = base::Hours(25);
  csd_service_->AddPhishingReport(now - twenty_five_hours);
  csd_service_->AddPhishingReport(now - twenty_five_hours);
  csd_service_->AddPhishingReport(now);
  csd_service_->AddPhishingReport(now);

  EXPECT_EQ(2, csd_service_->GetPhishingNumReports());
  EXPECT_FALSE(OverPhishingReportLimit());
}

TEST_F(ClientSideDetectionServiceTest, CacheTest) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));

  TestCache();
}

TEST_F(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));

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

TEST_F(ClientSideDetectionServiceTest, IsLocalResource) {
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));

  net::IPAddress address;
  EXPECT_TRUE(csd_service_->IsLocalResource(address));

  // Create an IP address of invalid length
  uint8_t addr[5] = {0xFE, 0xDC, 0xBA, 0x98};
  address = net::IPAddress(addr);
  EXPECT_TRUE(csd_service_->IsLocalResource(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsLocalResource(address));
}

TEST_F(ClientSideDetectionServiceTest, TestModelFollowsPrefs) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      std::make_unique<ChromeClientSideDetectionServiceDelegate>(profile_));

  // Safe Browsing is not enabled.
  EXPECT_FALSE(csd_service_->enabled());

  // Safe Browsing is enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_TRUE(csd_service_->enabled());
}

}  // namespace safe_browsing
