// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_password_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormMetricsRecorder;

namespace {
constexpr char kDefaultUrl[] = "http://example.com";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";
constexpr char kDismissalReasonHistogramName[] =
    "PasswordManager.SaveUIDismissalReason";
}  // namespace

class SavePasswordMessageDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  SavePasswordMessageDelegateTest() = default;

 protected:
  void SetUp() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      const GURL& url);
  void SetUsernameAndPassword(base::string16 username, base::string16 password);

  void CreateMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save,
                     bool is_saving_google_account);
  void TriggerActionClick();
  void TriggerBlocklistClick();
  void TriggerMessageDismissedCallback();

  messages::MessageWrapper* GetMessageWrapper();

  void VerifyUkmMetrics(const ukm::TestUkmRecorder& ukm_recorder,
                        PasswordFormMetricsRecorder::BubbleDismissalReason
                            expected_dismissal_reason);

 private:
  SavePasswordMessageDelegate delegate_;
  password_manager::PasswordForm form_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
};

void SavePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  NavigateAndCommit(GURL(kDefaultUrl));
}

std::unique_ptr<MockPasswordFormManagerForUI>
SavePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url) {
  password_form_url_ = password_form_url;
  auto form_manager = std::make_unique<MockPasswordFormManagerForUI>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(testing::ReturnRef(form_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(
          testing::Return(password_manager::metrics_util::CredentialSourceType::
                              kPasswordManager));
  ON_CALL(*form_manager, GetURL())
      .WillByDefault(testing::ReturnRef(password_form_url_));
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/);
  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(testing::Return(metrics_recorder_.get()));
  return form_manager;
}

void SavePasswordMessageDelegateTest::SetUsernameAndPassword(
    base::string16 username,
    base::string16 password) {
  form_.username_value = std::move(username);
  form_.password_value = std::move(password);
}

void SavePasswordMessageDelegateTest::CreateMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool is_saving_google_account) {
  delegate_.CreateMessage(web_contents(), std::move(form_to_save),
                          is_saving_google_account);
}

void SavePasswordMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SavePasswordMessageDelegateTest::TriggerBlocklistClick() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

void SavePasswordMessageDelegateTest::TriggerMessageDismissedCallback() {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread());
  EXPECT_EQ(nullptr, GetMessageWrapper());
  metrics_recorder_.reset();
}

messages::MessageWrapper* SavePasswordMessageDelegateTest::GetMessageWrapper() {
  return delegate_.message_.get();
}

void SavePasswordMessageDelegateTest::VerifyUkmMetrics(
    const ukm::TestUkmRecorder& ukm_recorder,
    PasswordFormMetricsRecorder::BubbleDismissalReason
        expected_dismissal_reason) {
  const auto& entries =
      ukm_recorder.GetEntriesByName(ukm::builders::PasswordForm::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(ukm_source_id_, entry->source_id);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_ShownName, 1);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_TriggerName,
        static_cast<int64_t>(PasswordFormMetricsRecorder::BubbleTrigger::
                                 kPasswordManagerSuggestionAutomatic));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::PasswordForm::kSaving_Prompt_InteractionName,
        static_cast<int64_t>(expected_dismissal_reason));
  }
}

// Tests that message properties (title, description, icon, button text) are
// set correctly.
TEST_F(SavePasswordMessageDelegateTest, MessagePropertyValues) {
  SetUsernameAndPassword(base::ASCIIToUTF16(kUsername),
                         base::ASCIIToUTF16(kPassword));
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  CreateMessage(std::move(form_manager), false /*is_saving_google_account*/);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            GetMessageWrapper()->GetTitle());
  EXPECT_NE(base::string16::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kUsername)));
  EXPECT_EQ(base::string16::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kPassword)));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON),
            GetMessageWrapper()->GetSecondaryActionText());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD),
      GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  TriggerMessageDismissedCallback();
}

// Tests that the title is set correctly when the user is syncing passwords to
// their Google Account.
TEST_F(SavePasswordMessageDelegateTest, SaveToGoogleTitle) {
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  CreateMessage(std::move(form_manager), true /*is_saving_google_account*/);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_TO_GOOGLE),
            GetMessageWrapper()->GetTitle());

  TriggerMessageDismissedCallback();
}

// Tests that password form is saved and metrics recorded correctly when the
// user clicks "Save" button.
TEST_F(SavePasswordMessageDelegateTest, SaveOnActionClick) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EXPECT_CALL(*form_manager, Save());
  CreateMessage(std::move(form_manager), false /*is_saving_google_account*/);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerMessageDismissedCallback();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that password form is not saved and metrics recorded correctly when the
// user dismisses the message.
TEST_F(SavePasswordMessageDelegateTest, DontSaveOnDismiss) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EXPECT_CALL(*form_manager, Save()).Times(0);
  CreateMessage(std::move(form_manager), false /*is_saving_google_account*/);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerMessageDismissedCallback();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored);
  histogram_tester.ExpectUniqueSample(
      kDismissalReasonHistogramName,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}
