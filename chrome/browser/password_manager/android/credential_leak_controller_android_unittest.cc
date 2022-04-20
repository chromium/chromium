// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using password_manager::CreateLeakType;
using password_manager::HasChangeScript;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::metrics_util::LeakDialogDismissalReason;

namespace {

constexpr char kOrigin[] = "https://example.com";
constexpr char16_t kUsername[] = u"test_username";

// The On*Dialog() methods used by the tests below all invoke `delete this;`,
// thus there is no memory leak here.
CredentialLeakControllerAndroid* MakeController(
    IsSaved is_saved,
    IsReused is_reused,
    IsSyncing is_syncing,
    HasChangeScript has_change_script,
    PasswordChangeSuccessTracker* password_change_success_tracker = nullptr) {
  return new CredentialLeakControllerAndroid(
      CreateLeakType(is_saved, is_reused, is_syncing, has_change_script),
      GURL(kOrigin), kUsername, password_change_success_tracker,
      /*window_android=*/nullptr);
}

}  // namespace

TEST(CredentialLeakControllerAndroidTest, ClickedCancel) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(true), IsSyncing(true),
                 HasChangeScript(false))
      ->OnCancelDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedClose, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.CheckupAndChange",
      LeakDialogDismissalReason::kClickedClose, 1);
}

TEST(CredentialLeakControllerAndroidTest, ClickedOk) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(false), IsSyncing(false),
                 HasChangeScript(false))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedOk, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kClickedOk, 1);
}

TEST(CredentialLeakControllerAndroidTest, ClickedCheckPasswords) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(true), IsReused(true), IsSyncing(true),
                 HasChangeScript(false))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Checkup",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);
}

TEST(CredentialLeakControllerAndroidTest, ClickedChangePasswordAutomatically) {
  base::test::ScopedFeatureList enable_password_change;
  enable_password_change.InitAndEnableFeature(
      password_manager::features::kPasswordChange);
  base::HistogramTester histogram_tester;

  testing::NiceMock<MockPasswordChangeSuccessTracker>
      password_change_success_tracker;
  EXPECT_CALL(
      password_change_success_tracker,
      OnChangePasswordFlowStarted(
          GURL(kOrigin), base::UTF16ToUTF8(kUsername),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog));

  MakeController(IsSaved(true), IsReused(false), IsSyncing(true),
                 HasChangeScript(true), &password_change_success_tracker)
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedChangePasswordAutomatically, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.ChangeAutomatically",
      LeakDialogDismissalReason::kClickedChangePasswordAutomatically, 1);
}

TEST(CredentialLeakControllerAndroidTest, NoDirectInteraction) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(false), IsSyncing(false),
                 HasChangeScript(false))
      ->OnCloseDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);
}
