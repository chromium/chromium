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
#include "components/messages/android/mock_message_dispatcher_bridge.h"
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
constexpr char kAccountEmail[] = "account@example.com";
constexpr char kDismissalReasonHistogramName[] =
    "PasswordManager.SaveUIDismissalReason";
}  // namespace

class SavePasswordMessageDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  SavePasswordMessageDelegateTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      const GURL& url);
  void SetUsernameAndPassword(std::u16string username, std::u16string password);

  void EnqueueMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save,
                      bool user_signed_in);
  void TriggerActionClick();
  void TriggerBlocklistClick();

  void ExpectDismissMessageCall();
  void DismissMessage(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* GetMessageWrapper();

  void CommitPasswordFormMetrics();
  void VerifyUkmMetrics(const ukm::TestUkmRecorder& ukm_recorder,
                        PasswordFormMetricsRecorder::BubbleDismissalReason
                            expected_dismissal_reason);

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

 private:
  SavePasswordMessageDelegate delegate_;
  password_manager::PasswordForm form_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

void SavePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/);

  NavigateAndCommit(GURL(kDefaultUrl));

  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void SavePasswordMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordFormManagerForUI>
SavePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url) {
  password_form_url_ = password_form_url;
  auto form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(testing::ReturnRef(form_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(
          testing::Return(password_manager::metrics_util::CredentialSourceType::
                              kPasswordManager));
  ON_CALL(*form_manager, GetURL())
      .WillByDefault(testing::ReturnRef(password_form_url_));
  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(testing::Return(metrics_recorder_.get()));
  return form_manager;
}

void SavePasswordMessageDelegateTest::SetUsernameAndPassword(
    std::u16string username,
    std::u16string password) {
  form_.username_value = std::move(username);
  form_.password_value = std::move(password);
}

void SavePasswordMessageDelegateTest::EnqueueMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool user_signed_in) {
  base::Optional<AccountInfo> account_info;
  if (user_signed_in) {
    account_info = AccountInfo();
    account_info.value().email = kAccountEmail;
  }
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_.DisplaySavePasswordPromptInternal(
      web_contents(), std::move(form_to_save), account_info);
}

void SavePasswordMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SavePasswordMessageDelegateTest::TriggerBlocklistClick() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

void SavePasswordMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   content::WebContents* web_contents,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SavePasswordMessageDelegateTest::DismissMessage(
    messages::DismissReason dismiss_reason) {
  ExpectDismissMessageCall();
  delegate_.DismissSavePasswordPromptInternal(dismiss_reason);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

messages::MessageWrapper* SavePasswordMessageDelegateTest::GetMessageWrapper() {
  return delegate_.message_.get();
}

void SavePasswordMessageDelegateTest::CommitPasswordFormMetrics() {
  // PasswordFormMetricsRecorder::dtor commits accumulated metrics.
  metrics_recorder_.reset();
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
  EnqueueMessage(std::move(form_manager), false /*user_signed_in*/);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            GetMessageWrapper()->GetTitle());
  EXPECT_NE(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kUsername)));
  EXPECT_EQ(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kPassword)));
  EXPECT_EQ(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kAccountEmail)));

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON),
            GetMessageWrapper()->GetSecondaryButtonMenuText());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD),
      GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the user is signed.
TEST_F(SavePasswordMessageDelegateTest, SignedInDescription) {
  SetUsernameAndPassword(base::ASCIIToUTF16(kUsername),
                         base::ASCIIToUTF16(kPassword));
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EnqueueMessage(std::move(form_manager), true /*user_signed_in*/);

  EXPECT_NE(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kUsername)));
  EXPECT_EQ(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kPassword)));
  EXPECT_NE(std::u16string::npos, GetMessageWrapper()->GetDescription().find(
                                      base::ASCIIToUTF16(kAccountEmail)));

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SavePasswordMessageDelegateTest, OnlyOnePromptAtATime) {
  SetUsernameAndPassword(base::ASCIIToUTF16(kUsername),
                         base::ASCIIToUTF16(kPassword));
  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EnqueueMessage(std::move(form_manager), true /*user_signed_in*/);

  ExpectDismissMessageCall();
  auto form_manager2 = CreateFormManager(GURL(kDefaultUrl));
  EnqueueMessage(std::move(form_manager2), true /*user_signed_in*/);
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that password form is saved and metrics recorded correctly when the
// user clicks "Save" button.
TEST_F(SavePasswordMessageDelegateTest, SaveOnActionClick) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), false /*user_signed_in*/);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
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
  EnqueueMessage(std::move(form_manager), false /*user_signed_in*/);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

// Tests that password form is not saved and metrics recorded correctly when the
// message is autodismissed.
TEST_F(SavePasswordMessageDelegateTest, MetricOnAutodismissTimer) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager = CreateFormManager(GURL(kDefaultUrl));
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), false /*user_signed_in*/);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored);
  histogram_tester.ExpectUniqueSample(
      kDismissalReasonHistogramName,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}
