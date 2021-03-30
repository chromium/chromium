// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/user_population.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"

using ::testing::_;

namespace safe_browsing {

namespace {
constexpr char kRealTimeLookupUrl[] =
    "https://enterprise-safebrowsing.googleapis.com/safebrowsing/clientreport/"
    "realtime";

class MockReferrerChainProvider : public ReferrerChainProvider {
 public:
  virtual ~MockReferrerChainProvider() = default;
  MOCK_METHOD3(IdentifyReferrerChainByWebContents,
               AttributionResult(content::WebContents* web_contents,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD4(IdentifyReferrerChainByEventURL,
               AttributionResult(const GURL& event_url,
                                 SessionID event_tab_id,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD3(IdentifyReferrerChainByPendingEventURL,
               AttributionResult(const GURL& event_url,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
};

}  // namespace

class ChromeEnterpriseRealTimeUrlLookupServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(test_pref_service_.registry());
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */,
        false /* restore_session */);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        nullptr, content_setting_map_.get());
    referrer_chain_provider_ = std::make_unique<MockReferrerChainProvider>();

    TestingProfile::Builder builder;
    test_profile_ = builder.Build();

    enterprise_rt_service_ = std::make_unique<
        ChromeEnterpriseRealTimeUrlLookupService>(
        test_shared_loader_factory_, cache_manager_.get(), test_profile_.get(),
        base::BindRepeating(
            [](Profile* profile, syncer::TestSyncService* test_sync_service) {
              ChromeUserPopulation population = GetUserPopulation(profile);
              population.set_is_history_sync_enabled(
                  safe_browsing::SyncUtils::IsHistorySyncEnabled(
                      test_sync_service));
              population.set_profile_management_status(
                  ChromeUserPopulation::NOT_MANAGED);
              population.set_is_under_advanced_protection(true);
              return population;
            },
            test_profile_.get(), &test_sync_service_),
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            test_profile_.get()),
        referrer_chain_provider_.get());

    test_profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
        REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    test_profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);
  }

  void TearDown() override {
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url) {
    return enterprise_rt_service_->GetCachedRealTimeUrlVerdict(url);
  }

  void MayBeCacheRealTimeUrlVerdict(
      GURL url,
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    new_threat_info->set_verdict_type(verdict_type);
    new_threat_info->set_threat_type(threat_type);
    new_threat_info->set_cache_duration_sec(cache_duration_sec);
    new_threat_info->set_cache_expression_using_match_type(cache_expression);
    new_threat_info->set_cache_expression_match_type(
        cache_expression_match_type);
    enterprise_rt_service_->MayBeCacheRealTimeUrlVerdict(url, response);
  }

  void SetUpRTLookupResponse(
      RTLookupResponse::ThreatInfo::VerdictType verdict_type,
      RTLookupResponse::ThreatInfo::ThreatType threat_type,
      int cache_duration_sec,
      const std::string& cache_expression,
      RTLookupResponse::ThreatInfo::CacheExpressionMatchType
          cache_expression_match_type) {
    RTLookupResponse response;
    RTLookupResponse::ThreatInfo* new_threat_info = response.add_threat_info();
    RTLookupResponse::ThreatInfo threat_info;
    threat_info.set_verdict_type(verdict_type);
    threat_info.set_threat_type(threat_type);
    threat_info.set_cache_duration_sec(cache_duration_sec);
    threat_info.set_cache_expression_using_match_type(cache_expression);
    threat_info.set_cache_expression_match_type(cache_expression_match_type);
    *new_threat_info = threat_info;
    std::string expected_response_str;
    response.SerializeToString(&expected_response_str);
    test_url_loader_factory_.AddResponse(kRealTimeLookupUrl,
                                         expected_response_str);
  }

  ChromeEnterpriseRealTimeUrlLookupService* enterprise_rt_service() {
    return enterprise_rt_service_.get();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<ChromeEnterpriseRealTimeUrlLookupService>
      enterprise_rt_service_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  std::unique_ptr<TestingProfile> test_profile_;
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<MockReferrerChainProvider> referrer_chain_provider_;
};

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_ResponseIsAlreadyCached) {
  GURL url("http://example.test/");
  MayBeCacheRealTimeUrlVerdict(url, RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  base::MockCallback<RTLookupRequestCallback> request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(url, request_callback.Get(),
                                       response_callback.Get());

  // |request_callback| should not be called if the verdict is already cached.
  EXPECT_CALL(request_callback, Run(_, _)).Times(0);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ true, _));

  task_environment_.RunUntilIdle();
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_RequestWithDmToken) {
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting("dm_token"));
  // Referrer chain is currently disabled for enterprise requests.
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(_, _, _))
      .Times(0);

  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url,
      base::BindOnce(
          [](std::unique_ptr<RTLookupRequest> request, std::string token) {
            EXPECT_EQ("http://example.test/", request->url());
            EXPECT_EQ("dm_token", request->dm_token());
            EXPECT_EQ(ChromeUserPopulation::SAFE_BROWSING,
                      request->population().user_population());
            EXPECT_TRUE(request->population().is_history_sync_enabled());
            EXPECT_EQ(ChromeUserPopulation::NOT_MANAGED,
                      request->population().profile_management_status());
            EXPECT_TRUE(request->population().is_under_advanced_protection());
            EXPECT_EQ("", token);
          }),
      response_callback.Get());

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  task_environment_.RunUntilIdle();

  // Check the response is cached.
  EXPECT_NE(nullptr, GetCachedRealTimeUrlVerdict(url));
}

}  // namespace safe_browsing
