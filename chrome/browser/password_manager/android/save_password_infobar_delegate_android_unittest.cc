// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_password_infobar_delegate_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormMetricsRecorder;
using testing::Return;
using testing::ReturnRef;

namespace {

class TestSavePasswordInfoBarDelegate : public SavePasswordInfoBarDelegate {
 public:
  TestSavePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool is_smartlock_branding_enabled)
      : SavePasswordInfoBarDelegate(web_contents,
                                    std::move(form_to_save),
                                    is_smartlock_branding_enabled) {}

  ~TestSavePasswordInfoBarDelegate() override = default;
};

class SavePasswordInfoBarDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  SavePasswordInfoBarDelegateTest() = default;
  ~SavePasswordInfoBarDelegateTest() override = default;

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateMockFormManager(
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
      bool with_federation_origin);

 protected:
  std::unique_ptr<PasswordManagerInfoBarDelegate> CreateDelegate(
      std::unique_ptr<PasswordFormManagerForUI> password_form_manager,
      bool is_smartlock_branding_enabled);
  void CreateTestForm(bool with_federation_origin);

  password_manager::PasswordForm test_form_;
};

std::unique_ptr<MockPasswordFormManagerForUI>
SavePasswordInfoBarDelegateTest::CreateMockFormManager(
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
    bool with_federation_origin) {
  auto password_form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  CreateTestForm(with_federation_origin);

  ON_CALL(*password_form_manager, GetPendingCredentials)
      .WillByDefault(ReturnRef(test_form_));
  ON_CALL(*password_form_manager, GetURL)
      .WillByDefault(ReturnRef(test_form_.url));
  if (metrics_recorder) {
    ON_CALL(*password_form_manager, GetMetricsRecorder)
        .WillByDefault(Return(metrics_recorder.get()));
  }
  return password_form_manager;
}

std::unique_ptr<PasswordManagerInfoBarDelegate>
SavePasswordInfoBarDelegateTest::CreateDelegate(
    std::unique_ptr<PasswordFormManagerForUI> password_form_manager,
    bool is_smartlock_branding_enabled) {
  return std::make_unique<TestSavePasswordInfoBarDelegate>(
      web_contents(), std::move(password_form_manager),
      is_smartlock_branding_enabled);
}

void SavePasswordInfoBarDelegateTest::CreateTestForm(
    bool with_federation_origin) {
  test_form_.url = GURL("https://example.com");
  test_form_.username_value = u"username";
  test_form_.password_value = u"12345";
  if (with_federation_origin) {
    test_form_.federation_origin =
        url::Origin::Create(GURL("https://example.com"));
  }
}

void SavePasswordInfoBarDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
}

void SavePasswordInfoBarDelegateTest::TearDown() {
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(SavePasswordInfoBarDelegateTest, CancelTest) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  EXPECT_CALL(*password_form_manager.get(), Blocklist());
  std::unique_ptr<ConfirmInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/true));
  EXPECT_TRUE(infobar->Cancel());
}

TEST_F(SavePasswordInfoBarDelegateTest, HasDetailsMessageWhenSyncing) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/true));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER),
            infobar->GetDetailsMessageText());
}

TEST_F(SavePasswordInfoBarDelegateTest, EmptyDetailsMessageWhenNotSyncing) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  EXPECT_TRUE(infobar->GetDetailsMessageText().empty());
}

TEST_F(SavePasswordInfoBarDelegateTest,
       EmptyDetailsMessageForFederatedCredentialsWhenSyncing) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/true);
  NavigateAndCommit(GURL("https://example.com"));
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/true));
  EXPECT_TRUE(infobar->GetDetailsMessageText().empty());
}

TEST_F(SavePasswordInfoBarDelegateTest,
       EmptyDetailsMessageForFederatedCredentialsWhenNotSyncing) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/true);
  NavigateAndCommit(GURL("https://example.com"));
  std::unique_ptr<PasswordManagerInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  EXPECT_TRUE(infobar->GetDetailsMessageText().empty());
}

TEST_F(SavePasswordInfoBarDelegateTest, RecordsSaveAfterUnblocklisting) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  ON_CALL(*password_form_manager, WasUnblocklisted).WillByDefault(Return(true));
  std::unique_ptr<ConfirmInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  base::HistogramTester histogram_tester;
  infobar->Accept();
  infobar.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReasonAfterUnblacklisting",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

TEST_F(SavePasswordInfoBarDelegateTest, RecordNeverAfterUnblocklisting) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  ON_CALL(*password_form_manager, WasUnblocklisted).WillByDefault(Return(true));
  std::unique_ptr<ConfirmInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  base::HistogramTester histogram_tester;
  infobar->Cancel();
  infobar.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReasonAfterUnblacklisting",
      password_manager::metrics_util::CLICKED_NEVER, 1);
}

TEST_F(SavePasswordInfoBarDelegateTest, RecordDismissAfterUnblocklisting) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  ON_CALL(*password_form_manager, WasUnblocklisted).WillByDefault(Return(true));
  std::unique_ptr<ConfirmInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  base::HistogramTester histogram_tester;
  infobar->InfoBarDismissed();
  infobar.reset();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReasonAfterUnblacklisting",
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

TEST_F(SavePasswordInfoBarDelegateTest, DontRecordIfNotUnblocklisted) {
  std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
      CreateMockFormManager(
          /*metrics_recorder=*/nullptr, /*with_federation_origin=*/false);
  ON_CALL(*password_form_manager, WasUnblocklisted)
      .WillByDefault(Return(false));
  std::unique_ptr<ConfirmInfoBarDelegate> infobar(
      CreateDelegate(std::move(password_form_manager),
                     /*is_smartlock_branding_enabled=*/false));
  base::HistogramTester histogram_tester;
  infobar->InfoBarDismissed();
  infobar.reset();
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SaveUIDismissalReasonAfterUnblacklisting", 0);
}

class SavePasswordInfoBarDelegateTestForUKMs
    : public SavePasswordInfoBarDelegateTest,
      public ::testing::WithParamInterface<
          PasswordFormMetricsRecorder::BubbleDismissalReason> {
 public:
  SavePasswordInfoBarDelegateTestForUKMs() = default;
  ~SavePasswordInfoBarDelegateTestForUKMs() = default;
};

// Verify that URL keyed metrics are recorded for showing and interacting with
// the password save prompt.
TEST_P(SavePasswordInfoBarDelegateTestForUKMs, VerifyUKMRecording) {
  using BubbleTrigger = PasswordFormMetricsRecorder::BubbleTrigger;
  using BubbleDismissalReason =
      PasswordFormMetricsRecorder::BubbleDismissalReason;
  using UkmEntry = ukm::builders::PasswordForm;

  BubbleDismissalReason dismissal_reason = GetParam();
  SCOPED_TRACE(::testing::Message() << "dismissal_reason = "
                                    << static_cast<int64_t>(dismissal_reason));

  ukm::SourceId expected_source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    // Setup metrics recorder
    auto recorder = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        /*is_main_frame_secure=*/true, expected_source_id,
        /*pref_service=*/nullptr);

    // Exercise delegate.
    std::unique_ptr<MockPasswordFormManagerForUI> password_form_manager =
        CreateMockFormManager(recorder, /*with_federation_origin=*/false);
    ON_CALL(*password_form_manager.get(), GetCredentialSource)
        .WillByDefault(Return(password_manager::metrics_util::
                                  CredentialSourceType::kPasswordManager));
    if (dismissal_reason == BubbleDismissalReason::kDeclined)
      EXPECT_CALL(*password_form_manager.get(), Blocklist());
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(std::move(password_form_manager),
                       /*is_smartlock_branding_enabled=*/true));
    switch (dismissal_reason) {
      case BubbleDismissalReason::kAccepted:
        EXPECT_TRUE(infobar->Accept());
        break;
      case BubbleDismissalReason::kDeclined:
        EXPECT_TRUE(infobar->Cancel());
        break;
      case BubbleDismissalReason::kIgnored:
        // Do nothing.
        break;
      case BubbleDismissalReason::kUnknown:
        NOTREACHED();
        break;
    }
  }

  // Verify metrics.
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(expected_source_id, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(entry,
                                        UkmEntry::kSaving_Prompt_ShownName, 1);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kSaving_Prompt_TriggerName,
        static_cast<int64_t>(
            BubbleTrigger::kPasswordManagerSuggestionAutomatic));
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kSaving_Prompt_InteractionName,
        static_cast<int64_t>(dismissal_reason));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SavePasswordInfoBarDelegateTestForUKMs,
    ::testing::Values(
        PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
        PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined,
        PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored));

}  // namespace
