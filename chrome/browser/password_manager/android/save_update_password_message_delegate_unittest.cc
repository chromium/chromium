// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"
#include <jni.h>
#include <algorithm>
#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

using base::MockCallback;
using base::RepeatingCallback;
using base::test::FeatureRef;
using base::test::FeatureRefAndParams;
using password_manager::MockPasswordFormManagerForUI;
using password_manager::PasswordForm;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordFormMetricsRecorder;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {
constexpr char kDefaultUrl[] = "http://example.com";
constexpr char16_t kUsername[] = u"username";
constexpr char16_t kUsername2[] = u"username2";
constexpr char16_t kPassword[] = u"password";
constexpr char kAccountEmail[] = "account@example.com";
constexpr char16_t kAccountEmail16[] = u"account@example.com";
constexpr char kAccountFullName[] = "First Last";
constexpr char16_t kAccountFullName16[] = u"First Last";
constexpr char kSaveUIDismissalReasonHistogramName[] =
    "PasswordManager.SaveUIDismissalReason";
constexpr char kUpdateUIDismissalReasonHistogramName[] =
    "PasswordManager.UpdateUIDismissalReason";
constexpr char kSaveUpdatePasswordMessageDismissalReason[] =
    "PasswordManager.SaveUpdateUIDismissalReasonAndroid";
constexpr char kSavePasswordMessageDismissalReason[] =
    "PasswordManager.SaveUpdateUIDismissalReasonAndroid.Save";
constexpr char kUpdatePasswordMessageDismissalReason[] =
    "PasswordManager.SaveUpdateUIDismissalReasonAndroid.Update";
constexpr char kConfirmUsernameMessageDismissalReason[] =
    "PasswordManager.SaveUpdateUIDismissalReasonAndroid."
    "UpdateWithUsernameConfirmation";

struct FeatureConfigTestParam {
  bool with_unified_password_manager_android;
  bool with_exploratory_save_update_password_strings;
  int save_update_prompt_syncing_string_version;
};

}  // namespace

class MockPasswordEditDialog : public PasswordEditDialog {
 public:
  MOCK_METHOD(void,
              ShowPasswordEditDialog,
              (const std::vector<std::u16string>& usernames,
               const std::u16string& username,
               const std::u16string& password,
               const std::string& account_email),
              (override));
  MOCK_METHOD(void,
              ShowLegacyPasswordEditDialog,
              (const std::vector<std::u16string>& usernames,
               int selected_username_index,
               const std::string& account_email),
              (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

class SaveUpdatePasswordMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveUpdatePasswordMessageDelegateTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManager(
      const GURL& password_form_url,
      const std::vector<const PasswordForm*>* best_matches);
  void SetPendingCredentials(std::u16string username, std::u16string password);
  static PasswordForm CreatePasswordForm(std::u16string username,
                                         std::u16string password);

  void EnqueueMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save,
                      bool user_signed_in,
                      bool update_password,
                      absl::optional<AccountInfo> account_info = {});
  void TriggerActionClick();
  void TriggerPasswordEditDialog(bool update_password);
  void TriggerNeverSaveMenuItem();

  void ExpectDismissMessageCall();
  void DismissMessage(messages::DismissReason dismiss_reason);
  void DestroyDelegate();

  messages::MessageWrapper* GetMessageWrapper();

  // Password edit dialog factory function that is passed to
  // SaveUpdatePasswordMessageDelegate. Passes the dialog prepared by
  // PreparePasswordEditDialog. Captures accept and dismiss callbacks.
  std::unique_ptr<PasswordEditDialog> CreatePasswordEditDialog(
      content::WebContents* web_contents,
      PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback,
      PasswordEditDialog::LegacyDialogAcceptedCallback
          legacy_dialog_accepted_callback,
      PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback);

  // Creates a mock of PasswordEditDialog that will be passed to
  // SaveUpdatePasswordMessageDelegate through CreatePasswordEditDialog factory.
  // Returns non-owning pointer to the mock for test to configure mock
  // expectations.
  MockPasswordEditDialog* PreparePasswordEditDialog();

  base::MockCallback<RepeatingCallback<
      void(gfx::NativeWindow,
           Profile*,
           password_manager::metrics_util::PasswordMigrationWarningTriggers)>>&
  GetMigrationWarningCallback();

  void TriggerDialogAcceptedCallback(const std::u16string& username,
                                     const std::u16string& password);
  void TriggerLegacyDialogAcceptedCallback(int selected_username_index);
  void TriggerDialogDismissedCallback(bool dialog_accepted);

  // TODO(crbug.com/1428562): Remove this helper as it makes tests hard to read.
  std::u16string GetExpectedUPMMessageDescription(
      bool is_update,
      bool is_signed_in,
      const std::u16string& account_email);
  void CommitPasswordFormMetrics();
  void VerifyUkmMetrics(const ukm::TestUkmRecorder& ukm_recorder,
                        PasswordFormMetricsRecorder::BubbleDismissalReason
                            expected_dismissal_reason);

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  const std::vector<const PasswordForm*>* empty_best_matches() {
    return &kEmptyBestMatches;
  }

  const std::vector<const PasswordForm*>* two_forms_best_matches() {
    return &kTwoFormsBestMatches;
  }

 private:
  const PasswordForm kPasswordForm1 = CreatePasswordForm(kUsername, kPassword);
  const PasswordForm kPasswordForm2 = CreatePasswordForm(kUsername2, kPassword);
  const std::vector<const PasswordForm*> kEmptyBestMatches = {};
  const std::vector<const PasswordForm*> kTwoFormsBestMatches = {
      &kPasswordForm1, &kPasswordForm2};

  PasswordForm pending_credentials_;
  std::unique_ptr<SaveUpdatePasswordMessageDelegate> delegate_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockPasswordEditDialog> mock_password_edit_dialog_;
  PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback_;
  PasswordEditDialog::LegacyDialogAcceptedCallback
      legacy_dialog_accepted_callback_;
  PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback_;
  base::MockCallback<RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      mock_password_migration_warning_callback_;
};

class SaveUpdatePasswordMessageDelegateWithFeaturesTest
    : public testing::WithParamInterface<FeatureConfigTestParam>,
      public SaveUpdatePasswordMessageDelegateTest {
 protected:
  void SetUp() override;
  void InitFeatureList();

  // TODO(crbug.com/1428562): Remove this helper as it makes tests hard to read.
  std::u16string GetExploratoryStringsMessageDescription(
      bool is_update,
      bool is_signed_in,
      const std::u16string& account_email,
      int new_string_version);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

SaveUpdatePasswordMessageDelegateTest::SaveUpdatePasswordMessageDelegateTest() =
    default;

void SaveUpdatePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/);
  NavigateAndCommit(GURL(kDefaultUrl));

  // Using `new` and `WrapUnique` to access a non-public constructor.
  delegate_ = base::WrapUnique(new SaveUpdatePasswordMessageDelegate(
      base::BindRepeating(
          &SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog,
          base::Unretained(this)),
      mock_password_migration_warning_callback_.Get()));

  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void SaveUpdatePasswordMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

void SaveUpdatePasswordMessageDelegateWithFeaturesTest::SetUp() {
  SaveUpdatePasswordMessageDelegateTest::SetUp();
  InitFeatureList();
}
std::unique_ptr<MockPasswordFormManagerForUI>
SaveUpdatePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url,
    const std::vector<const PasswordForm*>* best_matches) {
  password_form_url_ = password_form_url;
  auto form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(ReturnRef(pending_credentials_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(Return(password_manager::metrics_util::
                                CredentialSourceType::kPasswordManager));
  ON_CALL(*form_manager, GetURL()).WillByDefault(ReturnRef(password_form_url_));
  ON_CALL(*form_manager, GetBestMatches())
      .WillByDefault(ReturnRef(*best_matches));
  ON_CALL(*form_manager, GetFederatedMatches())
      .WillByDefault(Return(std::vector<const PasswordForm*>{}));

  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(Return(metrics_recorder_.get()));
  return form_manager;
}

void SaveUpdatePasswordMessageDelegateTest::SetPendingCredentials(
    std::u16string username,
    std::u16string password) {
  pending_credentials_.username_value = std::move(username);
  pending_credentials_.password_value = std::move(password);
}

// static
PasswordForm SaveUpdatePasswordMessageDelegateTest::CreatePasswordForm(
    std::u16string username,
    std::u16string password) {
  PasswordForm password_form;
  password_form.username_value = std::move(username);
  password_form.password_value = std::move(password);
  return password_form;
}

void SaveUpdatePasswordMessageDelegateTest::EnqueueMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool user_signed_in,
    bool update_password,
    absl::optional<AccountInfo> account_info) {
  if (user_signed_in && !account_info) {
    account_info = AccountInfo();
    account_info.value().email = kAccountEmail;
  }
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->DisplaySaveUpdatePasswordPromptInternal(
      web_contents(), std::move(form_to_save), account_info, update_password);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // Simulate call from Java to dismiss message on primary button click.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerPasswordEditDialog(
    bool update_password) {
  if (update_password) {
    GetMessageWrapper()->HandleSecondaryActionClick(
        base::android::AttachCurrentThread());
  } else {
    GetMessageWrapper()->HandleSecondaryMenuItemSelected(
        base::android::AttachCurrentThread(),
        static_cast<int>(SaveUpdatePasswordMessageDelegate::
                             SavePasswordDialogMenuItem::kEditPassword));
  }
  // Simulate call from Java to dismiss message on secondary button click.
  DismissMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerNeverSaveMenuItem() {
  GetMessageWrapper()->HandleSecondaryMenuItemSelected(
      base::android::AttachCurrentThread(),
      static_cast<int>(SaveUpdatePasswordMessageDelegate::
                           SavePasswordDialogMenuItem::kNeverSave));
  // Simulate call from Java to dismiss message on secondary button click.
  DismissMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

void SaveUpdatePasswordMessageDelegateTest::DismissMessage(
    messages::DismissReason dismiss_reason) {
  ExpectDismissMessageCall();
  delegate_->DismissSaveUpdatePasswordMessage(dismiss_reason);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void SaveUpdatePasswordMessageDelegateTest::DestroyDelegate() {
  delegate_.reset();
}

messages::MessageWrapper*
SaveUpdatePasswordMessageDelegateTest::GetMessageWrapper() {
  return delegate_->message_.get();
}

std::unique_ptr<PasswordEditDialog>
SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog(
    content::WebContents* web_contents,
    PasswordEditDialog::DialogAcceptedCallback dialog_accepted_callback,
    PasswordEditDialog::LegacyDialogAcceptedCallback
        legacy_dialog_accepted_callback,
    PasswordEditDialog::DialogDismissedCallback dialog_dismissed_callback) {
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);
  legacy_dialog_accepted_callback_ = std::move(legacy_dialog_accepted_callback);
  dialog_dismissed_callback_ = std::move(dialog_dismissed_callback);
  return std::move(mock_password_edit_dialog_);
}

MockPasswordEditDialog*
SaveUpdatePasswordMessageDelegateTest::PreparePasswordEditDialog() {
  mock_password_edit_dialog_ = std::make_unique<MockPasswordEditDialog>();
  return mock_password_edit_dialog_.get();
}

base::MockCallback<RepeatingCallback<
    void(gfx::NativeWindow,
         Profile*,
         password_manager::metrics_util::PasswordMigrationWarningTriggers)>>&
SaveUpdatePasswordMessageDelegateTest::GetMigrationWarningCallback() {
  return mock_password_migration_warning_callback_;
}

void SaveUpdatePasswordMessageDelegateTest::TriggerDialogAcceptedCallback(
    const std::u16string& username,
    const std::u16string& password) {
  std::move(dialog_accepted_callback_).Run(username, password);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerLegacyDialogAcceptedCallback(
    int selected_username_index) {
  std::move(legacy_dialog_accepted_callback_).Run(selected_username_index);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerDialogDismissedCallback(
    bool dialog_accepted) {
  std::move(dialog_dismissed_callback_).Run(dialog_accepted);
}

std::u16string
SaveUpdatePasswordMessageDelegateTest::GetExpectedUPMMessageDescription(
    bool is_update,
    bool is_signed_in,
    const std::u16string& account_email) {
  if (is_signed_in) {
    return l10n_util::GetStringFUTF16(
        is_update
            ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION
            : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION,
        account_email);
  }
  return l10n_util::GetStringUTF16(
      is_update
          ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION
          : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION);
}

void SaveUpdatePasswordMessageDelegateTest::CommitPasswordFormMetrics() {
  // PasswordFormMetricsRecorder::dtor commits accumulated metrics.
  metrics_recorder_.reset();
}

void SaveUpdatePasswordMessageDelegateTest::VerifyUkmMetrics(
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

void SaveUpdatePasswordMessageDelegateWithFeaturesTest::InitFeatureList() {
  std::vector<FeatureRefAndParams> enabled_features;
  std::vector<FeatureRef> disabled_features;

  FeatureConfigTestParam feature_config = GetParam();
  if (feature_config.with_unified_password_manager_android) {
    enabled_features.push_back(
        {password_manager::features::kUnifiedPasswordManagerAndroid, {}});
  } else {
    disabled_features.push_back(
        password_manager::features::kUnifiedPasswordManagerAndroid);
    disabled_features.push_back(
        password_manager::features::kUnifiedPasswordManagerAndroidBranding);
  }

  if (feature_config.with_exploratory_save_update_password_strings) {
    enabled_features.push_back(
        {password_manager::features::kExploratorySaveUpdatePasswordStrings,
         {{password_manager::features::kSaveUpdatePromptSyncingStringVersion
               .name,
           base::NumberToString(
               feature_config.save_update_prompt_syncing_string_version)}}});
  } else {
    disabled_features.push_back(
        password_manager::features::kExploratorySaveUpdatePasswordStrings);
  }

  scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                     disabled_features);
}

std::u16string SaveUpdatePasswordMessageDelegateWithFeaturesTest::
    GetExploratoryStringsMessageDescription(bool is_update,
                                            bool is_signed_in,
                                            const std::u16string& account_email,
                                            int new_string_version) {
  if (!is_signed_in) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_SAVE_UPDATE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION_V1);
  }

  switch (new_string_version) {
    case 2:
      return l10n_util::GetStringFUTF16(
          is_update
              ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_V2
              : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_V2,
          account_email);
    case 3:
      return l10n_util::GetStringFUTF16(
          is_update
              ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_V3
              : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_V3,
          account_email);
    default:
      ADD_FAILURE() << "All string version param values should be handled";
      return u"";
  }
}

// Tests that secondary menu icon is set for the save password message
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       CogButton_SavePassword_PasswordEditDialogWithDetails) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitWithFeatures(
      {password_manager::features::kPasswordEditDialogWithDetails,
       password_manager::features::kUnifiedPasswordManagerAndroidBranding},
      {});
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);

  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that secondary menu icon is set for the update password message
// in case when user has only single credential stored for the web site
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       CogButton_SingleCredUpdatePassword_PasswordEditDialogWithDetails) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitWithFeatures(
      {password_manager::features::kPasswordEditDialogWithDetails,
       password_manager::features::kUnifiedPasswordManagerAndroidBranding},
      {});
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);

  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that secondary menu icon is not set for the update password message
// in case when user has multiple credentials stored for the web site
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NoCogButton_MultipleCredUpdatePassword_PasswordEditDialogWithDetails) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitWithFeatures(
      {password_manager::features::kPasswordEditDialogWithDetails,
       password_manager::features::kUnifiedPasswordManagerAndroidBranding},
      {});
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);

  EXPECT_EQ(0, GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveUpdatePasswordMessageDelegateTest, OnlyOnePromptAtATime) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);

  ExpectDismissMessageCall();
  auto form_manager2 =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager2), /*user_signed_in=*/true,
                 /*update_password=*/false);
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that password form is saved and metrics recorded correctly when the
// user clicks "Save" button.
TEST_F(SaveUpdatePasswordMessageDelegateTest, SaveOnActionClick) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that the local password migration warning will show when the user
// clicks the "Save" button.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerLocalPasswordMigrationWarning_OnSaveClicked) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(
      GetMigrationWarningCallback(),
      Run(_, _,
          password_manager::metrics_util::PasswordMigrationWarningTriggers::
              kPasswordSaveUpdateMessage));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerLocalPasswordMigrationWarning_OnSavePasswordDialogAccepted) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  TriggerPasswordEditDialog(/*update_password=*/false);

  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*form_manager_pointer, Save());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
}

// Tests that the local password migration warning will not show when the user
// dismisses the save password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerLocalPasswordMigrationWarning_OnSaveMessageDismissed) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will show when the user
// accepts the update password message in case when there is no confirmation
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerLocalPasswordMigrationWarning_OnUpdatePasswordWithSingleForm) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  SetPendingCredentials(kUsername, kPassword);
  PasswordForm password_form = CreatePasswordForm(kUsername, kPassword);
  std::vector<const PasswordForm*> single_form_best_matches = {&password_form};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), &single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will not show when the user
// dismisses the update password message.
TEST_F(
    SaveUpdatePasswordMessageDelegateTest,
    DontTriggerLocalPasswordMigrationWarning_OnUpdatePasswordMessageDismissed) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  SetPendingCredentials(kUsername, kPassword);
  PasswordForm password_form = CreatePasswordForm(kUsername, kPassword);
  std::vector<const PasswordForm*> single_form_best_matches = {&password_form};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), &single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will show when the user
// accepts the update password message and the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerLocalPasswordMigrationWarning_OnUpdatePasswordDialogAccepted) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(
      *mock_dialog,
      ShowPasswordEditDialog(
          ElementsAre(std::u16string(kUsername), std::u16string(kUsername2)),
          Eq(kUsername), Eq(kPassword), Eq(kAccountEmail)));
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  TriggerActionClick();
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
}

// Tests that the local password migration warning will show when the user
// accepts the update password message and cancels the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerLocalPasswordMigrationWarning_OnUpdatePasswordDialogCanceled) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(
      *mock_dialog,
      ShowPasswordEditDialog(
          ElementsAre(std::u16string(kUsername), std::u16string(kUsername2)),
          Eq(kUsername), Eq(kPassword), Eq(kAccountEmail)));
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  TriggerActionClick();
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);
}

// Tests that password form is not saved and metrics recorded correctly when the
// user dismisses the message.
TEST_F(SaveUpdatePasswordMessageDelegateTest, DontSaveOnDismiss) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

// Tests that password form is not saved and metrics recorded correctly when the
// message is autodismissed.
TEST_F(SaveUpdatePasswordMessageDelegateTest, MetricOnAutodismissTimer) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

// Tests that the local password migration warning will not show when the user
// lets the save message time out.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerLocalPasswordMigrationWarning_OnSaveMessageAutodismissTimer) {
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will not show when the user
// lets the update message time out.
TEST_F(
    SaveUpdatePasswordMessageDelegateTest,
    DontTriggerLocalPasswordMigrationWarning_OnUpdateMessageAutodismissTimer) {
  SetPendingCredentials(kUsername, kPassword);
  PasswordForm password_form = CreatePasswordForm(kUsername, kPassword);
  std::vector<const PasswordForm*> single_form_best_matches = {&password_form};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), &single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that update password message with a single PasswordForm immediately
// saves the form on Update button tap and doesn't display confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, UpdatePasswordWithSingleForm) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  SetPendingCredentials(kUsername, kPassword);
  PasswordForm password_form = CreatePasswordForm(kUsername, kPassword);
  std::vector<const PasswordForm*> single_form_best_matches = {&password_form};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), &single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that the update dialog is shown after the message in case if multiple
// password match the form.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggeredEditDialogLegacy_TwoFormsMatching_UpdatePassword) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  // Verify parameters to Show() call.
  EXPECT_CALL(*mock_dialog, ShowLegacyPasswordEditDialog(
                                ElementsAre(std::u16string(kUsername),
                                            std::u16string(kUsername2)),
                                0, kAccountEmail));
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  TriggerActionClick();
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);
}

// Tests triggering password edit dialog and saving credentials after the
// user accepts the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, TriggerEditDialogLegacy_Accept) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, ShowLegacyPasswordEditDialog);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerLegacyDialogAcceptedCallback(/*selected_username_index=*/0);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests triggering password edit dialog and saving credentials with
// empty username after the user accepts the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerEditDialogLegacy_WithEmptyUsername_Accept) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  SetPendingCredentials(kUsername, kPassword);
  PasswordForm any_pasword_form = CreatePasswordForm(kUsername, kPassword);
  PasswordForm empty_username_password_form =
      CreatePasswordForm(u"", kPassword);
  std::vector<const PasswordForm*> best_matches = {
      &any_pasword_form, &empty_username_password_form};

  auto form_manager = CreateFormManager(GURL(kDefaultUrl), &best_matches);
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, ShowLegacyPasswordEditDialog);
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_CALL(*form_manager_pointer, Save());
  EXPECT_CALL(*form_manager_pointer,
              OnUpdateUsernameFromPrompt(testing::Eq(u"")));
  TriggerLegacyDialogAcceptedCallback(/*selected_username_index=*/1);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
}

// Tests that credentials are not saved if the user cancels password edit
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, TriggerEditDialogLegacy_Cancel) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save).Times(0);
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, ShowLegacyPasswordEditDialog);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

// Verifies that:
// 1. Username confirmation dialog is shown after clicking on 'Continue'
// in the message.
// 2. Saving the password form is executed after clicking on Update button of
// the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerConfirmUsernameDialog_Accept) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(
      kSaveUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::
              kAcceptInUsernameConfirmDialog,
      1);
  histogram_tester.ExpectUniqueSample(
      kConfirmUsernameMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::
              kAcceptInUsernameConfirmDialog,
      1);
}
// Verifies that:
// 1. Save password dialog is shown after clicking on cog menu item
// "Edit password"in the message.
// 2. Saving the password form is executed after clicking on Save button of the
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_Accept) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);

  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*form_manager_pointer, Save());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  // The real password edit dialog triggers dialog dismissed delegate inside.
  // Here we use the mock that doesn't do this, so the dismiss is called
  // manually here.
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(
      kSaveUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kAcceptInDialog,
      1);
  histogram_tester.ExpectUniqueSample(
      kSavePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kAcceptInDialog,
      1);
}

// Verifies that the site is blocklisted after clicking on
// "Never for this site" menu option in Save message
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_NeverSave) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*form_manager_pointer, Blocklist());
  TriggerNeverSaveMenuItem();

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_NEVER, 1);
  histogram_tester.ExpectUniqueSample(
      kSaveUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kNeverSave,
      1);
  histogram_tester.ExpectUniqueSample(
      kSavePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kNeverSave,
      1);
}

// Verifies that the password migration warning is not shown after selecting
// "Never for this site" menu option in the Save message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerLocalPasswordMigrationWarning_OnNeverSave) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  EXPECT_CALL(*form_manager_pointer, Blocklist());
  TriggerNeverSaveMenuItem();
}

// Verifies that:
// 1. Update password dialog is shown after clicking on cog button (secondary
// action) in the message.
// 2. Updating the password form is executed after clicking on Update button of
// the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerUpdateMessage_CogButton_Accept) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/true);

  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*form_manager_pointer, Save());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);

  // The real password edit dialog triggers dialog dismissed delegate inside.
  // Here we use the mock that doesn't do this, so the dismiss is called
  // manually here.
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      kUpdateUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectUniqueSample(
      kSaveUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kAcceptInDialog,
      1);
  histogram_tester.ExpectUniqueSample(
      kUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kAcceptInDialog,
      1);
}

// Verifies that:
// 1. Save password dialog is shown after clicking on cog menu item
// "Edit password"in the message.
// 2. The dialog is dismissed with negative result after clicking on Cancel
// button.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_Cancel) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*form_manager_pointer, Save()).Times(0);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
  histogram_tester.ExpectUniqueSample(
      kSaveUpdatePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kCancelInDialog,
      1);
  histogram_tester.ExpectUniqueSample(
      kSavePasswordMessageDismissalReason,
      SaveUpdatePasswordMessageDelegate::
          SaveUpdatePasswordMessageDismissReason::kCancelInDialog,
      1);
}

// Tests that if the exploratory strings feature is given an unsupported
// string version as a param, the regular strings are used instead.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       ExploratoryStringsWithWrongParamFallsBackToRegular) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeatureWithParameters(
      password_manager::features::kExploratorySaveUpdatePasswordStrings,
      {{password_manager::features::kSaveUpdatePromptSyncingStringVersion.name,
        "1"}});
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool kUserNotSignedIn = false;
  const bool kNotUpdate = false;
  EnqueueMessage(std::move(form_manager), kUserNotSignedIn, kNotUpdate);
  EXPECT_EQ(GetExpectedUPMMessageDescription(kNotUpdate, kUserNotSignedIn,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests parameterized with different feature states

// Tests that message properties (title, description, icon, button text) are
// set correctly for save password message.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       MessagePropertyValues_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = false;
  const bool is_update = false;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());

  // Validate message description that depends on
  // kExploratorySaveUpdatePasswordStrings feature
  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountEmail16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountEmail16),
              GetMessageWrapper()->GetDescription());
  } else {
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail16));
  }

  // Validate remaining message fields
  if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(
                  IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP),
              GetMessageWrapper()->GetIconResourceId());
  } else {
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
              GetMessageWrapper()->GetTitle());
    EXPECT_EQ(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD),
        GetMessageWrapper()->GetIconResourceId());
  }

  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that secondary button title is right.
// kPasswordEditDialogWithDetails feature off.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       MessageSecondaryButtonProperty_SavePassword) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndDisableFeature(
      password_manager::features::kPasswordEditDialogWithDetails);

  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);

  if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MESSAGE_NEVER_SAVE_MENU_ITEM),
        GetMessageWrapper()->GetSecondaryButtonMenuText());
  } else {
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON),
              GetMessageWrapper()->GetSecondaryButtonMenuText());
  }
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for update password message.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       MessagePropertyValues_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = false;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_UPDATE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountEmail16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountEmail16),
              GetMessageWrapper()->GetDescription());
  }

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(std::u16string(),
            GetMessageWrapper()->GetSecondaryButtonMenuText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user saves a
// password.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       SignedInDescription_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = false;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountEmail16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountEmail16),
              GetMessageWrapper()->GetDescription());
  } else {
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail16));
  }
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed-in user with a
// non-displayable email saves a password.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       SignedInDescription_SavePasswordNonDisplayableEmail) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = false;

  absl::optional<AccountInfo> account_info;
  account_info = AccountInfo();
  account_info.value().email = kAccountEmail;
  account_info.value().full_name = kAccountFullName;
  AccountCapabilitiesTestMutator mutator(&account_info.value().capabilities);
  mutator.set_can_have_email_address_displayed(false);

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update, account_info);
  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountFullName16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountFullName16),
              GetMessageWrapper()->GetDescription());
  } else {
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail16));
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountFullName16));
  }
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user updates a
// password.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       SignedInDescription_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountEmail16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountEmail16),
              GetMessageWrapper()->GetDescription());
  } else {
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail16));
  }
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed-in user with a
// non-displayable email updates a password.
TEST_P(SaveUpdatePasswordMessageDelegateWithFeaturesTest,
       SignedInDescription_UpdatePasswordNonDisplayableEmail) {
  SetPendingCredentials(kUsername, kPassword);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = true;

  absl::optional<AccountInfo> account_info;
  account_info = AccountInfo();
  account_info.value().email = kAccountEmail;
  account_info.value().full_name = kAccountFullName;
  AccountCapabilitiesTestMutator mutator(&account_info.value().capabilities);
  mutator.set_can_have_email_address_displayed(false);

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update, account_info);
  if (GetParam().with_exploratory_save_update_password_strings) {
    // password_manager::features::kExploratorySaveUpdatePasswordStrings is
    // enabled
    EXPECT_EQ(GetExploratoryStringsMessageDescription(
                  is_update, is_signed_in, kAccountFullName16,
                  GetParam().save_update_prompt_syncing_string_version),
              GetMessageWrapper()->GetDescription());
  } else if (GetParam().with_unified_password_manager_android) {
    // password_manager::features::kUnifiedPasswordManagerAndroid is enabled
    EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                               kAccountFullName16),
              GetMessageWrapper()->GetDescription());
  } else {
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kUsername));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kPassword));
    EXPECT_EQ(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountEmail16));
    EXPECT_NE(std::u16string::npos,
              GetMessageWrapper()->GetDescription().find(kAccountFullName16));
  }
  DismissMessage(messages::DismissReason::UNKNOWN);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaveUpdatePasswordMessageDelegateWithFeaturesTest,
    testing::Values(
        // Exploratory strings are disabled, no version specified
        FeatureConfigTestParam{
            .with_unified_password_manager_android = false,
            .with_exploratory_save_update_password_strings = false},
        FeatureConfigTestParam{
            .with_unified_password_manager_android = true,
            .with_exploratory_save_update_password_strings = false},
        // Exploratory strings with UPM enabled
        FeatureConfigTestParam{
            .with_unified_password_manager_android = true,
            .with_exploratory_save_update_password_strings = true,
            .save_update_prompt_syncing_string_version = 2},
        FeatureConfigTestParam{
            .with_unified_password_manager_android = true,
            .with_exploratory_save_update_password_strings = true,
            .save_update_prompt_syncing_string_version = 3},
        // Exploratory strings with UPM disabled
        FeatureConfigTestParam{
            .with_unified_password_manager_android = false,
            .with_exploratory_save_update_password_strings = true,
            .save_update_prompt_syncing_string_version = 2},
        FeatureConfigTestParam{
            .with_unified_password_manager_android = false,
            .with_exploratory_save_update_password_strings = true,
            .save_update_prompt_syncing_string_version = 3}));
