// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_controller_desktop.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SuspiciousSiteControllerDesktopTest
    : public ChromeRenderViewHostTestHarness {
 public:
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
    SuspiciousSiteControllerDesktop::ShowForWebContents(web_contents(),
                                                        navigation_id);
    return SuspiciousSiteControllerDesktop::FromWebContents(web_contents());
  }

  bool HasShown(SuspiciousSiteControllerDesktop* controller) {
    return controller->has_shown_;
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

}  // namespace safe_browsing
