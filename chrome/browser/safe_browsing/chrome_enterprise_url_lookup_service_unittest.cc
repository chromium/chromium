// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/platform_test.h"

using base::test::RunOnceClosure;
using ::testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace safe_browsing {

namespace {
constexpr char kRealTimeLookupUrl[] =
    "https://enterprise-safebrowsing.googleapis.com/safebrowsing/clientreport/"
    "realtime";

constexpr char kTestProfileEmail[] = "test@example.com";
constexpr char kContentAreaAccountEmail[] = "area@example.com";

class MockReferrerChainProvider : public ReferrerChainProvider {
 public:
  virtual ~MockReferrerChainProvider() = default;
  MOCK_METHOD3(IdentifyReferrerChainByRenderFrameHost,
               AttributionResult(content::RenderFrameHost* rfh,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD5(IdentifyReferrerChainByEventURL,
               AttributionResult(const GURL& event_url,
                                 SessionID event_tab_id,
                                 const content::GlobalRenderFrameHostId&
                                     event_outermost_main_frame_id,
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

bool GetRequestProto(const network::ResourceRequest& request,
                     RTLookupRequest* request_proto) {
  if (!request.request_body || !request.request_body->elements()) {
    return false;
  }

  // Supporting one DataElementBytes is sufficient here. If request
  // protos grow to need data pipes, we would need further test code
  // to read the contents of the pipe.
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  if (elements->size() != 1 ||
      elements->at(0).type() !=
          network::mojom::DataElementDataView::Tag::kBytes) {
    return false;
  }
  return request_proto->ParseFromString(std::string(
      elements->at(0).As<network::DataElementBytes>().AsStringPiece()));
}

}  // namespace

class ChromeEnterpriseRealTimeUrlLookupServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(test_pref_service_.registry());
    enterprise_connectors::RegisterProfilePrefs(test_pref_service_.registry());
    PlatformTest::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session */,
        false /*should_record_metrics*/);
    cache_manager_ = std::make_unique<VerdictCacheManager>(
        /*history_service=*/nullptr, content_setting_map_.get(),
        &test_pref_service_,
        /*sync_observer=*/nullptr);
    referrer_chain_provider_ = std::make_unique<MockReferrerChainProvider>();

    test_profile_ = profile_manager_->CreateTestingProfile("testing_profile");

    identity_test_env_.MakePrimaryAccountAvailable(
        kTestProfileEmail, signin::ConsentLevel::kSignin);

    management_service_ = std::make_unique<policy::ManagementService>(
        std::vector<std::unique_ptr<policy::ManagementStatusProvider>>());

    enterprise_rt_service_ =
        CreateServiceAndEnablePolicy(test_profile_, /*is_off_the_record=*/false,
                                     /*is_guest_session=*/false,
                                     /*set_raw_token_fetcher=*/true);
  }

  void TearDown() override {
    cache_manager_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  std::unique_ptr<ChromeEnterpriseRealTimeUrlLookupService>
  CreateServiceAndEnablePolicy(Profile* profile,
                               bool is_off_the_record = false,
                               bool is_guest_session = false,
                               bool set_raw_token_fetcher = false) {
    auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
    if (set_raw_token_fetcher) {
      raw_token_fetcher_ = token_fetcher.get();
    }

    auto enterprise_rt_service = std::make_unique<
        ChromeEnterpriseRealTimeUrlLookupService>(
        test_shared_loader_factory_, cache_manager_.get(),
        base::BindRepeating(
            [](Profile* profile, syncer::TestSyncService* test_sync_service) {
              ChromeUserPopulation population =
                  GetUserPopulationForProfile(profile);
              population.set_is_history_sync_enabled(
                  safe_browsing::SyncUtils::IsHistorySyncEnabled(
                      test_sync_service));
              population.set_profile_management_status(
                  ChromeUserPopulation::NOT_MANAGED);
              population.set_is_under_advanced_protection(true);
              return population;
            },
            profile, &test_sync_service_),
        std::move(token_fetcher),
        // TODO(crbug.com/406211981): Replace with mock ConnectorsServiceBase.
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            profile),
        referrer_chain_provider_.get(), &test_pref_service_,
        /*webui_delegate=*/nullptr, identity_test_env_.identity_manager(),
        management_service_.get(), is_off_the_record, is_guest_session,
        base::BindRepeating([]() -> std::string { return kTestProfileEmail; }),
        base::BindRepeating([](const GURL& tab_url) -> std::string {
          return kContentAreaAccountEmail;
        }),
        base::BindRepeating([] { return true; }),
        /*is_command_line_switch_supported=*/true);

    test_pref_service_.SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    test_pref_service_.SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);
    // ConnectorsService reads from the profile prefs until the dependency on
    // Profile is removed.
    profile->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    profile->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);

    return enterprise_rt_service;
  }

  std::unique_ptr<RTLookupResponse> GetCachedRealTimeUrlVerdict(
      const GURL& url) {
    return enterprise_rt_service_->GetCachedRealTimeUrlVerdict(url);
  }

  ChromeEnterpriseRealTimeUrlLookupService* enterprise_rt_service() {
    return enterprise_rt_service_.get();
  }

  void FulfillAccessTokenRequest(std::string token) {
    raw_token_fetcher_->RunAccessTokenCallback(token);
  }

  TestSafeBrowsingTokenFetcher* raw_token_fetcher() {
    return raw_token_fetcher_;
  }

  void MayBeCacheRealTimeUrlVerdict(
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
    enterprise_rt_service_->MayBeCacheRealTimeUrlVerdict(response);
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

  // Must be the first member to be initialized first and destroyed last.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<policy::ManagementService> management_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  // TestingProfileManager owns `test_profile_` so it must be freed after all
  // reference to the test profile are freed.
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ChromeEnterpriseRealTimeUrlLookupService>
      enterprise_rt_service_;
  std::unique_ptr<VerdictCacheManager> cache_manager_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  raw_ptr<TestSafeBrowsingTokenFetcher> raw_token_fetcher_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  raw_ptr<TestingProfile> test_profile_;
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<MockReferrerChainProvider> referrer_chain_provider_;
};

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_ResponseIsAlreadyCached) {
  GURL url("http://example.test/");
  MayBeCacheRealTimeUrlVerdict(RTLookupResponse::ThreatInfo::DANGEROUS,
                               RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING,
                               60, "example.test/",
                               RTLookupResponse::ThreatInfo::COVERING_MATCH);
  task_environment_.RunUntilIdle();

  base::MockCallback<network::TestURLLoaderFactory::Interceptor>
      request_callback;
  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url, response_callback.Get(), content::GetIOThreadTaskRunner({}),
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);

  test_url_loader_factory_.SetInterceptor(request_callback.Get());
  EXPECT_CALL(request_callback, Run(_)).Times(0);
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ true, _));

  task_environment_.RunUntilIdle();
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_RequestWithDmTokenAndAccessToken) {
  base::HistogramTester histogram_tester;
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  ReferrerChain returned_referrer_chain;
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));

  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url, response_callback.Get(), content::GetIOThreadTaskRunner({}),
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);

  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ true,
                                     /* is_cached_response */ false, _));

  bool request_validated = false;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_EQ("http://example.test/", request_proto.url());
        EXPECT_EQ("dm_token", request_proto.dm_token());
        EXPECT_EQ("test@example.com", request_proto.email());
        EXPECT_EQ("dm_token", request_proto.browser_dm_token());
        EXPECT_TRUE(request_proto.has_client_reporting_metadata());
        EXPECT_EQ(kContentAreaAccountEmail,
                  request_proto.content_area_account_email());
        EXPECT_EQ("", request_proto.profile_dm_token());
        EXPECT_FALSE(request_proto.local_ips().empty());
        EXPECT_EQ(ChromeUserPopulation::SAFE_BROWSING,
                  request_proto.population().user_population());
        EXPECT_TRUE(request_proto.population().is_history_sync_enabled());
        EXPECT_EQ(ChromeUserPopulation::NOT_MANAGED,
                  request_proto.population().profile_management_status());
        EXPECT_TRUE(request_proto.population().is_under_advanced_protection());

        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional(std::string("Bearer access_token_string")));

        request_validated = true;
      }));

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  FulfillAccessTokenRequest("access_token_string");

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(request_validated);

  // Check the response is cached.
  EXPECT_NE(nullptr, GetCachedRealTimeUrlVerdict(url));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.RT.HasAccessTokenFromFetcher", /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.RT.HasAccessTokenFromFetcher.Enterprise", /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.RT.GetToken.TimeTaken",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "SafeBrowsing.RT.GetToken.TimeTaken.Enterprise", /*expected_count=*/1);
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_OnInvalidAccessTokenCalledResponseCodeUnauthorized) {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  test_url_loader_factory_.ClearResponses();
  auto head = network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED);
  network::URLLoaderCompletionStatus status(net::OK);
  test_url_loader_factory_.AddResponse(GURL(kRealTimeLookupUrl),
                                       std::move(head), "", status);

  GURL url("http://example.test/");
  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url, response_callback.Get(), content::GetIOThreadTaskRunner({}),
      SessionID::InvalidValue(), /*referring_app_info=*/std::nullopt);

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  FulfillAccessTokenRequest("invalid_token_string");
  EXPECT_CALL(*raw_token_fetcher(),
              OnInvalidAccessToken("invalid_token_string"))
      .Times(1);

  base::RunLoop run_loop;
  EXPECT_CALL(response_callback, Run(/* is_rt_lookup_successful */ false,
                                     /* is_cached_response */ false, _))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_EnhancedProtectionRequestWithReferringAppInfo) {
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  ReferrerChain returned_referrer_chain;
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));

  internal::ReferringAppInfo referring_app_info;
  referring_app_info.referring_app_name = "foo";
  referring_app_info.referring_webapk_start_url = GURL("https://webapp.test");
  referring_app_info.referring_webapk_manifest_id =
      GURL("https://webapp.test/id");

  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::ENHANCED_PROTECTION);
  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url, response_callback.Get(), content::GetIOThreadTaskRunner({}),
      SessionID::InvalidValue(), referring_app_info);

  EXPECT_CALL(response_callback, Run(/*is_rt_lookup_successful=*/true,
                                     /*is_cached_response=*/false, _));

  bool request_validated = false;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_EQ("http://example.test/", request_proto.url());
        // ReferringAppInfo should be populated for ESB.
        EXPECT_TRUE(request_proto.has_referring_app_info());
        EXPECT_EQ(request_proto.referring_app_info().referring_app_name(),
                  "foo");
        // No WebAPK info for Enterprise RT lookup requests.
        EXPECT_FALSE(request_proto.referring_app_info().has_referring_webapk());
        request_validated = true;
      }));

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  FulfillAccessTokenRequest("access_token_string");

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestStartLookup_StandardProtectionRequestWithNoReferringAppInfo) {
  GURL url("http://example.test/");
  SetUpRTLookupResponse(RTLookupResponse::ThreatInfo::DANGEROUS,
                        RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING, 60,
                        "example.test/",
                        RTLookupResponse::ThreatInfo::COVERING_MATCH);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  ReferrerChain returned_referrer_chain;
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByPendingEventURL(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));

  internal::ReferringAppInfo referring_app_info;
  referring_app_info.referring_app_name = "foo";
  referring_app_info.referring_webapk_start_url = GURL("https://webapp.test");
  referring_app_info.referring_webapk_manifest_id =
      GURL("https://webapp.test/id");

  SetSafeBrowsingState(&test_pref_service_,
                       SafeBrowsingState::STANDARD_PROTECTION);
  base::MockCallback<RTLookupResponseCallback> response_callback;
  enterprise_rt_service()->StartLookup(
      url, response_callback.Get(), content::GetIOThreadTaskRunner({}),
      SessionID::InvalidValue(), referring_app_info);

  EXPECT_CALL(response_callback, Run(/*is_rt_lookup_successful=*/true,
                                     /*is_cached_response=*/false, _));

  bool request_validated = false;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        RTLookupRequest request_proto;
        ASSERT_TRUE(GetRequestProto(request, &request_proto));
        EXPECT_EQ("http://example.test/", request_proto.url());
        // ReferringAppInfo should not be populated for ESB.
        EXPECT_FALSE(request_proto.has_referring_app_info());
        request_validated = true;
      }));

  EXPECT_TRUE(raw_token_fetcher()->WasStartCalled());
  FulfillAccessTokenRequest("access_token_string");

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(request_validated);
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       TestCanCheckSafeBrowsingHighConfidenceAllowlist_BypassAllowlistFeature) {
  test_profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

  // Can check allowlist if `kEnterpriseRealTimeUrlCheckMode` is disabled.
  test_profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);
  EXPECT_TRUE(
      enterprise_rt_service()->CanCheckSafeBrowsingHighConfidenceAllowlist());

  // Bypass allowlist if the `kEnterpriseRealTimeUrlCheckMode` pref is set.
  test_profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  EXPECT_FALSE(
      enterprise_rt_service()->CanCheckSafeBrowsingHighConfidenceAllowlist());
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest, CanPerformFullURLLookup) {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  EXPECT_TRUE(enterprise_rt_service()->CanPerformFullURLLookup());
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       CanPerformFullURLLookup_GuestModeEnabled) {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  auto guest_rt_service = CreateServiceAndEnablePolicy(
      test_profile_, /*is_off_the_record=*/false, /*is_guest_session=*/true);
  EXPECT_TRUE(guest_rt_service->CanPerformFullURLLookup());
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       CanPerformFullURLLookup_OffTheRecordDisabled) {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  auto off_the_record_rt_service =
      CreateServiceAndEnablePolicy(test_profile_, /*is_off_the_record=*/true);
  EXPECT_FALSE(off_the_record_rt_service->CanPerformFullURLLookup());
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest, CanCheckUrl_IPAddresses) {
  EXPECT_TRUE(
      enterprise_rt_service()->CanCheckUrl(GURL("https://google.com/")));
  EXPECT_TRUE(enterprise_rt_service()->CanCheckUrl(GURL("http://192.168.1.1")));
  EXPECT_TRUE(enterprise_rt_service()->CanCheckUrl(GURL("http://172.16.2.2")));
  EXPECT_TRUE(enterprise_rt_service()->CanCheckUrl(GURL("http://127.0.0.1")));
}

TEST_F(ChromeEnterpriseRealTimeUrlLookupServiceTest,
       CheckShouldOverrideKnownSafeUrlDecision) {
  EXPECT_TRUE(enterprise_rt_service()->ShouldOverrideKnownSafeUrlDecision(
      GURL("chrome://flags/")));
  EXPECT_FALSE(enterprise_rt_service()->ShouldOverrideKnownSafeUrlDecision(
      GURL("chrome://newtab/")));
  EXPECT_FALSE(enterprise_rt_service()->ShouldOverrideKnownSafeUrlDecision(
      GURL("http://example.com/")));
}

}  // namespace safe_browsing
