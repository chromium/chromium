// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/mock_password_checkup_launcher_helper.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

constexpr ukm::SourceId kTestSourceId = 0x1234;

using password_manager::CreateLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogMetricsRecorder;
using password_manager::metrics_util::LeakDialogType;
using testing::_;
using testing::StrictMock;
using UkmEntry = ukm::builders::PasswordManager_LeakWarningDialog;

namespace {

constexpr char kOrigin[] = "https://example.com";
constexpr char16_t kUsername[] = u"test_username";

// The On*Dialog() methods used by the tests below all invoke `delete this;`,
// thus there is no memory leak here.
CredentialLeakControllerAndroid* MakeController(
    std::unique_ptr<MockPasswordCheckupLauncherHelper> check_launcher,
    IsSaved is_saved,
    IsReused is_reused,
    IsSyncing is_syncing) {
  password_manager::CredentialLeakType leak_type =
      CreateLeakType(is_saved, is_reused, is_syncing);
  auto recorder = std::make_unique<LeakDialogMetricsRecorder>(
      kTestSourceId, password_manager::GetLeakDialogType(leak_type));
  // Set sampling rate to 100% to avoid flakiness.
  recorder->SetSamplingRateForTesting(1.0);
  return new CredentialLeakControllerAndroid(
      leak_type, GURL(kOrigin), kUsername,
      /*window_android=*/nullptr, std::move(check_launcher),
      std::move(recorder));
}

void CheckUkmMetricsExpectations(
    ukm::TestAutoSetUkmRecorder& recorder,
    LeakDialogType expected_dialog_type,
    LeakDialogDismissalReason expected_dismissal_reason) {
  const auto& entries = recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    recorder.ExpectEntryMetric(entry,
                               UkmEntry::kPasswordLeakDetectionDialogTypeName,
                               static_cast<int64_t>(expected_dialog_type));
    recorder.ExpectEntryMetric(
        entry, UkmEntry::kPasswordLeakDetectionDialogDismissalReasonName,
        static_cast<int64_t>(expected_dismissal_reason));
  }
}

}  // namespace

TEST(CredentialLeakControllerAndroidTest, ClickedCancel) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  MakeController(std::make_unique<MockPasswordCheckupLauncherHelper>(),
                 IsSaved(false), IsReused(true), IsSyncing(true))
      ->OnCancelDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedClose, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.CheckupAndChange",
      LeakDialogDismissalReason::kClickedClose, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder,
                              LeakDialogType::kCheckupAndChange,
                              LeakDialogDismissalReason::kClickedClose);
}

TEST(CredentialLeakControllerAndroidTest, ClickedOkDoesNotLaunchCheckup) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  std::unique_ptr<StrictMock<MockPasswordCheckupLauncherHelper>> mock_launcher =
      std::make_unique<StrictMock<MockPasswordCheckupLauncherHelper>>();
  MakeController(std::move(mock_launcher), IsSaved(false), IsReused(false),
                 IsSyncing(false))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedOk, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kClickedOk, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder, LeakDialogType::kChange,
                              LeakDialogDismissalReason::kClickedOk);
}

TEST(CredentialLeakControllerAndroidTest,
     ClickedCheckPasswordsLaunchesCheckup) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  std::unique_ptr<MockPasswordCheckupLauncherHelper> mock_launcher =
      std::make_unique<MockPasswordCheckupLauncherHelper>();
  EXPECT_CALL(
      *mock_launcher,
      LaunchLocalCheckup(
          _, _, password_manager::PasswordCheckReferrerAndroid::kLeakDialog));
  MakeController(std::move(mock_launcher), IsSaved(true), IsReused(true),
                 IsSyncing(true))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Checkup",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  CheckUkmMetricsExpectations(
      test_ukm_recorder, LeakDialogType::kCheckup,
      LeakDialogDismissalReason::kClickedCheckPasswords);
}

TEST(CredentialLeakControllerAndroidTest, NoDirectInteraction) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  MakeController(std::make_unique<MockPasswordCheckupLauncherHelper>(),
                 IsSaved(false), IsReused(false), IsSyncing(false))
      ->OnCloseDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  CheckUkmMetricsExpectations(test_ukm_recorder, LeakDialogType::kChange,
                              LeakDialogDismissalReason::kNoDirectInteraction);
}
