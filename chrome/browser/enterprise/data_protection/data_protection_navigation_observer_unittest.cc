// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_logging_settings.h"
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
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/enterprise/data_protection/features.h"
#include "components/enterprise/data_protection/utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/writeable_pref_store.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace enterprise_data_protection {

namespace {

constexpr const char* kInternalUrls[] = {
    "chrome://version",
    "chrome-extension://abcdefghijklmnop",
    "chrome-native://newtab",
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
    bool has_matched_rule = false,
    bool block_screenshot = false) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  threat_info.set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  if (has_matched_rule || watermark_text.has_value()) {
    *threat_info.mutable_matched_url_navigation_rule()->mutable_rule_id() =
        "123";
    *threat_info.mutable_matched_url_navigation_rule()->mutable_rule_name() =
        "watermark rule";
    threat_info.mutable_matched_url_navigation_rule()->set_block_screenshot(
        block_screenshot);
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
    bool has_matched_rule,
    bool block_screenshot) {
  safe_browsing::RTLookupResponse response;
  safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
      response.add_threat_info();
  *new_threat_info = GetTestThreatInfo(std::move(watermark_text), 1709181364,
                                       has_matched_rule, block_screenshot);
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
    if (url_to_watermark_.contains(url)) {
      watermark_text = url_to_watermark_[url];
    } else {
      watermark_text = "custom_message";
    }

    bool block_screenshot = should_block_screenshot_;
    if (url_to_block_screenshot_.contains(url)) {
      block_screenshot = url_to_block_screenshot_[url];
    }

    auto response = std::make_unique<safe_browsing::RTLookupResponse>(
        CreateRTLookupResponse(std::move(watermark_text),
                               should_have_matched_rule_, block_screenshot));

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

  void set_should_block_screenshot(bool should_block_screenshot) {
    should_block_screenshot_ = should_block_screenshot;
  }

  void SetWatermarkTextForURL(const GURL& url,
                              std::optional<std::string> watermark_text) {
    url_to_watermark_[url] = std::move(watermark_text);
  }

  void SetBlockScreenshotForURL(const GURL& url, bool block_screenshot) {
    url_to_block_screenshot_[url] = block_screenshot;
  }

  void SetShouldHaveMatchedRule(bool should_have_matched_rule) {
    should_have_matched_rule_ = should_have_matched_rule;
  }

 private:
  base::OnceClosure on_start_lookup_complete_;
  bool is_rt_lookup_successful_ = true;
  std::map<GURL, std::optional<std::string>> url_to_watermark_;
  std::map<GURL, bool> url_to_block_screenshot_;
  bool should_have_matched_rule_ = false;
  bool should_block_screenshot_ = false;
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
            base::BindRepeating(
                &enterprise_connectors::test::BuildRealtimeReportingClient));
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
      base::RepeatingCallback<void(const UrlSettings&)> callback)
      : content::WebContentsObserver(web_contents),
        lookup_service_(lookup_service),
        repeating_callback_(std::move(callback)) {}

  FakeDataProtectionNavigationController(
      content::WebContents* web_contents,
      safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
      DataProtectionNavigationObserver::Callback callback)
      : content::WebContentsObserver(web_contents),
        lookup_service_(lookup_service),
        once_callback_(std::move(callback)) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Actual controller only instantiates observer for primary main
    // navigations.
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    EXPECT_EQ(web_contents(), navigation_handle->GetWebContents());
    DataProtectionNavigationObserver::Callback callback;
    if (repeating_callback_) {
      callback = base::BindOnce(repeating_callback_);
    } else if (once_callback_) {
      callback = std::move(once_callback_);
    }
    auto navigation_observer =
        std::make_unique<DataProtectionNavigationObserver>(
            *navigation_handle, lookup_service_, web_contents(), this,
            std::move(callback));

    navigation_observers_.emplace(navigation_handle->GetNavigationId(),
                                  std::move(navigation_observer));
  }

  void Cleanup(int64_t navigation_id) override {
    navigation_observers_.erase(navigation_observers_.find(navigation_id));
  }

 private:
  raw_ptr<safe_browsing::RealTimeUrlLookupServiceBase> lookup_service_;
  base::RepeatingCallback<void(const UrlSettings&)> repeating_callback_;
  DataProtectionNavigationObserver::Callback once_callback_;
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

  validator.ExpectUrlFilteringInterstitialEvent(expected_event);

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

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_DataControls_LateVerdict) {
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

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_is_rt_lookup_successful(true);
  lookup_service_.set_should_block_screenshot(false);
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  simulator->Start();
  simulator->Commit();

  auto* user_data_before_lookup = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data_before_lookup);
  EXPECT_FALSE(user_data_before_lookup->settings().allow_screenshots);

  EXPECT_TRUE(future_lookup_complete.Wait());
  EXPECT_FALSE(future.Get().allow_screenshots);

  auto* user_data_after_lookup = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data_after_lookup);
  EXPECT_FALSE(user_data_after_lookup->settings().allow_screenshots);
  ASSERT_TRUE(user_data_after_lookup->rt_lookup_response());
  ASSERT_FALSE(
      user_data_after_lookup->rt_lookup_response()->threat_info().empty());
  EXPECT_FALSE(user_data_after_lookup->rt_lookup_response()
                   ->threat_info(0)
                   .matched_url_navigation_rule()
                   .block_screenshot());
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
       InternalURLs_CreateForNavigationIfNeeded) {
  auto WillCreatePendingNav = [](const GURL& url) {
    return !std::ranges::contains(url::GetEmptyDocumentSchemes(),
                                  url.GetScheme());
  };

  SetContents(CreateTestWebContents());

  for (const auto* url : kInternalUrls) {
    GURL gurl(url);
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        gurl, web_contents());
    // Empty document scheme pages commit synchronously without a pending nav
    // handle, breaking `simulator->GetNavigationHandle()`. Since
    // CreateForNavigationIfNeeded only cares about the GURL, just mock a handle
    // with the expected GURL.
    auto mock_nav_handle = content::MockNavigationHandle(gurl, main_rfh());
    base::test::TestFuture<const UrlSettings&> future;
    FakeDataProtectionNavigationController controller(
        web_contents(), &lookup_service_, future.GetCallback());
    simulator->Start();
    auto navigation_observer =
        DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
            &controller, Profile::FromBrowserContext(browser_context()),
            WillCreatePendingNav(gurl) ? simulator->GetNavigationHandle()
                                       : &mock_nav_handle,
            future.GetCallback());
    ASSERT_NE(navigation_observer, nullptr);
  }
}

TEST_F(DataProtectionNavigationObserverTest,
       InternalURLs_ApplyDataProtectionSettings) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  data_controls::SetDataControls(profile()->GetPrefs(), {R"(
        {
          "name":"block",
          "rule_id":"1234",
          "sources":{"urls":["*"]},
          "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
        }
      )"});

  SetContents(CreateTestWebContents());

  for (const auto* url : kInternalUrls) {
    NavigateAndCommit(GURL(url));
    base::test::TestFuture<const UrlSettings&> future;
    DataProtectionNavigationObserver::ApplyDataProtectionSettings(
        Profile::FromBrowserContext(browser_context()), web_contents(),
        future.GetCallback());
    EXPECT_FALSE(future.Get().allow_screenshots);

    // Value should be cached.
    auto* user_data = DataProtectionPageUserData::GetForPage(
        GetPageFromWebContents(web_contents()));
    ASSERT_TRUE(user_data);
    EXPECT_EQ(user_data->settings(), future.Get());
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

TEST_F(DataProtectionNavigationObserverTest,
       SubframeNavigation_DoesNotDestroyObserver) {
  // Disable real-time check so the verdict is received immediately upon
  // observer creation.
  profile()->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  SetContents(CreateTestWebContents());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), web_contents()->GetPrimaryMainFrame());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  // Start the main frame navigation. This creates the observer.
  simulator->Start();

  // Create a subframe and simulate a complete navigation on it.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://subframe.com"), subframe);
  subframe_simulator->Start();
  subframe_simulator->Commit();

  // Commit the main frame navigation. If the observer was prematurely destroyed
  // by the subframe navigation, the callback would be dropped and this would
  // hang/fail.
  simulator->Commit();

  EXPECT_TRUE(future.IsReady());
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

enum class ScreenshotProtectionSource {
  kDataControls,
  kRealTimeUrlLookup,
};

class DataProtectionNavigationObserverRedirectScreenshotTest
    : public DataProtectionNavigationObserverTest,
      public testing::WithParamInterface<ScreenshotProtectionSource> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DataProtectionNavigationObserverRedirectScreenshotTest,
    testing::Values(ScreenshotProtectionSource::kDataControls,
                    ScreenshotProtectionSource::kRealTimeUrlLookup));

TEST_P(DataProtectionNavigationObserverRedirectScreenshotTest,
       BlockScreenshot_Redirect) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);

  switch (GetParam()) {
    case ScreenshotProtectionSource::kDataControls:
      data_controls::SetDataControls(profile()->GetPrefs(), {R"(
            {
              "name":"block",
              "rule_id":"1234",
              "sources":{"urls":["redirect.com"]},
              "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"} ]
            }
          )"});
      break;
    case ScreenshotProtectionSource::kRealTimeUrlLookup:
      lookup_service_.SetShouldHaveMatchedRule(true);
      lookup_service_.SetBlockScreenshotForURL(GURL("https://example.com"),
                                               false);
      lookup_service_.SetBlockScreenshotForURL(GURL("https://redirect.com"),
                                               true);
      lookup_service_.SetWatermarkTextForURL(GURL("https://example.com"),
                                             std::nullopt);
      lookup_service_.SetWatermarkTextForURL(GURL("https://redirect.com"),
                                             std::nullopt);
      break;
  }

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

#if BUILDFLAG(ENTERPRISE_WATERMARK)

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
#endif  //  BUILDFLAG(ENTERPRISE_WATERMARK)

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
    : public testing::TestWithParam<WatermarkStringParams> {
 protected:
  DataProtectionWatermarkStringTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableWatermarkTimestampTimezone);
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    DataProtectionWatermarkStringTest,
    DataProtectionWatermarkStringTest,
    testing::Values(
        WatermarkStringParams(
            "example@email.com",
            "custom_message",
            1709181364,
            "custom_message\nexample@email.com\n2024-02-29T04:36:04+00:00"),
        WatermarkStringParams(
            "<device-id>",
            "custom_message",
            1709181364,
            "custom_message\n<device-id>\n2024-02-29T04:36:04+00:00"),
        WatermarkStringParams("example@email.com",
                              "",
                              1709181364,
                              "example@email.com\n2024-02-29T04:36:04+00:00"),
        WatermarkStringParams("example@email.com",
                              std::nullopt,
                              1709181364,
                              "")));

TEST_P(DataProtectionWatermarkStringTest,
       TestGetWatermarkStringFromThreatInfo) {
  base::test::ScopedRestoreDefaultTimezone tz("UTC");
  safe_browsing::RTLookupResponse::ThreatInfo threat_info =
      GetTestThreatInfo(GetParam().custom_message, GetParam().timestamp_seconds,
                        GetParam().custom_message.has_value());
  EXPECT_EQ(
      enterprise_data_protection::GetWatermarkString(
          GetParam().identifier, threat_info.matched_url_navigation_rule()),
      GetParam().expected);
}

TEST_F(DataProtectionNavigationObserverTest,
       TestGetWatermarkStringFromThreatInfo_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableWatermarkTimestampTimezone);

  base::test::ScopedRestoreDefaultTimezone tz("UTC");
  safe_browsing::RTLookupResponse::ThreatInfo threat_info =
      GetTestThreatInfo("custom_message", 1709181364, true);

  EXPECT_EQ(enterprise_data_protection::GetWatermarkString(
                "example@email.com", threat_info.matched_url_navigation_rule()),
            "custom_message\nexample@email.com\n2024-02-29T04:36:04.000Z");
}

class SinglePageAppWatermarkTest : public DataProtectionNavigationObserverTest {
};

class SameDocumentNavigationWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit SameDocumentNavigationWebContentsObserver(
      content::WebContents* web_contents,
      FakeRealTimeUrlLookupService* lookup_service,
      content::BrowserContext* browser_context)
      : content::WebContentsObserver(web_contents),
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

    ASSERT_NE(navigation_observer, nullptr);
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<FakeRealTimeUrlLookupService> lookup_service_;
  raw_ptr<content::BrowserContext> browser_context_;
};

TEST_F(SinglePageAppWatermarkTest,
       CheckSameDocumentNavigation_CreateForNavigationIfNeeded) {
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("https://example.com"));
  SameDocumentNavigationWebContentsObserver observer(
      web_contents(), &lookup_service_, browser_context());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com#fragment"), main_rfh());

  // Ensure that the navigation callbacks are invoked, since the assertion is
  // outside the test body. If DidFinishNavigation() was called, then it is
  // guaranteed that DidStartNavigation() was called prior, thereby checking the
  // same document assertion.
  EXPECT_CALL(observer, DidFinishNavigation);
  simulator->CommitSameDocument();
}

struct WatermarkChangeParams {
  std::string source_url;
  std::optional<std::string> source_watermark;
  std::string destination_url;
  std::optional<std::string> destination_watermark;
} kWatermarkChangeTestCases[]{
    {
        .source_url = "https://example.com/watermark",
        .source_watermark = "custom_message",
        .destination_url = "https://example.com/watermark#unwatermarked",
        .destination_watermark = std::nullopt,
    },
    {
        .source_url = "https://example.com/unwatermarked",
        .source_watermark = std::nullopt,
        .destination_url = "https://example.com/unwatermarked#watermark",
        .destination_watermark = "custom_message",
    }};

class SinglePageAppWatermarkChangeTest
    : public SinglePageAppWatermarkTest,
      public testing::WithParamInterface<WatermarkChangeParams> {};

TEST_P(SinglePageAppWatermarkChangeTest,
       SameDocumentNavigation_WatermarkChanges) {
  DataProtectionNavigationObserver::SetLookupServiceForTesting(
      &lookup_service_);
  lookup_service_.SetWatermarkTextForURL(GURL(GetParam().source_url),
                                         GetParam().source_watermark);
  lookup_service_.SetWatermarkTextForURL(GURL(GetParam().destination_url),
                                         GetParam().destination_watermark);

  SetContents(CreateTestWebContents());
  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetRepeatingCallback());

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL(GetParam().source_url), web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  simulator->Commit();

  EXPECT_EQ(future.Take().watermark_text.empty(),
            !GetParam().source_watermark.has_value());

  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings().watermark_text.empty(),
            !GetParam().source_watermark.has_value());

  auto same_doc_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(GetParam().destination_url), main_rfh());
  same_doc_simulator->CommitSameDocument();

  EXPECT_EQ(future.Take().watermark_text.empty(),
            !GetParam().destination_watermark.has_value());

  // Verify PageUserData is updated to empty watermark on the same
  // content::Page.
  user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_EQ(user_data->settings().watermark_text.empty(),
            !GetParam().destination_watermark.has_value());
}

INSTANTIATE_TEST_SUITE_P(,
                         SinglePageAppWatermarkChangeTest,
                         testing::ValuesIn(kWatermarkChangeTestCases));

class OrderedDataProtectionNavigationObserverTest
    : public DataProtectionNavigationObserverTest,
      public testing::WithParamInterface<bool> {
 public:
  OrderedDataProtectionNavigationObserverTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnableWatermarkTimestampTimezone);
  }
  bool IsNavigationFinishedAfterVerdictReceived() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OrderedDataProtectionNavigationObserverTest, TestWatermarkTextUpdated) {
  base::test::ScopedRestoreDefaultTimezone tz("UTC");
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
  validator.ExpectUrlFilteringInterstitialEvent(expected_event);

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
                "\n2024-02-29T04:36:04+00:00");

  // Value should be cached.
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_NE(user_data->settings().watermark_text.find("custom_message"),
            std::string::npos);
  run_loop.Run();
}

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_DataControls_DistillerUrl_BlockScreenshot) {
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

  GURL original_url("https://example.com/article");
  GURL distilled_url = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, original_url, "Article Title");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      distilled_url, web_contents());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_is_rt_lookup_successful(false);
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  simulator->Start();
  EXPECT_TRUE(future_lookup_complete.Wait());
  simulator->Commit();

  EXPECT_FALSE(future.Get().allow_screenshots);
  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_FALSE(user_data->settings().allow_screenshots);
}

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_RTLookup_DistillerUrl_BlockScreenshot) {
  GURL original_url("https://example.com/article");
  GURL distilled_url = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, original_url, "Article Title");

  lookup_service_.set_should_block_screenshot(true);
  lookup_service_.SetShouldHaveMatchedRule(true);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      distilled_url, web_contents());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  base::test::TestFuture<void> future_lookup_complete;
  lookup_service_.set_on_start_lookup_complete(
      future_lookup_complete.GetCallback());

  simulator->Start();
  EXPECT_TRUE(future_lookup_complete.Wait());
  simulator->Commit();

  EXPECT_FALSE(future.Get().allow_screenshots);

  auto* user_data = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  ASSERT_TRUE(user_data);
  EXPECT_FALSE(user_data->settings().allow_screenshots);
}

TEST_F(DataProtectionNavigationObserverTest,
       TestScreenshotUpdated_DistillerUrl_InvalidHash) {
  GURL invalid_distilled_url(
      "chrome-distiller://invalid_hash/?url=https%3A%2F%2Fexample.com");

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      invalid_distilled_url, web_contents());

  base::test::TestFuture<const UrlSettings&> future;
  FakeDataProtectionNavigationController controller(
      web_contents(), &lookup_service_, future.GetCallback());

  simulator->Start();
  simulator->Commit();

  EXPECT_TRUE(future.Get().allow_screenshots);
}

INSTANTIATE_TEST_SUITE_P(OrderedDataProtectionNavigationObserverTest,
                         OrderedDataProtectionNavigationObserverTest,
                         testing::Bool());

}  // namespace enterprise_data_protection
