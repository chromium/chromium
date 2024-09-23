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
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/password_manager/android/access_loss/mock_password_access_loss_warning_bridge.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
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

}  // namespace

namespace {
class TestDeviceLockBridge : public DeviceLockBridge {
 public:
  TestDeviceLockBridge() = default;

  TestDeviceLockBridge(const TestDeviceLockBridge&) = delete;
  TestDeviceLockBridge& operator=(const TestDeviceLockBridge&) = delete;

  bool ShouldShowDeviceLockUi() override { return should_show_device_lock_ui_; }

  bool RequiresDeviceLock() override { return requires_device_lock_; }

  void LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      ui::WindowAndroid* window_android,
      DeviceLockRequirementMetCallback callback) override {
    callback_ = std::move(callback);
    device_lock_ui_shown_count_++;
  }

  void SimulateDeviceLockComplete(bool is_device_lock_set) {
    std::move(callback_).Run(is_device_lock_set);
  }

  void SetShouldShowDeviceLockUi(bool should_show_device_lock_ui) {
    requires_device_lock_ = should_show_device_lock_ui;
    should_show_device_lock_ui_ = should_show_device_lock_ui;
  }

  int device_lock_ui_shown_count() { return device_lock_ui_shown_count_; }

 private:
  bool requires_device_lock_ = false;
  bool should_show_device_lock_ui_ = false;
  int device_lock_ui_shown_count_ = 0;
  DeviceLockRequirementMetCallback callback_;
};

}  // namespace

class MockPasswordEditDialog : public PasswordEditDialog {
 public:
  MOCK_METHOD(void,
              ShowPasswordEditDialog,
              (const std::vector<std::u16string>& usernames,
               const std::u16string& username,
               const std::u16string& password,
               const std::optional<std::string>& account_email),
              (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(void,
              ShowPasswordManagerErrorMessage,
              (password_manager::ErrorMessageFlowType,
               password_manager::PasswordStoreBackendErrorType),
              (override));
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
      const std::vector<PasswordForm>& best_matches);
  void RecordPasswordSaved();
  void SetPendingCredentials(std::u16string username,
                             std::u16string password,
                             bool is_account_store = false);
  static PasswordForm CreatePasswordForm(std::u16string username,
                                         std::u16string password,
                                         bool is_account_store = false);

  void EnqueueMessage(std::unique_ptr<PasswordFormManagerForUI> form_to_save,
                      bool user_signed_in,
                      bool update_password,
                      std::optional<AccountInfo> account_info = {});
  void TriggerActionClick();
  void TriggerActionClick(messages::DismissReason dismiss_reason);
  void TriggerPasswordEditDialog(bool update_password);
  void TriggerNeverSaveMenuItem();

  void ExpectDismissMessageCall();
  void DismissMessage(messages::DismissReason dismiss_reason);
  void DestroyDelegate();

  TestDeviceLockBridge* test_bridge();
  MockPasswordAccessLossWarningBridge* mock_access_loss_warning_bridge();
  bool is_password_saved();

  messages::MessageWrapper* GetMessageWrapper();
  MockPasswordManagerClient* GetClient();

  // Password edit dialog factory function that is passed to
  // SaveUpdatePasswordMessageDelegate. Passes the dialog prepared by
  // PreparePasswordEditDialog. Captures accept and dismiss callbacks.
  std::unique_ptr<PasswordEditDialog> CreatePasswordEditDialog(
      content::WebContents* web_contents,
      PasswordEditDialogBridgeDelegate* pasword_edit_dialog_bridge_delegate);

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
  void TriggerDialogDismissedCallback(bool dialog_accepted);

  // TODO(crbug.com/40900579): Remove this helper as it makes tests hard to
  // read.
  std::u16string GetExpectedUPMMessageDescription(
      bool is_update,
      bool is_signed_in,
      const std::u16string& account_email);
  void CommitPasswordFormMetrics();
  void VerifyUkmMetrics(const ukm::TestUkmRecorder& ukm_recorder,
                        PasswordFormMetricsRecorder::BubbleDismissalReason
                            expected_dismissal_reason);
  void EnableUseUPMLocalAndSeparateStores();

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  std::vector<PasswordForm> empty_best_matches() { return {}; }

  std::vector<PasswordForm> two_forms_best_matches() {
    return {CreatePasswordForm(kUsername, kPassword),
            CreatePasswordForm(kUsername2, kPassword)};
  }

  PasswordEditDialogBridgeDelegate* get_password_edit_dialog_bridge_delegate() {
    return delegate_.get();
  }

  void FastForward() { task_environment()->FastForwardBy(base::Seconds(1)); }

 private:
  PasswordForm pending_credentials_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockPasswordEditDialog> mock_password_edit_dialog_;
  base::MockCallback<RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      mock_password_migration_warning_callback_;
  raw_ptr<TestDeviceLockBridge> test_bridge_;
  raw_ptr<MockPasswordAccessLossWarningBridge> mock_access_loss_warning_bridge_;
  std::unique_ptr<SaveUpdatePasswordMessageDelegate> delegate_;
  bool is_password_saved_ = false;
  MockPasswordManagerClient password_manager_client_;
};

SaveUpdatePasswordMessageDelegateTest::SaveUpdatePasswordMessageDelegateTest()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

void SaveUpdatePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/);
  NavigateAndCommit(GURL(kDefaultUrl));

  auto bridge = std::make_unique<TestDeviceLockBridge>();
  test_bridge_ = bridge.get();
  auto access_loss_bridge =
      std::make_unique<MockPasswordAccessLossWarningBridge>();
  mock_access_loss_warning_bridge_ = access_loss_bridge.get();
  delegate_ = std::make_unique<SaveUpdatePasswordMessageDelegate>(
      base::PassKey<class SaveUpdatePasswordMessageDelegateTest>(),
      base::BindRepeating(
          &SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog,
          base::Unretained(this)),
      mock_password_migration_warning_callback_.Get(), std::move(bridge),
      std::move(access_loss_bridge));

  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);

  ON_CALL(*(password_manager_client_.GetPasswordFeatureManager()),
          ShouldUpdateGmsCore)
      .WillByDefault(Return(false));
}

void SaveUpdatePasswordMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordFormManagerForUI>
SaveUpdatePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url,
    const std::vector<PasswordForm>& best_matches) {
  password_form_url_ = password_form_url;
  auto form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(ReturnRef(pending_credentials_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(Return(password_manager::metrics_util::
                                CredentialSourceType::kPasswordManager));
  ON_CALL(*form_manager, GetURL()).WillByDefault(ReturnRef(password_form_url_));
  ON_CALL(*form_manager, GetBestMatches()).WillByDefault(Return(best_matches));
  ON_CALL(*form_manager, GetFederatedMatches())
      .WillByDefault(Return(base::span<const PasswordForm>()));
  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(Return(metrics_recorder_.get()));
  ON_CALL(*form_manager, Save())
      .WillByDefault(testing::Invoke(
          this, &SaveUpdatePasswordMessageDelegateTest::RecordPasswordSaved));
  ON_CALL(*form_manager, GetPasswordStoreForSaving(_))
      .WillByDefault([](const PasswordForm& form) -> PasswordForm::Store {
        return form.IsUsingAccountStore() ? PasswordForm::Store::kAccountStore
                                          : PasswordForm::Store::kProfileStore;
      });
  return form_manager;
}

void SaveUpdatePasswordMessageDelegateTest::RecordPasswordSaved() {
  is_password_saved_ = true;
}

void SaveUpdatePasswordMessageDelegateTest::SetPendingCredentials(
    std::u16string username,
    std::u16string password,
    bool is_account_store) {
  pending_credentials_.username_value = std::move(username);
  pending_credentials_.password_value = std::move(password);
  pending_credentials_.in_store =
      is_account_store ? password_manager::PasswordForm::Store::kAccountStore
                       : password_manager::PasswordForm::Store::kProfileStore;
}

// static
PasswordForm SaveUpdatePasswordMessageDelegateTest::CreatePasswordForm(
    std::u16string username,
    std::u16string password,
    bool is_account_store) {
  PasswordForm password_form;
  password_form.username_value = std::move(username);
  password_form.password_value = std::move(password);
  password_form.match_type = PasswordForm::MatchType::kExact;
  password_form.in_store =
      is_account_store ? password_manager::PasswordForm::Store::kAccountStore
                       : password_manager::PasswordForm::Store::kProfileStore;
  return password_form;
}

void SaveUpdatePasswordMessageDelegateTest::EnqueueMessage(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool user_signed_in,
    bool update_password,
    std::optional<AccountInfo> account_info) {
  if (user_signed_in && !account_info) {
    account_info = AccountInfo();
    account_info.value().email = kAccountEmail;
  }
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  delegate_->DisplaySaveUpdatePasswordPromptInternal(
      web_contents(), std::move(form_to_save), account_info, update_password,
      &password_manager_client_);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerActionClick() {
  TriggerActionClick(messages::DismissReason::PRIMARY_ACTION);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerActionClick(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // Simulate call from Java to dismiss message on primary button click.
  DismissMessage(dismiss_reason);
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

TestDeviceLockBridge* SaveUpdatePasswordMessageDelegateTest::test_bridge() {
  return test_bridge_;
}

MockPasswordAccessLossWarningBridge*
SaveUpdatePasswordMessageDelegateTest::mock_access_loss_warning_bridge() {
  return mock_access_loss_warning_bridge_;
}

bool SaveUpdatePasswordMessageDelegateTest::is_password_saved() {
  return is_password_saved_;
}

messages::MessageWrapper*
SaveUpdatePasswordMessageDelegateTest::GetMessageWrapper() {
  return delegate_->message_.get();
}

MockPasswordManagerClient* SaveUpdatePasswordMessageDelegateTest::GetClient() {
  return &password_manager_client_;
}

std::unique_ptr<PasswordEditDialog>
SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog(
    content::WebContents* web_contents,
    PasswordEditDialogBridgeDelegate* pasword_edit_dialog_bridge_delegate) {
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
  // std::move(dialog_accepted_callback_).Run(username, password);
  delegate_->HandleSavePasswordFromDialog(username, password);
}

void SaveUpdatePasswordMessageDelegateTest::TriggerDialogDismissedCallback(
    bool dialog_accepted) {
  // std::move(dialog_dismissed_callback_).Run(dialog_accepted);

  delegate_->HandleDialogDismissed(dialog_accepted);
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
  for (const ukm::mojom::UkmEntry* entry : entries) {
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

void SaveUpdatePasswordMessageDelegateTest::
    EnableUseUPMLocalAndSeparateStores() {
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
}

// Tests that secondary menu icon is set for the save password message
TEST_F(SaveUpdatePasswordMessageDelegateTest, CogButton_SavePassword) {
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
       CogButton_SingleCredUpdatePassword) {
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
       NoCogButton_MultipleCredUpdatePassword) {
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

// Tests that the access loss warning will show when the user
// clicks the "Save" button.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerAccessLossWarning_OnSaveClicked) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_access_loss_warning_bridge(),
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the message to update GMSCore will show when the user
// clicks the "Save" button if the GMSCore version is too low to save account
// passwords.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NudgeToUpdateGmsCore_OnSaveClicked) {
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible));
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(true));
  TriggerActionClick();

  // Fast forward, since Update message is shown with a delay.
  FastForward();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the message to update GMSCore will not show when the user
// clicks the "Save" button.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontNudgeToUpdateGmsCore_OnSaveClicked) {
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible))
      .Times(0);
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(false));
  TriggerActionClick();

  // Fast forward, since Update message is shown with a delay.
  FastForward();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will show when the user
// accepts the password edit dialog.
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

// Tests that the password access loss warning will show when the user
// accepts the password edit dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerAccessLossWarning_OnSavePasswordDialogAccepted) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
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
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_access_loss_warning_bridge(),
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage));
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

// Tests that the access loss warning will not show when the user
// dismisses the save password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerAccessLossWarning_OnSaveMessageDismissed) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
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
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the access loss warning will show when the user accepts the update
// password message in case when there is no confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerAccessLossWarning_OnUpdatePasswordWithSingleForm) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_access_loss_warning_bridge(),
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the message to update GMSCore will show when the user accepts the
// update password message in case when there is no confirmation
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NudgeToUpdateGmsCore_OnUpdatePasswordWithSingleForm) {
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible));
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(true));
  TriggerActionClick();

  // Fast forward, since Update message is shown with a delay.
  FastForward();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the message to update GMSCore will not show when the user accepts
// the update password message in case when there is no confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontNudgeToUpdateGmsCore_OnUpdatePasswordWithSingleForm) {
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible))
      .Times(0);
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(false));
  TriggerActionClick();

  // Fast forward, since Update message is shown with a delay.
  FastForward();
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
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the access loss warning will not show when the user
// dismisses the update password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerAccessLossWarning_OnUpdatePasswordMessageDismissed) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
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

// Tests that the access loss warning will show when the user accepts the update
// password message and the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerAccessLossWarning_OnUpdatePasswordDialogAccepted) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
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
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
  TriggerActionClick();
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  EXPECT_CALL(
      *mock_access_loss_warning_bridge(),
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage));
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
}

// Tests that the message to update GMSCore will show when the user accepts the
// update password message and the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NudgeToUpdateGmsCore_OnUpdatePasswordDialogAccepted) {
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
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible));
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(true));
  TriggerActionClick();
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
  // Fast forward, since Update message is shown with a delay.
  FastForward();
}

// Tests that the message to update GMSCore will not show when the user accepts
// the update password message and the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontNudgeToUpdateGmsCore_OnUpdatePasswordDialogAccepted) {
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
  EXPECT_CALL(*GetClient(),
              ShowPasswordManagerErrorMessage(
                  password_manager::ErrorMessageFlowType::kSaveFlow,
                  password_manager::PasswordStoreBackendErrorType::
                      kGMSCoreOutdatedSavingPossible))
      .Times(0);
  EXPECT_CALL(*(GetClient()->GetPasswordFeatureManager()), ShouldUpdateGmsCore)
      .WillOnce(Return(false));
  TriggerActionClick();
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  EXPECT_CALL(GetMigrationWarningCallback(), Run);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);
  // Fast forward, since Update message is shown with a delay.
  FastForward();
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

// Tests that the local password migration warning will show when the user
// accepts the update password message and cancels the confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerAccessLossWarning_OnUpdatePasswordDialogCanceled) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
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
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
  TriggerActionClick();
  EXPECT_CALL(
      *mock_access_loss_warning_bridge(),
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage));
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

// Tests that the access loss warning will not show when the user lets the save
// message time out.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerAccessLossWarning_OnSaveMessageAutodismissTimer) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the local password migration warning will not show when the user
// lets the update message time out.
TEST_F(
    SaveUpdatePasswordMessageDelegateTest,
    DontTriggerLocalPasswordMigrationWarning_OnUpdateMessageAutodismissTimer) {
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(GetMigrationWarningCallback(), Run).Times(0);
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that the access loss warning will not show when the user lets the
// update message time out.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerAccessLossWarning_OnUpdateMessageAutodismissTimer) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that update password message with a single PasswordForm immediately
// saves the form on Update button tap and doesn't display confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, UpdatePasswordWithSingleForm) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
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

// Verifies that:
// 1. Username confirmation dialog is shown after clicking on 'Continue'
// in the message.
// 2. Saving the password form is executed after clicking on Update button of
// the dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerConfirmUsernameDialog_Accept) {
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
}
// Verifies that:
// 1. Save password dialog is shown after clicking on cog menu item
// "Edit password"in the message.
// 2. Saving the password form is executed after clicking on Save button of the
// dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_Accept) {
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
}

// Verifies that the site is blocklisted after clicking on
// "Never for this site" menu option in Save message
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_NeverSave) {
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

// Verifies that the access loss warning is not shown after selecting
// "Never for this site" menu option in the Save message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontTriggerAccessLossWarning_OnNeverSave) {
  base::test::ScopedFeatureList scoped_feature_state;
  scoped_feature_state.InitAndEnableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);

  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* form_manager_pointer = form_manager.get();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
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
}

// Verifies that:
// 1. Save password dialog is shown after clicking on cog menu item
// "Edit password"in the message.
// 2. The dialog is dismissed with negative result after clicking on Cancel
// button.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TriggerSaveMessage_CogButton_Cancel) {
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
}

// Tests that password is saved if device lock UI is shown and device lock is
// set during a save password flow.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockUiShown_DeviceLockSet) {
  // Create a scoped window so that WebContents::GetNativeView::GetWindowAndroid
  // does not return null.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  test_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown but password is not saved yet.
  EXPECT_EQ(1, test_bridge()->device_lock_ui_shown_count());
  EXPECT_EQ(false, is_password_saved());

  // Verify that password is saved after receiving the callback that device lock
  // was set.
  test_bridge()->SimulateDeviceLockComplete(true);
  EXPECT_EQ(true, is_password_saved());
}

// Tests that password is updated if device lock UI is shown and device lock is
// set during an update password flow.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UpdatePassword_DeviceLockUiShown_DeviceLockSet) {
  // Create a scoped window so that WebContents::GetNativeView::GetWindowAndroid
  // does not return null.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  test_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown but password is not saved yet.
  EXPECT_EQ(1, test_bridge()->device_lock_ui_shown_count());
  EXPECT_EQ(false, is_password_saved());

  // Verify that password is updated after receiving the callback that device
  // lock was set.
  test_bridge()->SimulateDeviceLockComplete(true);
  EXPECT_EQ(true, is_password_saved());
}

// Tests that password is not saved if device lock UI is shown but device lock
// is not set during a save password flow.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockUiShown_DeviceLockNotSet) {
  // Create a scoped window so that WebContents::GetNativeView::GetWindowAndroid
  // does not return null.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  test_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown.
  EXPECT_EQ(1, test_bridge()->device_lock_ui_shown_count());

  // Verify that password is not saved after device lock was not set.
  test_bridge()->SimulateDeviceLockComplete(false);
}

// Tests that password is not saved if device lock UI needs to be shown but is
// not.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockUiNotShown) {
  test_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick(messages::DismissReason::UNKNOWN);
}

// Tests parameterized with different feature states

// Tests that message properties (title, description, icon, button text) are
// set correctly for save password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
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
  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(
                IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP),
            GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS),
            GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that message properties (title, description, icon, button text) are
// set correctly for update password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/false);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = false;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_UPDATE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(std::u16string(),
            GetMessageWrapper()->GetSecondaryButtonMenuText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user saves a
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_SavePassword) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = false;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed-in user with a
// non-displayable email saves a password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_SavePasswordNonDisplayableEmail) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = false;

  std::optional<AccountInfo> account_info;
  account_info = AccountInfo();
  account_info.value().email = kAccountEmail;
  account_info.value().full_name = kAccountFullName;
  AccountCapabilitiesTestMutator mutator(&account_info.value().capabilities);
  mutator.set_can_have_email_address_displayed(false);

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update, account_info);

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountFullName16),
            GetMessageWrapper()->GetDescription());
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user updates a
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_UpdatePassword) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed in user updated
// the password in the local store.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_UpdatePasswordInAccountStore) {
  // Enables using split storages (local and account).
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword, true)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  const bool is_signed_in = true;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed in user updated
// the password in the local store.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedOutDescription_UpdatePasswordInLocalStore) {
  // Enables using split storages (local and account).
  EnableUseUPMLocalAndSeparateStores();

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/false);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword, false)};
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), single_form_best_matches);
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/is_update);

  // Should display signed out message for updating the password in the local
  // store (even when the user is signed in).
  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, false, kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when the signed-in user with a
// non-displayable email updates a password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_UpdatePasswordNonDisplayableEmail) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = true;

  std::optional<AccountInfo> account_info;
  account_info = AccountInfo();
  account_info.value().email = kAccountEmail;
  account_info.value().full_name = kAccountFullName;
  AccountCapabilitiesTestMutator mutator(&account_info.value().capabilities);
  mutator.set_can_have_email_address_displayed(false);

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update, account_info);
  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountFullName16),
            GetMessageWrapper()->GetDescription());
  DismissMessage(messages::DismissReason::UNKNOWN);
}

TEST_F(SaveUpdatePasswordMessageDelegateTest, RecordsPromptShownWhenEnqueuing) {
  base::HistogramTester histogram_tester;
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  auto form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FormSubmissionsVsSavePrompts",
      password_manager::metrics_util::SaveFlowStep::kSavePromptShown, 1);
  DismissMessage(messages::DismissReason::UNKNOWN);
}
