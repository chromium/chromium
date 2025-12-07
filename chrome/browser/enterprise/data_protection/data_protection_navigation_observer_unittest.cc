// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/writeable_pref_store.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace enterprise_data_protection {

namespace {

constexpr const char* kSkippedUrls[] = {
    "chrome://version",
    "chrome-extension://abcdefghijklmnop",
};

content::Page& GetPageFromWebContents(content::WebContents* web_contents) {
  return web_contents->GetPrimaryMainFrame()->GetPage();
}

chrome::cros::reporting::proto::TriggeredRuleInfo MakeTriggeredRuleInfo(
    bool has_watermark) {
  chrome::cros::reporting::proto::TriggeredRuleInfo info;
  info.set_action(
      chrome::cros::reporting::proto::TriggeredRuleInfo::REPORT_ONLY);
  info.set_rule_id(123);
  info.set_rule_name("watermark rule");
  if (has_watermark) {
    info.set_has_watermarking(true);
  }
  return info;
}

safe_browsing::RTLookupResponse::ThreatInfo GetTestThreatInfo(
    std::optional<std::string> watermark_text,
    int64_t timestamp_seconds,
    bool has_matched_rule = false) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  threat_info.set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  if (has_matched_rule || watermark_text.has_value()) {
    *threat_info.mutable_matched_url_navigation_rule()->mutable_rule_id() =
        "123";
    *threat_info.mutable_matched_url_navigation_rule()->mutable_rule_name() =
        "watermark rule";
  }
  if (watermark_text.has_value()) {
    safe_browsing::MatchedUrlNavigationRule::WatermarkMessage wm;
    wm.set_watermark_message(*watermark_text);
    wm.mutable_timestamp()->set_seconds(timestamp_seconds);
    *threat_info.mutable_matched_url_navigation_rule()
         ->mutable_watermark_message() = wm;
  }

  return threat_info;
}

safe_browsing::RTLookupResponse CreateRTLookupResponse(
    std::optional<std::string> watermark_text,
    bool has_matched_rule) {
  safe_browsing::RTLookupResponse response;
  safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
      response.add_threat_info();
  *new_threat_info = GetTestThreatInfo(std::move(watermark_text), 1709181364,
                                       has_matched_rule);
  return response;
}

void OnRealtimeLookupComplete(
    safe_browsing::RTLookupResponseCallback response_callback,
    base::OnceClosure on_start_lookup_complete,
    bool is_rt_lookup_successful,
    std::unique_ptr<safe_browsing::RTLookupResponse> response) {
  std::move(response_callback)
      .Run(is_rt_lookup_successful,
           /*is_cached_response=*/false, std::move(response));
  if (!on_start_lookup_complete.is_null()) {
    std::move(on_start_lookup_complete).Run();
  }
}

class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    // Create custom threat info instance. The DataProtectionNavigationObserver
    // does not care whether the verdict came from the verdict cache or from an
    // actual lookup request, as long as it gets a verdict back.
    std::optional<std::string> watermark_text;
    if (url_to_watermark_.count(url) > 0) {
      watermark_text = url_to_watermark_[url];
    } else {
      watermark_text = "custom_message";
    }

    auto response = std::make_unique<safe_browsing::RTLookupResponse>(
        CreateRTLookupResponse(std::move(watermark_text),
                               should_have_matched_rule_));

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&OnRealtimeLookupComplete, std::move(response_callback),
                       std::move(on_start_lookup_complete_),
                       is_rt_lookup_successful_, std::move(response)));
  }

  void set_on_start_lookup_complete(base::OnceClosure closure) {
    on_start_lookup_complete_ = std::move(closure);
  }

  void set_is_rt_lookup_successful(bool successful) {
    is_rt_lookup_successful_ = successful;
  }

  void SetWatermarkTextForURL(const GURL& url,
                              std::optional<std::string> watermark_text) {
    url_to_watermark_[url] = std::move(watermark_text);
  }

  void SetShouldHaveMatchedRule(bool should_have_matched_rule) {
    should_have_matched_rule_ = should_have_matched_rule;
  }

 private:
  base::OnceClosure on_start_lookup_complete_;
  bool is_rt_lookup_successful_ = true;
  std::map<GURL, std::optional<std::string>> url_to_watermark_;
  bool should_have_matched_rule_ = false;
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
        "test-user@chromium.org", signin::ConsentLevel::kSignin);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());

    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile()->GetPrefs(), true);

    // Enable real-time URL checks.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    profile->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
    profile->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
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

class FakeDataProtectionNavigationController
    : public DataProtectionNavigationDelegate,
      public content::WebContentsObserver {
 public:
  FakeDataProtectionNavigationController(
      content::WebContents* web_contents,
      safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
      DataProtectionNavigationObserver::Callback callback)
      : content::WebContentsObserver(web_contents),
        lookup_service_(lookup_service),
        callback_(std::move(callback)) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    EXPECT_EQ(web_contents(), navigation_handle->GetWebContents());
    auto navigation_observer =
        std::make_unique<DataProtectionNavigationObserver>(
            *navigation_handle, lookup_service_, web_contents(), this,
            std::move(callback_));

    navigation_observers_.emplace(navigation_handle->GetNavigationId(),
                                  std::move(navigation_observer));
  }

  void Cleanup(int64_t navigation_id) override {
    navigation_observers_.erase(navigation_observers_.find(navigation_id));
  }

 private:
  raw_ptr<safe_browsing::RealTimeUrlLookupServiceBase> lookup_service_;
  DataProtectionNavigationObserver::Callback callback_;
  DataProtectionNavigationObserver::NavigationObservers navigation_observers_;
};

TEST_F(DataProtectionNavigationObserverTest, MatchedAuditRuleHasEvent) {
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://example.com/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
  expected_event.set_profile_user_name("test-user@chromium.org");
  expected_event.set_profile_identifier(profile()->GetPath().AsUTF8Unsafe());
  *expected_event.add_triggered_rule_info() =
      MakeTriggeredRuleInfo(/*has_watermark=*/false);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  lookup_service_.SetShouldHaveMatchedRule(true);
  lookup_service_.SetWatermarkTextForURL(GURL("https://example.com/"),
                                         std::nullopt);
  lookup_service_.SetWatermarkTextForURL(GURL("https://redirect.com/"),
                                         std::nullopt);

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/"), web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by DataProtectionNavigationController. So we simply call
  // Start() and manually construct the class using the navigation handle that
  // is provided once Start() is called.
  simulator->Start();

  EXPECT_TRUE(future_lookup_complete.Wait());

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  run_loop.Run();
}

TEST_F(DataProtectionNavigationObserverTest,
       TestWatermarkTextUpdated_NoUrlCheck) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://test"), web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by BrowserView. So we simply call Start() and manually
  // construct the class using the navigation handle that is provided once
  // Start() is called.
  simulator->Start();

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  std::string watermark_text = future.Get().watermark_text;
  EXPECT_TRUE(watermark_text.empty());

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_TRUE(user_data->settings().watermark_text.empty());
}

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_DataControls) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["example.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  // The screenshot protection comes from data controls and not the lookup,
  // even when the lookup fails.
  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_is_rt_lookup_successful(false);
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by DataProtectionNavigationController. So we simply call
  // Start() and manually construct the class using the navigation handle that
  // is provided once Start() is called.
  simulator->Start();
  EXPECT_TRUE(future_lookup_complete.Wait());

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  EXPECT_FALSE(future.Get().allow_screenshots);
  EXPECT_TRUE(future.Get().watermark_text.empty());

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_DataControls_NoUrlCheck) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["example.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), web_contents()->GetPrimaryMainFrame());
  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by DataProtectionNavigationController. So we simply call
  // Start() and manually construct the class using the navigation handle that
  // is provided once Start() is called.
  simulator->Start();

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  EXPECT_FALSE(future.Get().allow_screenshots);
  EXPECT_TRUE(future.Get().watermark_text.empty());

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
}

// An invalid watermark response generates no report.
TEST_F(DataProtectionNavigationObserverTest, InvalidResponse_NoReport) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://test"), web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_is_rt_lookup_successful(false);
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by DataProtectionNavigationController. So we simply call
  // Start() and manually construct the class using the navigation handle that
  // is provided once Start() is called.
  simulator->Start();
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
    base::test::TestFuture<const UrlSettings&> future;
    FakeDataProtectionNavigationController controller(
        web_contents(), &lookup_service_, future.GetCallback());
    simulator->Start();
    auto navigation_observer =
        DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
            &controller, Profile::FromBrowserContext(browser_context()),
            simulator->GetNavigationHandle(), future.GetCallback());
    ASSERT_EQ(navigation_observer, nullptr);
    ASSERT_EQ(future.Get(), UrlSettings());
  }
}

TEST_F(DataProtectionNavigationObserverTest,
       SkipSpecialURLs_ApplyDataProtectionSettings) {
  SetContents(CreateTestWebContents());

  for (const auto* url : kSkippedUrls) {
    NavigateAndCommit(GURL(url));
    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::ApplyDataProtectionSettings(
        Profile::FromBrowserContext(browser_context()), web_contents(),
        future.GetCallback());
    ASSERT_EQ(future.Get(), UrlSettings());
  }
}

TEST_F(DataProtectionNavigationObserverTest,
       SubframeNavigation_DoesNotUpdateDataProtectionState) {
  // Data Protection state should not be updated during background navigations.
  // These can take place in various cases:
  // 1. Chrome has some optimizations to start navigations while the URL is
  // being typed in the omnibox.
  // 2. Sub-navigations to URLs that are different from the main omnibox URL.
  // Since the URL could be different, it could negate the verdict reached using
  // the main frame URL.
  SetContents(CreateTestWebContents());

  // Set up verdicts for main-frame and sub-frame URLs.
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);
  lookup_service_.SetWatermarkTextForURL(GURL("https://example.com/"),
                                         "custom_message");
  lookup_service_.SetWatermarkTextForURL(
      GURL("https://background-navigation.com/"), std::nullopt);

  // Navigate to main-frame URL and check verdict.
  NavigateAndCommit(GURL("https://example.com"));
  {
    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::ApplyDataProtectionSettings(
        Profile::FromBrowserContext(browser_context()), web_contents(),
        future.GetCallback());
    EXPECT_NE(future.Get().watermark_text.find("custom_message"),
              std::string::npos);
  }

  // Navigate to sub-frame URL and verify that watermark string is unchanged.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://background-navigation.com/"), subframe);
  simulator->SetInitiatorFrame(main_rfh());
  simulator->Commit();
  {
    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::ApplyDataProtectionSettings(
        Profile::FromBrowserContext(browser_context()), web_contents(),
        future.GetCallback());
    EXPECT_NE(future.Get().watermark_text.find("custom_message"),
              std::string::npos);
  }
}

TEST_F(DataProtectionNavigationObserverTest, ApplyDataProtectionSettings) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);

  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());
  EXPECT_NE(future.Get().watermark_text.find("custom_message"),
            std::string::npos);
  EXPECT_TRUE(future.Get().allow_screenshots);  // Default is true.

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       ApplyDataProtectionSettings_NoUrlCheck) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);

  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());
  EXPECT_TRUE(future.Get().watermark_text.empty());
  EXPECT_TRUE(future.Get().allow_screenshots);  // Default is true.

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
  EXPECT_TRUE(user_data->settings().watermark_text.empty());
  EXPECT_TRUE(user_data->settings().allow_screenshots);
}

TEST_F(DataProtectionNavigationObserverTest,
       ApplyDataProtectionSettings_DC_BlockScreenshot) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["example.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());
  EXPECT_NE(future.Get().watermark_text.find("custom_message"),
            std::string::npos);
  EXPECT_FALSE(future.Get().allow_screenshots);

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       ApplyDataProtectionSettings_DC_BlockScreenshot_NoUrlCheck) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["example.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());
  EXPECT_TRUE(future.Get().watermark_text.empty());
  EXPECT_FALSE(future.Get().allow_screenshots);

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       ApplyDataProtectionSettings_DC_BlockScreenshot_Redirect) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["redirect.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  lookup_service_.SetWatermarkTextForURL(GURL("https://example.com"),
                                         std::nullopt);
  lookup_service_.SetWatermarkTextForURL(GURL("https://redirect.com"),
                                         std::nullopt);

  SetContents(CreateTestWebContents());
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), web_contents()->GetPrimaryMainFrame());
  base::test::TestFuture<const UrlSettings&> navigation_future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, navigation_future.GetCallback());

  const GURL kRedirectUrl = GURL("https://redirect.com");

  // Do initial navigation request which allows screenshots.
  {
    base::test::TestFuture<void> future_lookup_complete;
    lookup_service_.set_on_start_lookup_complete(
        future_lookup_complete.GetCallback());
    simulator->Start();
    EXPECT_TRUE(future_lookup_complete.Wait());
  }

  // Redirect to a URL that should not allow screenshots.
  {
    base::test::TestFuture<void> future_lookup_complete;
    lookup_service_.set_on_start_lookup_complete(
        future_lookup_complete.GetCallback());
    simulator->Redirect(kRedirectUrl);
    EXPECT_TRUE(future_lookup_complete.Wait());
  }

  simulator->Commit();
  EXPECT_TRUE(navigation_future.Wait());

  // The result of the above should be that
  // screenshots are not allowed.
  base::test::TestFuture<const UrlSettings&> get_settings_future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      get_settings_future.GetCallback());
  EXPECT_FALSE(get_settings_future.Get().allow_screenshots);

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), get_settings_future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       ApplyDataProtectionSettings_DC_BlockScreenshot_RedirectWithoutUrlCheck) {
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["redirect.com"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  SetContents(CreateTestWebContents());
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), web_contents()->GetPrimaryMainFrame());
  base::test::TestFuture<const UrlSettings&> navigation_future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, navigation_future.GetCallback());
  const GURL kRedirectUrl = GURL("https://redirect.com");
  simulator->Start();

  // Redirect to a URL that should not allow screenshots.
  simulator->Redirect(kRedirectUrl);

  simulator->Commit();
  EXPECT_TRUE(navigation_future.Wait());

  // The result of the above should be that
  // screenshots are not allowed.
  base::test::TestFuture<const UrlSettings&> get_settings_future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      get_settings_future.GetCallback());
  EXPECT_FALSE(get_settings_future.Get().allow_screenshots);

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings(), get_settings_future.Get());
}

TEST_F(DataProtectionNavigationObserverTest,
       WatermarkWebUI_CreateForNavigationIfNeeded) {
  SetContents(CreateTestWebContents());

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(chrome::kChromeUIWatermarkURL), web_contents());
  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());
  simulator->Start();
  auto navigation_observer =
      DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
          &controller, Profile::FromBrowserContext(browser_context()),
          simulator->GetNavigationHandle(), future.GetCallback());

  // The observer should be null since the callback is invoked directly.
  ASSERT_EQ(navigation_observer, nullptr);

  // The settings should contain the default watermark text.
  const UrlSettings& settings = future.Get();
  EXPECT_EQ(settings.watermark_text, "Watermark Test Page");
  EXPECT_TRUE(settings.allow_screenshots);
}

TEST_F(DataProtectionNavigationObserverTest,
       WatermarkWebUI_ApplyDataProtectionSettings) {
  SetContents(CreateTestWebContents());

  NavigateAndCommit(GURL(chrome::kChromeUIWatermarkURL));
  base::test::TestFuture<const UrlSettings&> future;
  DataProtectionNavigationObserver::ApplyDataProtectionSettings(
      Profile::FromBrowserContext(browser_context()), web_contents(),
      future.GetCallback());

  // The settings should contain the default watermark text.
  const UrlSettings& settings = future.Get();
  EXPECT_EQ(settings.watermark_text, "Watermark Test Page");
  EXPECT_TRUE(settings.allow_screenshots);
}

TEST_F(DataProtectionNavigationObserverTest, TestVerdictCacheMaxSizeFlag) {
  EXPECT_EQ(200UL, DataProtectionNavigationObserver::GetVerdictCacheMaxSize());

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        enterprise_data_protection::kEnableVerdictCache,
        {{enterprise_data_protection::kVerdictCacheMaxSize.name,
          base::ToString(0)}});
    // Falls back to default value when set to an invalid value.
    EXPECT_EQ(200UL,
              DataProtectionNavigationObserver::GetVerdictCacheMaxSize());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        enterprise_data_protection::kEnableVerdictCache,
        {{enterprise_data_protection::kVerdictCacheMaxSize.name,
          base::ToString(500UL)}});
    EXPECT_EQ(500UL,
              DataProtectionNavigationObserver::GetVerdictCacheMaxSize());
  }
}

namespace {

struct WatermarkStringParams {
  WatermarkStringParams(std::string identifier,
                        std::optional<std::string> custom_message,
                        int64_t timestamp_seconds,
                        std::string expected)
      : identifier(identifier),
        custom_message(std::move(custom_message)),
        timestamp_seconds(timestamp_seconds),
        expected(expected) {}

  std::string identifier;
  std::optional<std::string> custom_message;
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
                              "example@email.com\n2024-02-29T04:36:04.000Z"),
        WatermarkStringParams("example@email.com",
                              std::nullopt,
                              1709181364,
                              "")));

TEST_P(DataProtectionWatermarkStringTest,
       TestGetWatermarkStringFromThreatInfo) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info =
      GetTestThreatInfo(GetParam().custom_message, GetParam().timestamp_seconds,
                        GetParam().custom_message.has_value());
  EXPECT_EQ(
      enterprise_data_protection::GetWatermarkString(
          GetParam().identifier, threat_info.matched_url_navigation_rule()),
      GetParam().expected);
}

class SinglePageAppWatermarkTest : public DataProtectionNavigationObserverTest,
                                   public testing::WithParamInterface<bool> {};

class SameDocumentNavigationWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit SameDocumentNavigationWebContentsObserver(
      bool is_single_page_app_enabled,
      content::WebContents* web_contents,
      FakeRealTimeUrlLookupService* lookup_service,
      content::BrowserContext* browser_context)
      : content::WebContentsObserver(web_contents),
        is_single_page_app_enabled_(is_single_page_app_enabled),
        lookup_service_(lookup_service),
        browser_context_(browser_context) {}

  MOCK_METHOD(void,
              DidFinishNavigation,
              (content::NavigationHandle*),
              (override));

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    base::test::TestFuture<const UrlSettings&> future;

    FakeDataProtectionNavigationController controller(
        web_contents(), lookup_service_, future.GetCallback());

    auto navigation_observer =
        DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
            &controller, Profile::FromBrowserContext(browser_context_),
            navigation_handle, future.GetCallback());

    ASSERT_EQ(navigation_observer != nullptr, is_single_page_app_enabled_);
  }

 private:
  bool is_single_page_app_enabled_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<FakeRealTimeUrlLookupService> lookup_service_;
  raw_ptr<content::BrowserContext> browser_context_;
};

TEST_P(SinglePageAppWatermarkTest,
       CheckSameDocumentNavigation_CreateForNavigationIfNeeded) {
  base::test::ScopedFeatureList scoped_feature_list;
  bool is_single_page_app_enabled = GetParam();
  if (is_single_page_app_enabled) {
    scoped_feature_list.InitAndEnableFeature(
        enterprise_data_protection::kEnableSinglePageAppDataProtection);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        enterprise_data_protection::kEnableSinglePageAppDataProtection);
  }
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));
  SameDocumentNavigationWebContentsObserver observer(
      is_single_page_app_enabled, web_contents(), &lookup_service_,
      browser_context());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com#fragment"), main_rfh());

  // Ensure that the navigation callbacks are invoked, since the assertion is
  // outside the test body. If DidFinishNavigation() was called, then it is
  // guaranteed that DidStartNavigation() was called prior, thereby checking the
  // same document assertion.
  EXPECT_CALL(observer, DidFinishNavigation);
  simulator->CommitSameDocument();
}

INSTANTIATE_TEST_SUITE_P(SinglePageAppWatermarkTest,
                         SinglePageAppWatermarkTest,
                         testing::Bool());

class OrderedDataProtectionNavigationObserverTest
    : public DataProtectionNavigationObserverTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsNavigationFinishedAfterVerdictReceived() const { return GetParam(); }
};

TEST_P(OrderedDataProtectionNavigationObserverTest, TestWatermarkTextUpdated) {
  chrome::cros::reporting::proto::UrlFilteringInterstitialEvent expected_event;
  expected_event.set_url("https://test/");
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EVENT_RESULT_ALLOWED);
  expected_event.set_profile_user_name("test-user@chromium.org");
  expected_event.set_profile_identifier(profile()->GetPath().AsUTF8Unsafe());
  *expected_event.add_triggered_rule_info() =
      MakeTriggeredRuleInfo(/*has_watermark=*/true);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectURLFilteringInterstitialEvent(expected_event);

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://test"), web_contents()->GetPrimaryMainFrame());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by DataProtectionNavigationController. So we simply call
  // Start() and manually construct the class using the navigation handle that
  // is provided once Start() is called.
  simulator->Start();
  if (IsNavigationFinishedAfterVerdictReceived()) {
    EXPECT_TRUE(future_lookup_complete.Wait());
    simulator->Commit();
  } else {
    simulator->Commit();
    EXPECT_TRUE(future_lookup_complete.Wait());
  }

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
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(OrderedDataProtectionNavigationObserverTest,
                         OrderedDataProtectionNavigationObserverTest,
                         testing::Bool());

}  // namespace enterprise_data_protection
