// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace safe_browsing {

class SuspiciousSiteControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SuspiciousSiteControllerAndroid* MakeController() {
    SuspiciousSiteControllerAndroid::CreateForWebContents(web_contents());
    return SuspiciousSiteControllerAndroid::FromWebContents(web_contents());
  }
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
  SuspiciousSiteControllerAndroid* controller = MakeController();

  // TOUCH_OUTSIDE maps to navigate back.
  controller->CloseDialog(
      ui::ModalDialogWrapper::DismissalCause::TOUCH_OUTSIDE);

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kAdhered,
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
  GURL malicious_url("https://malicious.com");
  NavigateAndCommit(malicious_url);
  MakeController();

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

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome",
      SuspiciousSiteControllerAndroid::WarningOutcome::kDismissedBySystem,
      /*expected_bucket_count=*/1);
}

}  // namespace safe_browsing
