// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/writeable_pref_store.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace enterprise_data_protection {

namespace {

const char* kSkippedUrls[] = {
    "chrome://version",
    "chrome-extension://abcdefghijklmnop",
};

content::Page& GetPageFromWebContents(content::WebContents* web_contents) {
  return web_contents->GetPrimaryMainFrame()->GetPage();
}

safe_browsing::RTLookupResponse::ThreatInfo GetTestThreatInfo(
    std::string watermark_text,
    int64_t timestamp_seconds) {
  safe_browsing::MatchedUrlNavigationRule::WatermarkMessage wm;
  wm.set_watermark_message(watermark_text);
  wm.mutable_timestamp()->set_seconds(timestamp_seconds);

  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  threat_info.set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  safe_browsing::MatchedUrlNavigationRule* matched_url_navigation_rule =
      threat_info.mutable_matched_url_navigation_rule();
  *matched_url_navigation_rule->mutable_watermark_message() = wm;
  return threat_info;
}

safe_browsing::RTLookupResponse CreateWatermarkResponse() {
  safe_browsing::RTLookupResponse response;
  safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
      response.add_threat_info();
  *new_threat_info = GetTestThreatInfo("custom_message", 1709181364);
  return response;
}

class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id) override {
    // Create custom threat info instance. The DataProtectionNavigationObserver
    // does not care whether the verdict came from the verdict cache or from an
    // actual lookup request, as long as it gets a verdict back.
    auto response = std::make_unique<safe_browsing::RTLookupResponse>(
        CreateWatermarkResponse());

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](safe_browsing::RTLookupResponseCallback response_callback,
               base::OnceClosure on_start_lookup_complete,
               bool is_rt_lookup_successful,
               std::unique_ptr<safe_browsing::RTLookupResponse> response) {
              std::move(response_callback)
                  .Run(is_rt_lookup_successful,
                       /*is_cached_response=*/false, std::move(response));
              if (!on_start_lookup_complete.is_null()) {
                std::move(on_start_lookup_complete).Run();
              }
            },
            std::move(response_callback), std::move(on_start_lookup_complete_),
            is_rt_lookup_successful_, std::move(response)));
  }

  void set_on_start_lookup_complete(base::OnceClosure closure) {
    on_start_lookup_complete_ = std::move(closure);
  }

  void set_is_rt_lookup_successful(bool successful) {
    is_rt_lookup_successful_ = successful;
  }

 private:
  base::OnceClosure on_start_lookup_complete_;
  bool is_rt_lookup_successful_ = true;
};

class DataProtectionNavigationObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  Profile* profile() { return Profile::FromBrowserContext(browser_context()); }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm-token"));
    client_ = std::make_unique<policy::MockCloudPolicyClient>();

    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new extensions::SafeBrowsingPrivateEventRouter(context));
            }));
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new enterprise_connectors::RealtimeReportingClient(context));
            }));
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
    identity_test_environment_.MakePrimaryAccountAvailable(
        "test-user@chromium.org", signin::ConsentLevel::kSync);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());

    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile()->GetPrefs(), true);

    // Enable real-time URL checks.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    profile->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
        safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    profile->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);
  }

  void TearDown() override {
    DataProtectionNavigationObserver::SetLookupServiceForTesting(nullptr);
    SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  FakeRealTimeUrlLookupService lookup_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

}  // namespace

TEST_F(DataProtectionNavigationObserverTest, TestWatermarkTextUpdated) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectURLFilteringInterstitialEvent(
      /*url*/ "https://test/",
      /*event_result*/ "EVENT_RESULT_ALLOWED",
      /*profile_user_name*/ "test-user@chromium.org",
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*rt_lookup_response*/ CreateWatermarkResponse());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://test"), web_contents()->GetPrimaryMainFrame());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by BrowserView. So we simply call Start() and manually
  // construct the class using the navigation handle that is provided once
  // Start() is called.
  simulator->Start();
  content::NavigationHandle* navigation_handle =
      simulator->GetNavigationHandle();
  base::test::TestFuture<const UrlSettings&> future;

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  // The DataProtectionNavigationObserver needs to be constructed using
  // CreateForNavigationHandle to allow for proper lifetime management of the
  // object, since we call DeleteForNavigationHandle() in our
  // DidFinishNavigation() override.
  enterprise_data_protection::DataProtectionNavigationObserver::
      CreateForNavigationHandle(*navigation_handle, &lookup_service_,
                                navigation_handle->GetWebContents(),
                                future.GetCallback());
  EXPECT_TRUE(future_lookup_complete.Wait());

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  std::string watermark_text = future.Get().watermark_text;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  EXPECT_EQ(watermark_text,
            "custom_message\n" +
                connectors_service->GetRealTimeUrlCheckIdentifier() +
                "\n2024-02-29T04:36:04.000Z");

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_NE(user_data->settings().watermark_text.find("custom_message"),
            std::string::npos);
}

// An invalid watermark response generates no report.
TEST_F(DataProtectionNavigationObserverTest, InvalidResponse_NoReport) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://test"), web_contents()->GetPrimaryMainFrame());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by BrowserView. So we simply call Start() and manually
  // construct the class using the navigation handle that is provided once
  // Start() is called.
  simulator->Start();
  content::NavigationHandle* navigation_handle =
      simulator->GetNavigationHandle();
  base::test::TestFuture<const UrlSettings&> future;

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_is_rt_lookup_successful(false);
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  // The DataProtectionNavigationObserver needs to be constructed using
  // CreateForNavigationHandle to allow for proper lifetime management of the
  // object, since we call DeleteForNavigationHandle() in our
  // DidFinishNavigation() override.
  enterprise_data_protection::DataProtectionNavigationObserver::
      CreateForNavigationHandle(*navigation_handle, &lookup_service_,
                                navigation_handle->GetWebContents(),
                                future.GetCallback());
  EXPECT_TRUE(future_lookup_complete.Wait());

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  std::string watermark_text = future.Get().watermark_text;
  EXPECT_TRUE(watermark_text.empty());
}

TEST_F(DataProtectionNavigationObserverTest,
       SkipSpecialURLs_CreateForNavigationIfNeeded) {
  SetContents(CreateTestWebContents());

  for (const auto* url : kSkippedUrls) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(url), web_contents());
    simulator->Start();
    content::NavigationHandle* navigation_handle =
        simulator->GetNavigationHandle();

    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
        Profile::FromBrowserContext(browser_context()), navigation_handle,
        future.GetCallback());
    ASSERT_EQ(future.Get(), UrlSettings());
  }
}

TEST_F(DataProtectionNavigationObserverTest,
       SkipSpecialURLs_GetDataProtectionSettings) {
  SetContents(CreateTestWebContents());

  for (const auto* url : kSkippedUrls) {
    NavigateAndCommit(GURL(url));
    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::GetDataProtectionSettings(
        Profile::FromBrowserContext(browser_context()), web_contents(),
        future.GetCallback());
    ASSERT_EQ(future.Get(), UrlSettings());
  }
}

TEST_F(DataProtectionNavigationObserverTest, GetDataProtectionSettings) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);

  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::GetDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());
  EXPECT_NE(future.Get().watermark_text.find("custom_message"),
            std::string::npos);

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_NE(user_data->settings().watermark_text.find("custom_message"),
            std::string::npos);
}

namespace {

struct WatermarkStringParams {
  WatermarkStringParams(std::string identifier,
                        std::string custom_message,
                        int64_t timestamp_seconds,
                        std::string expected)
      : identifier(identifier),
        custom_message(custom_message),
        timestamp_seconds(timestamp_seconds),
        expected(expected) {}

  std::string identifier;
  std::string custom_message;
  int64_t timestamp_seconds;
  std::string expected;
};

class DataProtectionWatermarkStringTest
    : public testing::TestWithParam<WatermarkStringParams> {};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    DataProtectionWatermarkStringTest,
    DataProtectionWatermarkStringTest,
    testing::Values(
        WatermarkStringParams(
            "example@email.com",
            "custom_message",
            1709181364,
            "custom_message\nexample@email.com\n2024-02-29T04:36:04.000Z"),
        WatermarkStringParams(
            "<device-id>",
            "custom_message",
            1709181364,
            "custom_message\n<device-id>\n2024-02-29T04:36:04.000Z"),
        WatermarkStringParams("example@email.com",
                              "",
                              1709181364,
                              "example@email.com\n2024-02-29T04:36:04.000Z")));

TEST_P(DataProtectionWatermarkStringTest,
       TestGetWatermarkStringFromThreatInfo) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info = GetTestThreatInfo(
      GetParam().custom_message, GetParam().timestamp_seconds);
  EXPECT_EQ(enterprise_data_protection::GetWatermarkString(
                GetParam().identifier, threat_info),
            GetParam().expected);
}

}  // namespace enterprise_data_protection
