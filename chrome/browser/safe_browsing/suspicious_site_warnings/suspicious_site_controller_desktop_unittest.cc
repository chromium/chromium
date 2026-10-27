// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_controller_desktop.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace safe_browsing {

namespace {

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  return history::CreateHistoryService(context->GetPath(), /*create_db=*/true);
}

}  // namespace

class SuspiciousSiteControllerDesktopTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SuspiciousSiteControllerDesktopTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    test_ui_manager_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingUIManager>();
    sb_service_->SetUIManager(test_ui_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
  }

  void TearDown() override {
    DeleteContents();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    sb_service_.reset();
    test_ui_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SuspiciousSiteControllerDesktop* MakeController(int64_t navigation_id = 1) {
    security_interstitials::UnsafeResource resource;
    resource.url = web_contents()->GetLastCommittedURL().is_valid()
                       ? web_contents()->GetLastCommittedURL()
                       : GURL("https://suspicious.example.com");
    resource.threat_type =
        SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
    resource.threat_source = safe_browsing::ThreatSource::URL_REAL_TIME_CHECK;
    resource.is_async_check = true;
    resource.navigation_id = navigation_id;
    resource.rfh_locator =
        security_interstitials::UnsafeResourceLocator::CreateForFrameTreeNodeId(
            web_contents()
                ->GetPrimaryMainFrame()
                ->GetFrameTreeNodeId()
                .value());
    SuspiciousSiteControllerDesktop::ShowForWebContents(web_contents(),
                                                        resource);
    return SuspiciousSiteControllerDesktop::FromWebContents(web_contents());
  }

  bool HasShown(SuspiciousSiteControllerDesktop* controller) {
    return controller->has_shown_;
  }

  bool IsSuspended(SuspiciousSiteControllerDesktop* controller) {
    return controller->is_suspended_;
  }

  bool IsDismissed(SuspiciousSiteControllerDesktop* controller) {
    return controller->is_dismissed_;
  }

  TestSafeBrowsingUIManager* test_ui_manager() {
    return test_ui_manager_.get();
  }

 private:
  scoped_refptr<TestSafeBrowsingService> sb_service_;
  scoped_refptr<safe_browsing::TestSafeBrowsingUIManager> test_ui_manager_;
};

TEST_F(SuspiciousSiteControllerDesktopTest, OnBackToSafetyClicked) {
  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerDesktop* controller = MakeController();

  controller->OnBackToSafetyClicked();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kAdhered,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kBackToSafetyButton,
      /*expected_bucket_count=*/1);

  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerDesktopTest, OnMarkAsSafeClicked) {
  NavigateAndCommit(GURL("https://suspicious.example.com"));

  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerDesktop* controller = MakeController();

  controller->OnMarkAsSafeClicked();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kBypassed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kMarkAsSafe,
      /*expected_bucket_count=*/1);

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile());
  EXPECT_TRUE(SuspiciousSiteWarningAllowlist(hcsm).IsSiteAllowedForHost(
      "suspicious.example.com"));

  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerDesktopTest, OnLearnMoreClicked) {
  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerDesktop* controller = MakeController();

  controller->OnLearnMoreClicked();
  // Subsequent clicks should not log duplicate interaction metrics.
  controller->OnLearnMoreClicked();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kLearnMore,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerDesktopTest, OnBubbleDestroyed) {
  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  controller->OnBubbleDestroyed();

  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kDestroyed,
      /*expected_count=*/1);

  // Destructor logs outcome.
  web_contents()->RemoveUserData(
      SuspiciousSiteControllerDesktop::UserDataKey());

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kUnknown,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerDesktopTest, SuppressIfAllowlisted) {
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile());
  SuspiciousSiteWarningAllowlist(hcsm).AllowSiteForHost(
      "suspicious.example.com");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  navigation->Commit();

  // Controller should be dismissed immediately because host is allowlisted.
  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerDesktopTest, ShowsWarningAfterCommit) {
  bool shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  navigation->Commit();

  EXPECT_TRUE(shown);
  EXPECT_TRUE(HasShown(controller));
}

TEST_F(SuspiciousSiteControllerDesktopTest, DeferIfNavigationNotCommitted) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  bool shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  // Should not show before commit.
  EXPECT_FALSE(shown);

  navigation->Commit();

  // Should show once committed.
  EXPECT_TRUE(shown);
  EXPECT_TRUE(HasShown(controller));
}

TEST_F(SuspiciousSiteControllerDesktopTest, DiscardOnErrorPage) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  navigation->Fail(net::ERR_CONNECTION_FAILED);
  navigation->CommitErrorPage();
  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerDesktopTest, DiscardOnDifferentNavigation) {
  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation1->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation1->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://other.example.com"), web_contents());
  navigation2->Commit();

  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerDesktopTest, OnVisibilityChanged) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  bool shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  // Hide web contents before commit.
  web_contents()->WasHidden();
  navigation->Commit();

  // Should not show while hidden.
  EXPECT_FALSE(shown);

  // Show web contents.
  web_contents()->WasShown();
  EXPECT_TRUE(shown);
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       OnVisibilityChangedAfterBubbleShown) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  bool shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  navigation->Commit();
  EXPECT_TRUE(shown);

  // Tab is switched away (hidden).
  web_contents()->WasHidden();
  EXPECT_TRUE(IsSuspended(controller));

  // Tab is switched back (visible). Bubble should show again.
  shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));
  web_contents()->WasShown();
  EXPECT_TRUE(shown);
}

TEST_F(SuspiciousSiteControllerDesktopTest, OnVisibilityChangedAfterDismissed) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  bool shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);

  navigation->Commit();
  EXPECT_TRUE(shown);

  // User explicitly dismisses the bubble.
  controller->OnBubbleDismissed();
  EXPECT_TRUE(IsDismissed(controller));
  EXPECT_FALSE(IsSuspended(controller));

  // Tab is hidden.
  web_contents()->WasHidden();
  EXPECT_FALSE(IsSuspended(controller));

  // Tab is shown. Bubble should NOT show again.
  shown = false;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      base::BindOnce([](bool* shown) { *shown = true; }, &shown));
  web_contents()->WasShown();
  EXPECT_FALSE(shown);
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       SendsWarningShownCSBRROnShowBubble) {
  SetSafeBrowsingState(profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  NavigateAndCommit(GURL("https://suspicious.example.com/path"));

  SuspiciousSiteControllerDesktop* controller = MakeController();
  ASSERT_NE(controller, nullptr);
  controller->ShowBubble();

  std::list<std::string>* details = test_ui_manager()->GetThreatDetails();
  ASSERT_EQ(details->size(), 1u);

  ClientSafeBrowsingReportRequest report;
  ASSERT_TRUE(report.ParseFromString(details->front()));
  EXPECT_EQ(report.type(), ClientSafeBrowsingReportRequest::WARNING_SHOWN);
  EXPECT_EQ(report.url(), "https://suspicious.example.com/path");
  EXPECT_EQ(report.page_url(), "https://suspicious.example.com/path");
  EXPECT_EQ(report.url_request_destination(),
            ClientSafeBrowsingReportRequest::DOCUMENT);
  EXPECT_EQ(report.client_properties().url_api_type(),
            ClientSafeBrowsingReportRequest::REAL_TIME);
  EXPECT_TRUE(report.client_properties().is_async_check());
  EXPECT_EQ(report.warning_shown_info().warning_type(),
            ClientSafeBrowsingReportRequest::WarningShownInfo::
                SUSPICIOUS_SITE_WARNING);

  // Subsequent ShowBubble() calls for the same warning should not send
  // duplicate CSBRR reports.
  controller->ShowBubble();
  EXPECT_EQ(details->size(), 1u);
}

TEST_F(SuspiciousSiteControllerDesktopTest, HatsSurveyTriggeredOnBackToSafety) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  NavigateAndCommit(GURL("https://suspicious.example.com/path"));
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::UnorderedElementsAre(
              testing::Pair("Did proceed", false),
              testing::Pair("Learn more clicked", false),
              testing::Pair("Repeat visit", false)),
          testing::UnorderedElementsAre(
              testing::Pair("Site origin", "https://suspicious.example.com"),
              testing::Pair("User choice", "back_to_safety"),
              testing::Pair("Time prompt visible", "0.00"),
              testing::Pair("Referrer origin", "")),
          testing::_, testing::_,
          testing::Optional(std::string("BjcqKTVFY0ugnJ3q1cK0VD1dNzUU")),
          testing::_))
      .Times(1);

  controller->OnBackToSafetyClicked();
}

TEST_F(SuspiciousSiteControllerDesktopTest, HatsSurveyTriggeredOnMarkAsSafe) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  NavigateAndCommit(GURL("https://suspicious.example.com/path"));
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::Contains(testing::Pair("Did proceed", true)),
          testing::AllOf(
              testing::Contains(testing::Pair(
                  "Site origin", "https://suspicious.example.com")),
              testing::Contains(testing::Pair("User choice", "mark_as_safe"))),
          testing::_, testing::_,
          testing::Optional(std::string("qKoRRzn180ugnJ3q1cK0WX7PXscX")),
          testing::_))
      .Times(1);

  controller->OnMarkAsSafeClicked();
}

TEST_F(SuspiciousSiteControllerDesktopTest, HatsSurveyTriggeredOnDismiss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);
  base::HistogramTester histogram_tester;

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  NavigateAndCommit(GURL("https://suspicious.example.com/path"));
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::Contains(testing::Pair("Did proceed", true)),
          testing::AllOf(
              testing::Contains(testing::Pair(
                  "Site origin", "https://suspicious.example.com")),
              testing::Contains(testing::Pair("User choice", "dismiss"))),
          testing::_, testing::_,
          testing::Optional(std::string("qKoRRzn180ugnJ3q1cK0WX7PXscX")),
          testing::_))
      .Times(1);

  controller->OnBubbleDismissed();

  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kDismissed, 1);

  web_contents()->RemoveUserData(
      SuspiciousSiteControllerDesktop::UserDataKey());
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kBypassed, 1);
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       HatsSurveyTriggeredOnManualNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);
  base::HistogramTester histogram_tester;

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com/path"), web_contents());
  navigation1->SetReferrer(
      blink::mojom::Referrer::New(GURL("https://referrer.example.com/page"),
                                  network::mojom::ReferrerPolicy::kDefault));
  navigation1->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation1->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);
  navigation1->Commit();
  EXPECT_TRUE(HasShown(controller));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::Contains(testing::Pair("Did proceed", false)),
          testing::AllOf(
              testing::Contains(testing::Pair(
                  "Site origin", "https://suspicious.example.com")),
              testing::Contains(testing::Pair("Referrer origin",
                                              "https://referrer.example.com")),
              testing::Contains(
                  testing::Pair("User choice", "manual_navigation"))),
          testing::_, testing::_,
          testing::Optional(std::string("BjcqKTVFY0ugnJ3q1cK0VD1dNzUU")),
          testing::_))
      .Times(1);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://other.example.com/"), web_contents());
  navigation2->Commit();

  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kManualNavigation, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kAdhered, 1);
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       DidFinishNavigation_ErrorPageAfterBubbleShownTriggersSurvey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);
  base::HistogramTester histogram_tester;

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com/path"), web_contents());
  navigation1->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation1->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);
  navigation1->Commit();
  EXPECT_TRUE(HasShown(controller));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::Contains(testing::Pair("Did proceed", false)),
          testing::Contains(testing::Pair("User choice", "manual_navigation")),
          testing::_, testing::_,
          testing::Optional(std::string("BjcqKTVFY0ugnJ3q1cK0VD1dNzUU")),
          testing::_))
      .Times(1);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://other.example.com/"), web_contents());
  navigation2->Fail(net::ERR_CONNECTION_FAILED);
  navigation2->CommitErrorPage();

  EXPECT_FALSE(
      SuspiciousSiteControllerDesktop::FromWebContents(web_contents()));
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.SuspiciousSiteWarning.UserInteraction",
      SuspiciousSiteControllerDesktop::UserInteraction::kManualNavigation, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerDesktop::WarningOutcome::kAdhered, 1);
}

TEST_F(
    SuspiciousSiteControllerDesktopTest,
    OnVisibilityChanged_TabSwitched_SuspendsWithoutSurveyAndAccumulatesTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://suspicious.example.com"), web_contents());
  navigation->Start();

  SuspiciousSiteControllerDesktop* controller =
      MakeController(navigation->GetNavigationHandle()->GetNavigationId());
  ASSERT_NE(controller, nullptr);
  navigation->Commit();
  EXPECT_TRUE(HasShown(controller));

  task_environment()->FastForwardBy(base::Seconds(2));

  // Tab switch (hidden) should not trigger the survey.
  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(testing::_, testing::_, testing::_,
                                         testing::_, testing::_, testing::_,
                                         testing::_, testing::_))
      .Times(0);
  web_contents()->WasHidden();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service);

  // Time spent while hidden should not be counted.
  task_environment()->FastForwardBy(base::Seconds(10));

  // Tab becomes visible again for 1.5 seconds before dismissal.
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Milliseconds(1500));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(), testing::_,
          testing::Contains(testing::Pair("Time prompt visible", "3.50")),
          testing::_, testing::_, testing::_, testing::_))
      .Times(1);
  controller->OnBubbleDismissed();
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       FetchRepeatVisitCount_PopulatesRepeatVisitInSurvey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  HistoryServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestHistoryService));

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);

  GURL url("https://suspicious.example.com");
  NavigateAndCommit(url);
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
                  testing::Contains(testing::Pair("Repeat visit", true)),
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1);

  controller->OnBubbleDismissed();
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       OnLearnMoreClicked_PopulatesLearnMoreClickedInSurvey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  NavigateAndCommit(GURL("https://suspicious.example.com"));
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();
  controller->OnLearnMoreClicked();

  EXPECT_CALL(*mock_hats_service,
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
                  testing::Contains(testing::Pair("Learn more clicked", true)),
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .Times(1);

  controller->OnBubbleDismissed();
}

TEST_F(SuspiciousSiteControllerDesktopTest,
       HatsSurveyNotTriggeredAgainAfterDismissal) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSuspiciousSiteWarningSurvey);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  NavigateAndCommit(GURL("https://suspicious.example.com/path"));
  SuspiciousSiteControllerDesktop* controller = MakeController();
  controller->ShowBubble();

  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(
          kHatsSurveyTriggerSuspiciousSiteWarning, web_contents(),
          testing::Contains(testing::Pair("Did proceed", true)),
          testing::Contains(testing::Pair("User choice", "dismiss")),
          testing::_, testing::_,
          testing::Optional(std::string("qKoRRzn180ugnJ3q1cK0WX7PXscX")),
          testing::_))
      .Times(1);

  controller->OnBubbleDismissed();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service);

  // Resolving the warning later via Page Info should not trigger a second
  // survey.
  EXPECT_CALL(*mock_hats_service, LaunchSurveyForWebContents).Times(0);
  controller->OnMarkAsSafeClicked();
}

}  // namespace safe_browsing
