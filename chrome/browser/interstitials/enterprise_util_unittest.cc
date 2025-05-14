// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/connectors/core/features.h"
#endif

using ::testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

class MockSafeBrowsingNavigationObserverManager
    : public safe_browsing::SafeBrowsingNavigationObserverManager {
 public:
  explicit MockSafeBrowsingNavigationObserverManager(
      PrefService* pref_service,
      content::ServiceWorkerContext* context)
      : safe_browsing::SafeBrowsingNavigationObserverManager(pref_service,
                                                             context),
        notification_context_for_removal_(context) {}

  ~MockSafeBrowsingNavigationObserverManager() override {
    if (notification_context_for_removal_) {
      notification_context_for_removal_->RemoveObserver(this);
      ui::Clipboard::GetForCurrentThread()->RemoveObserver(this);
    }
  }

  MOCK_METHOD4(IdentifyReferrerChainByEventURL,
               safe_browsing::ReferrerChainProvider::AttributionResult(
                   const GURL& event_url,
                   SessionID event_tab_id,
                   int user_gesture_count_limit,
                   safe_browsing::ReferrerChain* out_referrer_chain));

  MOCK_METHOD3(
      IdentifyReferrerChainByPendingEventURL,
      AttributionResult(const GURL& event_url,
                        int user_gesture_count_limit,
                        safe_browsing::ReferrerChain* out_referrer_chain));

 private:
  raw_ptr<content::ServiceWorkerContext> notification_context_for_removal_;
};

class InterstitialEnterpriseUtilTest : public testing::Test {
 public:
  InterstitialEnterpriseUtilTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  std::unique_ptr<KeyedService> BuildMockNavigationObserverManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    PrefService* pref_service = profile->GetPrefs();
    CHECK(pref_service);
    content::ServiceWorkerContext* service_worker_context =
        profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
    CHECK(service_worker_context);
    auto mock_manager =
        std::make_unique<MockSafeBrowsingNavigationObserverManager>(
            pref_service, service_worker_context);
    navigation_observer_manager_ = mock_manager.get();
    return mock_manager;
  }

  void SetUp() override {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
  }

  void EnableReportingPolicy(Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile, base::BindRepeating(
                         &safe_browsing::BuildSafeBrowsingPrivateEventRouter));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(SAFE_BROWSING_AVAILABLE)
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile, base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new enterprise_connectors::RealtimeReportingClient(context));
            }));
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile->GetPrefs(), /*enabled=*/true, /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/{});
    auto* navigation_observer_manager_factory = safe_browsing::
        SafeBrowsingNavigationObserverManagerFactory::GetInstance();
    navigation_observer_manager_factory->SetTestingFactory(
        profile,
        base::BindRepeating(
            &InterstitialEnterpriseUtilTest::BuildMockNavigationObserverManager,
            base::Unretained(this)));

    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile)
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
  }

  void ValidateReferrerChain(const base::Value::Dict& report_dict,
                             std::string_view event_name) {
    const base::Value::List* events_list = report_dict.FindList("events");
    ASSERT_NE(events_list, nullptr);
    ASSERT_EQ(events_list->size(), 1u);
    const base::Value::Dict* event_dict = (*events_list)[0].GetIfDict();
    ASSERT_NE(event_dict, nullptr);
    const base::Value::Dict* intertitial_event =
        event_dict->FindDict(event_name);
    ASSERT_NE(intertitial_event, nullptr);
    const base::Value::List* referrer_chain =
        intertitial_event->FindList("referrers");
    EXPECT_EQ(referrer_chain->size(), 1u);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<MockSafeBrowsingNavigationObserverManager>
      navigation_observer_manager_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InterstitialEnterpriseUtilTest, RouterEventDisabledInIncognitoMode) {
  std::vector<base::test::FeatureRef> enable_features;
#if BUILDFLAG(IS_ANDROID)
  enable_features.push_back(
      enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid);
#endif
  enable_features.push_back(safe_browsing::kEnhancedFieldsForSecOps);
  scoped_feature_list_.InitWithFeatures(enable_features, {});
  Profile* incognito_profile =
      profile_manager_.CreateTestingProfile("testing_profile")
          ->GetPrimaryOTRProfile(
              /*create_if_needed=*/true);
  EnableReportingPolicy(incognito_profile);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  MaybeTriggerSecurityInterstitialShownEvent(
      web_contents_factory_.CreateWebContents(incognito_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
}

TEST_F(InterstitialEnterpriseUtilTest,
       SecurityInterstitialShownEventSentInGuestMode) {
  std::vector<base::test::FeatureRef> enable_features;
#if BUILDFLAG(IS_ANDROID)
  enable_features.push_back(
      enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid);
#endif
  enable_features.push_back(safe_browsing::kEnhancedFieldsForSecOps);
  scoped_feature_list_.InitWithFeatures(enable_features, {});
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);

  ASSERT_TRUE(safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
                  GetForBrowserContext(guest_profile));
  safe_browsing::ReferrerChain expected_referrer_chain;
  expected_referrer_chain.Add(
      enterprise_connectors::test::MakeReferrerChainEntry());
  EXPECT_CALL(
      *navigation_observer_manager_,
      IdentifyReferrerChainByPendingEventURL(GURL("https://phishing.com/"),
                                             /*user_gesture_count_limit=*/5, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_referrer_chain),
                      Return(safe_browsing::ReferrerChainProvider::SUCCESS)));
  base::RunLoop run_loop;
  base::Value::Dict report_dict;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            report_dict = std::move(report);
            run_loop.Quit();
          }));

  MaybeTriggerSecurityInterstitialShownEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
  run_loop.Run();
  ValidateReferrerChain(report_dict, "interstitialEvent");
}

TEST_F(InterstitialEnterpriseUtilTest,
       SecurityInterstitialProceededEventSentInGuestMode) {
  std::vector<base::test::FeatureRef> enable_features;
#if BUILDFLAG(IS_ANDROID)
  enable_features.push_back(
      enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid);
#endif
  enable_features.push_back(safe_browsing::kEnhancedFieldsForSecOps);
  scoped_feature_list_.InitWithFeatures(enable_features, {});
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);

  ASSERT_TRUE(safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
                  GetForBrowserContext(guest_profile));
  safe_browsing::ReferrerChain expected_referrer_chain;
  expected_referrer_chain.Add(
      enterprise_connectors::test::MakeReferrerChainEntry());
  EXPECT_CALL(
      *navigation_observer_manager_,
      IdentifyReferrerChainByPendingEventURL(GURL("https://phishing.com/"),
                                             /*user_gesture_count_limit=*/5, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_referrer_chain),
                      Return(safe_browsing::ReferrerChainProvider::SUCCESS)));
  base::RunLoop run_loop;
  base::Value::Dict report_dict;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            report_dict = std::move(report);
            run_loop.Quit();
          }));

  MaybeTriggerSecurityInterstitialProceededEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
  run_loop.Run();
  ValidateReferrerChain(report_dict, "interstitialEvent");
}

TEST_F(InterstitialEnterpriseUtilTest,
       UrlFilteringInterstitialEventSentInGuestMode) {
  scoped_feature_list_.InitWithFeatures(
      {safe_browsing::kEnhancedFieldsForSecOps}, {});
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);

  ASSERT_TRUE(safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
                  GetForBrowserContext(guest_profile));
  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  safe_browsing::ReferrerChain expected_referrer_chain;
  expected_referrer_chain.Add(
      enterprise_connectors::test::MakeReferrerChainEntry());
  EXPECT_CALL(
      *navigation_observer_manager_,
      IdentifyReferrerChainByPendingEventURL(GURL("https://phishing.com/"),
                                             /*user_gesture_count_limit=*/5, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_referrer_chain),
                      Return(safe_browsing::ReferrerChainProvider::SUCCESS)));
  base::RunLoop run_loop;
  base::Value::Dict report_dict;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            report_dict = std::move(report);
            run_loop.Quit();
          }));

  MaybeTriggerUrlFilteringInterstitialEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "ENTERPRISE_WARNED_SEEN", response);
  run_loop.Run();
  ValidateReferrerChain(report_dict, "urlFilteringInterstitialEvent");
}

TEST_F(InterstitialEnterpriseUtilTest, ReferrerChainFallsbackToEventUrl) {
  scoped_feature_list_.InitWithFeatures(
      {safe_browsing::kEnhancedFieldsForSecOps}, {});
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);

  ASSERT_TRUE(safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
                  GetForBrowserContext(guest_profile));
  safe_browsing::RTLookupResponse response;
  auto* threat_info = response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  auto* matched_url_navigation_rule =
      threat_info->mutable_matched_url_navigation_rule();
  matched_url_navigation_rule->set_rule_id("123");
  matched_url_navigation_rule->set_rule_name("test rule name");
  matched_url_navigation_rule->set_matched_url_category("test rule category");
  safe_browsing::ReferrerChain expected_referrer_chain;
  expected_referrer_chain.Add(
      enterprise_connectors::test::MakeReferrerChainEntry());
  EXPECT_CALL(
      *navigation_observer_manager_,
      IdentifyReferrerChainByPendingEventURL(GURL("https://phishing.com/"),
                                             /*user_gesture_count_limit=*/5, _))
      .WillOnce(Return(
          safe_browsing::ReferrerChainProvider::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_CALL(
      *navigation_observer_manager_,
      IdentifyReferrerChainByEventURL(GURL("https://phishing.com/"), _,
                                      /*user_gesture_count_limit=*/5, _))
      .WillOnce(DoAll(SetArgPointee<3>(expected_referrer_chain),
                      Return(safe_browsing::ReferrerChainProvider::SUCCESS)));
  base::RunLoop run_loop;
  base::Value::Dict report_dict;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](bool include_device_info, base::Value::Dict&& report,
              policy::CloudPolicyClient::ResultCallback callback) {
            report_dict = std::move(report);
            run_loop.Quit();
          }));

  MaybeTriggerUrlFilteringInterstitialEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "ENTERPRISE_WARNED_SEEN", response);
  run_loop.Run();
  ValidateReferrerChain(report_dict, "urlFilteringInterstitialEvent");
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(InterstitialEnterpriseUtilTest,
       RouterEventEnabledInGuestMode_NoEventReportedWhenExperimentOff) {
  scoped_feature_list_.InitWithFeatures(
      {}, {enterprise_connectors::kEnterpriseSecurityEventReportingOnAndroid});
  Profile* guest_profile =
      profile_manager_.CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EnableReportingPolicy(guest_profile);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  MaybeTriggerSecurityInterstitialShownEvent(
      web_contents_factory_.CreateWebContents(guest_profile),
      GURL("https://phishing.com/"), "reason",
      /*net_error_code=*/0);
}
#endif
