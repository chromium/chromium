// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace safe_browsing {

class SuspiciousSiteControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    sb_service_->Initialize();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    sb_service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SuspiciousSiteControllerAndroid* MakeController() {
    SuspiciousSiteControllerAndroid::ShowForWebContents(web_contents(),
                                                        /*navigation_id=*/1);
    return SuspiciousSiteControllerAndroid::FromWebContents(web_contents());
  }

 private:
  scoped_refptr<SafeBrowsingService> sb_service_;
};

TEST_F(SuspiciousSiteControllerAndroidTest, OnGoBackButtonClicked) {
  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerAndroid* controller = MakeController();

  controller->OnGoBackButtonClicked();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kAdhered,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerAndroidTest, OnContinueButtonClicked) {
  base::HistogramTester histogram_tester;
  SuspiciousSiteControllerAndroid* controller = MakeController();

  controller->OnContinueButtonClicked();

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kBypassed,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerAndroidTest, CloseDialogOutside) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  SuspiciousSiteControllerAndroid* controller = MakeController();
  controller->ShowDialog();

  // TOUCH_OUTSIDE closes the dialog view while keeping the controller active.
  controller->CloseDialog(
      ui::ModalDialogWrapper::DismissalCause::TOUCH_OUTSIDE);

  // Destroying the controller on tab close logs the tracked outcome
  // (kBypassed).
  web_contents()->RemoveUserData(
      SuspiciousSiteControllerAndroid::UserDataKey());

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kBypassed,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerAndroidTest, CloseDialogNavigateSameUrl) {
  GURL malicious_url("https://malicious.com");
  NavigateAndCommit(malicious_url);
  SuspiciousSiteControllerAndroid* controller = MakeController();

  // Dialog posts a task to show itself again if on Same URL.
  controller->CloseDialog(ui::ModalDialogWrapper::DismissalCause::NAVIGATE);
}

TEST_F(SuspiciousSiteControllerAndroidTest, CloseDialogNavigateDifferentUrl) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  GURL malicious_url("https://malicious.com");
  NavigateAndCommit(malicious_url);
  SuspiciousSiteControllerAndroid* controller = MakeController();
  controller->ShowDialog();

  // Navigate to a new distinct URL.
  GURL safe_url("https://safe.com");
  NavigateAndCommit(safe_url);

  // Controller will delete itself immediately on cross-origin navigation.
  EXPECT_FALSE(
      SuspiciousSiteControllerAndroid::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerAndroidTest, CloseDialogSuspendsOnTabSwitched) {
  MakeController();

  SuspiciousSiteControllerAndroid::FromWebContents(web_contents())
      ->CloseDialog(ui::ModalDialogWrapper::DismissalCause::TAB_SWITCHED);

  // The controller should not be deleted, as it is suspended.
  EXPECT_TRUE(SuspiciousSiteControllerAndroid::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerAndroidTest,
       CloseDialogSuspendsOnActivityDestroyed) {
  MakeController();

  SuspiciousSiteControllerAndroid::FromWebContents(web_contents())
      ->CloseDialog(ui::ModalDialogWrapper::DismissalCause::ACTIVITY_DESTROYED);

  // The controller should not be deleted, as it is suspended.
  EXPECT_TRUE(SuspiciousSiteControllerAndroid::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerAndroidTest,
       CloseDialogSuspendsOnInteractionDeferred) {
  MakeController();

  SuspiciousSiteControllerAndroid::FromWebContents(web_contents())
      ->CloseDialog(
          ui::ModalDialogWrapper::DismissalCause::DIALOG_INTERACTION_DEFERRED);

  // The controller should not be deleted, as it is suspended.
  EXPECT_TRUE(SuspiciousSiteControllerAndroid::FromWebContents(web_contents()));
}

TEST_F(SuspiciousSiteControllerAndroidTest, CloseDialogDismissedBySystem) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  SuspiciousSiteControllerAndroid* controller = MakeController();

  // Mark shown so metrics are logged on teardown.
  controller->ShowDialog();

  controller->CloseDialog(ui::ModalDialogWrapper::DismissalCause::UNKNOWN);

  // Closing the dialog view with an unhandled/UNKNOWN dismiss cause records
  // kUnknown on teardown.
  web_contents()->RemoveUserData(
      SuspiciousSiteControllerAndroid::UserDataKey());

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kUnknown,
      /*expected_bucket_count=*/1);
}

TEST_F(SuspiciousSiteControllerAndroidTest,
       CloseDialog_DismissedByCloseButton_PreservesAllowlistUrlSet) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  GURL malicious_url("https://suspicious.com");
  NavigateAndCommit(malicious_url);
  SuspiciousSiteControllerAndroid* controller = MakeController();

  // Calling ShowDialog adds the URL to the allowlist set as pending.
  controller->ShowDialog();

  SBThreatType threat_type;
  scoped_refptr<SafeBrowsingUIManager> ui_manager =
      g_browser_process->safe_browsing_service()->ui_manager();
  ASSERT_NE(ui_manager, nullptr);

  EXPECT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      malicious_url, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));
  EXPECT_EQ(threat_type, SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  // Closing/dismissing the dialog view leaves the URL in AllowlistUrlSet while
  // the user stays on the page so that the red Omnibox warning icon stays
  // active.
  controller->CloseDialog(ui::ModalDialogWrapper::DismissalCause::UNKNOWN);

  EXPECT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      malicious_url, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));
  EXPECT_EQ(threat_type, SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  // Destroying the controller (e.g. navigating away or closing tab) removes
  // current_suspicious_url_ from the allowlist set.
  web_contents()->RemoveUserData(
      SuspiciousSiteControllerAndroid::UserDataKey());

  EXPECT_FALSE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      malicious_url, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));
}

TEST_F(SuspiciousSiteControllerAndroidTest,
       ShowDialog_ReplacesPreviousSuspiciousUrlInAllowlist) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  GURL url1("https://suspicious1.com");
  NavigateAndCommit(url1);
  SuspiciousSiteControllerAndroid* controller = MakeController();
  controller->ShowDialog();

  scoped_refptr<SafeBrowsingUIManager> ui_manager =
      g_browser_process->safe_browsing_service()->ui_manager();
  ASSERT_NE(ui_manager, nullptr);

  SBThreatType threat_type;
  EXPECT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url1, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));

  GURL url2("https://suspicious2.com");
  NavigateAndCommit(url2);
  SuspiciousSiteControllerAndroid::ShowForWebContents(web_contents(),
                                                      /*navigation_id=*/2);
  auto* controller2 =
      SuspiciousSiteControllerAndroid::FromWebContents(web_contents());
  controller2->ShowDialog();

  // Old URL is removed from AllowlistUrlSet, and new URL is added.
  EXPECT_FALSE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url1, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));
  EXPECT_TRUE(ui_manager->IsUrlAllowlistedOrPendingForWebContents(
      url2, /*entry=*/nullptr, web_contents(),
      /*allowlist_only=*/false, &threat_type));
}

}  // namespace safe_browsing
