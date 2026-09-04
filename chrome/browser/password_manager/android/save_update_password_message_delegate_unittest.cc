// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"

#include <algorithm>
#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge_delegate.h"
#include "chrome/browser/password_manager/android/mock_password_manager_error_message_helper_bridge.h"
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
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_ui_types.h"
#include "url/gurl.h"

namespace {

using ::password_manager::MockPasswordFormManagerForUI;
using ::password_manager::PasswordForm;
using ::password_manager::PasswordFormManagerForUI;
using ::password_manager::PasswordFormMetricsRecorder;
using password_manager::PasswordString;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;

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
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(const syncer::SyncService*,
              GetSyncService,
              (),
              (const, override));
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
                             bool is_account_store = false,
                             url::SchemeHostPort federation_origin = {});
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
  void ExpectConfirmationMessageDismissCall();
  void DismissMessage(messages::DismissReason dismiss_reason);
  void DestroyDelegate();
  void DismissAllActiveUI();

  TestDeviceLockBridge* test_device_lock_bridge();
  MockPasswordManagerErrorMessageHelperBridge* helper_bridge();
  bool is_password_saved();
  bool IsDelegateStateCleared();

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
                            expected_dismissal_reason,
                        bool update_password);

  SaveUpdatePasswordMessageDelegate* delegate() { return delegate_.get(); }

  void DisplaySaveUpdatePasswordPromptInternal(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      std::optional<AccountInfo> account_info,
      bool update_password) {
    delegate_->DisplaySaveUpdatePasswordPromptInternal(
        web_contents(), std::move(form_to_save), account_info, update_password,
        &password_manager_client_);
  }

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

  scoped_refptr<password_manager::TestPasswordStore> password_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_;

 private:
  PasswordForm pending_credentials_;
  GURL password_form_url_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;
  ukm::SourceId ukm_source_id_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockPasswordEditDialog> mock_password_edit_dialog_;
  raw_ptr<TestDeviceLockBridge> test_device_lock_bridge_ = nullptr;
  std::unique_ptr<SaveUpdatePasswordMessageDelegate> delegate_;
  bool is_password_saved_ = false;
  MockPasswordManagerClient password_manager_client_;
  std::vector<password_manager::StoredCredential> best_matches_;
  // The `helper_bridge_` is owned by the `delegate_`.
  raw_ptr<MockPasswordManagerErrorMessageHelperBridge> helper_bridge_ = nullptr;
  syncer::TestSyncService sync_service_;
};

SaveUpdatePasswordMessageDelegateTest::SaveUpdatePasswordMessageDelegateTest()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

void SaveUpdatePasswordMessageDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      true /*is_main_frame_secure*/, ukm_source_id_, nullptr /*pref_service*/,
      password_manager_client_.GetProfileMetricsService());
  NavigateAndCommit(GURL(kDefaultUrl));

  password_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>();
  password_store_->Init();
  account_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>();
  account_store_->Init();
  ON_CALL(password_manager_client_, GetProfilePasswordStore())
      .WillByDefault(Return(password_store_.get()));
  ON_CALL(password_manager_client_, GetAccountPasswordStore())
      .WillByDefault(Return(account_store_.get()));
  ON_CALL(password_manager_client_, GetSyncService())
      .WillByDefault(Return(&sync_service_));

  std::unique_ptr<TestDeviceLockBridge> device_lock_bridge =
      std::make_unique<TestDeviceLockBridge>();
  test_device_lock_bridge_ = device_lock_bridge.get();
  std::unique_ptr<MockPasswordManagerErrorMessageHelperBridge>
      mock_helper_bridge =
          std::make_unique<MockPasswordManagerErrorMessageHelperBridge>();
  helper_bridge_ = mock_helper_bridge.get();
  delegate_ = std::make_unique<SaveUpdatePasswordMessageDelegate>(
      base::PassKey<class SaveUpdatePasswordMessageDelegateTest>(),
      base::BindRepeating(
          &SaveUpdatePasswordMessageDelegateTest::CreatePasswordEditDialog,
          base::Unretained(this)),
      std::move(device_lock_bridge), std::move(mock_helper_bridge));

  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void SaveUpdatePasswordMessageDelegateTest::TearDown() {
  if (delegate_) {
    delegate_->DismissAllActiveUI();
  }
  delegate_.reset();
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  password_store_->ShutdownOnUIThread();
  account_store_->ShutdownOnUIThread();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordFormManagerForUI>
SaveUpdatePasswordMessageDelegateTest::CreateFormManager(
    const GURL& password_form_url,
    const std::vector<PasswordForm>& best_matches) {
  password_form_url_ = password_form_url;
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      std::make_unique<testing::NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*form_manager, GetPendingCredentials())
      .WillByDefault(ReturnRef(pending_credentials_));
  ON_CALL(*form_manager, GetCredentialSource())
      .WillByDefault(Return(password_manager::metrics_util::
                                CredentialSourceType::kPasswordManager));
  ON_CALL(*form_manager, GetURL()).WillByDefault(ReturnRef(password_form_url_));
  best_matches_ = password_manager::FromPasswordForms(best_matches);
  ON_CALL(*form_manager, GetBestMatches()).WillByDefault([this]() {
    return base::span<const password_manager::StoredCredential>(best_matches_);
  });
  ON_CALL(*form_manager, GetFederatedMatches())
      .WillByDefault(
          Return(base::span<const password_manager::StoredCredential>()));
  ON_CALL(*form_manager, GetMetricsRecorder())
      .WillByDefault(Return(metrics_recorder_.get()));
  ON_CALL(*form_manager, IsFetchCompleted()).WillByDefault(Return(true));
  ON_CALL(*form_manager, Save()).WillByDefault([this]() {
    RecordPasswordSaved();
  });
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
    bool is_account_store,
    url::SchemeHostPort federation_origin) {
  pending_credentials_.username_value = std::move(username);
  pending_credentials_.password_value = PasswordString(std::move(password));
  pending_credentials_.in_store =
      is_account_store ? password_manager::PasswordForm::Store::kAccountStore
                       : password_manager::PasswordForm::Store::kProfileStore;
  pending_credentials_.federation_origin = federation_origin;
}

// static
PasswordForm SaveUpdatePasswordMessageDelegateTest::CreatePasswordForm(
    std::u16string username,
    std::u16string password,
    bool is_account_store) {
  PasswordForm password_form;
  password_form.username_value = std::move(username);
  password_form.password_value = PasswordString(std::move(password));
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
    account_info =
        AccountInfo::Builder(GaiaId("test_gaia"), kAccountEmail).Build();
  }
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(Return(true));
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
  ExpectDismissMessageCall();
  if (update_password) {
    GetMessageWrapper()->HandleSecondaryActionClick(
        base::android::AttachCurrentThread());
  } else {
    GetMessageWrapper()->HandleSecondaryMenuItemSelected(
        base::android::AttachCurrentThread(),
        static_cast<int>(SaveUpdatePasswordMessageDelegate::
                             SavePasswordDialogMenuItem::kEditPassword));
  }
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void SaveUpdatePasswordMessageDelegateTest::TriggerNeverSaveMenuItem() {
  ExpectDismissMessageCall();
  GetMessageWrapper()->HandleSecondaryMenuItemSelected(
      base::android::AttachCurrentThread(),
      static_cast<int>(SaveUpdatePasswordMessageDelegate::
                           SavePasswordDialogMenuItem::kNeverSave));
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void SaveUpdatePasswordMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_,
              DismissMessage(GetMessageWrapper(), _))
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      })
      .RetiresOnSaturation();
}

void SaveUpdatePasswordMessageDelegateTest::
    ExpectConfirmationMessageDismissCall() {
  EXPECT_CALL(message_dispatcher_bridge_,
              DismissMessage(delegate_->confirmation_message_.get(), _))
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

void SaveUpdatePasswordMessageDelegateTest::DismissAllActiveUI() {
  delegate_->DismissAllActiveUI();
}

TestDeviceLockBridge*
SaveUpdatePasswordMessageDelegateTest::test_device_lock_bridge() {
  return test_device_lock_bridge_;
}

MockPasswordManagerErrorMessageHelperBridge*
SaveUpdatePasswordMessageDelegateTest::helper_bridge() {
  return helper_bridge_;
}

bool SaveUpdatePasswordMessageDelegateTest::is_password_saved() {
  return is_password_saved_;
}

bool SaveUpdatePasswordMessageDelegateTest::IsDelegateStateCleared() {
  return !delegate_ || delegate_->web_contents_ == nullptr;
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
        expected_dismissal_reason,
    bool update_password) {
  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = ukm_recorder.GetEntriesByName(
          ukm::builders::PasswordForm::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    EXPECT_EQ(ukm_source_id_, entry->source_id);
    if (update_password) {
      ukm_recorder.ExpectEntryMetric(
          entry, ukm::builders::PasswordForm::kUpdating_Prompt_ShownName, 1);
      ukm_recorder.ExpectEntryMetric(
          entry, ukm::builders::PasswordForm::kUpdating_Prompt_TriggerName,
          static_cast<int64_t>(PasswordFormMetricsRecorder::BubbleTrigger::
                                   kPasswordManagerSuggestionAutomatic));
      ukm_recorder.ExpectEntryMetric(
          entry, ukm::builders::PasswordForm::kUpdating_Prompt_InteractionName,
          static_cast<int64_t>(expected_dismissal_reason));
    } else {
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
}

// Tests that secondary menu icon is set for the save password message
TEST_F(SaveUpdatePasswordMessageDelegateTest, CogButton_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);

  EXPECT_EQ(0, GetMessageWrapper()->GetSecondaryIconResourceId());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the previous prompt gets dismissed when the new one is enqueued.
TEST_F(SaveUpdatePasswordMessageDelegateTest, OnlyOnePromptAtATime) {
  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);

  ExpectDismissMessageCall();
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager2 =
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
      /*update_password=*/false);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SaveWithTrustedVaultError.Outcome", 0);
}

// Tests that password form is not saved and metrics recorded correctly when the
// user dismisses the message.
TEST_F(SaveUpdatePasswordMessageDelegateTest, DontSaveOnDismiss) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined,
      /*update_password=*/false);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_CANCEL, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SaveWithTrustedVaultError.Outcome", 0);
}

// Tests that the trusted vault key retrieval flow is started when the
// user clicks the "Save" button, in case the Trusted Vault was locked.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       StartTrustedVaultKeyRetrievalFlowOnSavePassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);
  std::unique_ptr<password_manager::FakePasswordStoreBackend>
      fake_account_backend =
          std::make_unique<password_manager::FakePasswordStoreBackend>();
  fake_account_backend->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));
  scoped_refptr<password_manager::PasswordStore> account_store =
      base::MakeRefCounted<password_manager::PasswordStore>(
          std::move(fake_account_backend));
  account_store->Init();
  ON_CALL(*GetClient(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store.get()));

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_FALSE(is_password_saved());
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

// Tests that the save password message is reprompted when the trusted vault
// is still locked after the key retrieval flow completes.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       RepromptSavePasswordWhenTrustedVaultStillLocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);
  base::HistogramTester histogram_tester;

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::OnceClosure recovery_callback;
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .WillOnce([&recovery_callback](
                    content::WebContents*,
                    trusted_vault::TrustedVaultUserActionTriggerForUMA,
                    base::OnceClosure callback) {
        recovery_callback = std::move(callback);
      });
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Simulate completion of the key retrieval flow while the vault is still
  // locked. The message should be reprompted with urgent priority because the
  // error is still present.
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(_, _, _, messages::MessagePriority::kUrgent))
      .WillOnce(Return(true));
  std::move(recovery_callback).Run();

  messages::MessageWrapper* reprompt_message = GetMessageWrapper();
  ASSERT_NE(nullptr, reprompt_message);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            reprompt_message->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_BUBBLES_SUBTITLE_TRUSTED_VAULT_ERROR),
            reprompt_message->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CONTINUE),
            reprompt_message->GetPrimaryButtonText());

  // Clicking the button on the reprompted message should start the flow again.
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Both the initial prompt and the repeated prompt were accepted by clicking
  // the primary action button.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.TrustedVaultError",
      password_manager::metrics_util::CLICKED_ACCEPT, 2);
}

// Tests that the dismissal reason is recorded to the trusted vault error metric
// when the user dismisses the prompt while saving is blocked by a trusted vault
// error.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DontSaveOnDismissWithTrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);
  std::unique_ptr<password_manager::FakePasswordStoreBackend>
      fake_account_backend =
          std::make_unique<password_manager::FakePasswordStoreBackend>();
  fake_account_backend->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));
  scoped_refptr<password_manager::PasswordStore> account_store =
      base::MakeRefCounted<password_manager::PasswordStore>(
          std::move(fake_account_backend));
  account_store->Init();
  ON_CALL(*GetClient(), GetAccountPasswordStore())
      .WillByDefault(Return(account_store.get()));

  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(), StartTrustedVaultKeyRetrievalFlow).Times(0);
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.TrustedVaultError",
      password_manager::metrics_util::CLICKED_CANCEL, 1);
}

// Tests that dismissing the reprompted message cleans up the delegate state.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       RepromptSavePasswordDismissalClearsState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::OnceClosure recovery_callback;
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .WillOnce([&recovery_callback](
                    content::WebContents*,
                    trusted_vault::TrustedVaultUserActionTriggerForUMA,
                    base::OnceClosure callback) {
        recovery_callback = std::move(callback);
      });
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Complete recovery flow while vault remains locked to reprompt the message.
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(_, _, _, messages::MessagePriority::kUrgent))
      .WillOnce(Return(true));
  std::move(recovery_callback).Run();

  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::GESTURE);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_TRUE(IsDelegateStateCleared());
}

// Tests that the password is saved and the message dismissed when the error
// state is resolved while the reshown message is still on screen.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePasswordWhenErrorStateResolvesWhileReshownMessageOnScreen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::OnceClosure recovery_callback;
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .WillOnce([&recovery_callback](
                    content::WebContents*,
                    trusted_vault::TrustedVaultUserActionTriggerForUMA,
                    base::OnceClosure callback) {
        recovery_callback = std::move(callback);
      });
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Complete recovery flow while vault remains locked to reprompt the message.
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(_, _, _, messages::MessagePriority::kUrgent))
      .WillOnce(Return(true));
  std::move(recovery_callback).Run();

  // The message was reprompted because the vault was still locked.
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_FALSE(is_password_saved());

  // Simulate that the error is resolved while the reprompted message is on
  // screen.
  account_store_->ReturnErrorOnRequest(std::nullopt);
  base::RunLoop run_loop;
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([&run_loop, this]() {
    RecordPasswordSaved();
    run_loop.Quit();
  });
  // Dismissal of the reprompted message.
  ExpectDismissMessageCall();
  // Enqueuing the confirmation message.
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage)
      .WillOnce(Return(true));
  account_store_->NotifyAboutError();
  run_loop.Run();

  EXPECT_TRUE(is_password_saved());
  EXPECT_EQ(nullptr, GetMessageWrapper());

  ExpectConfirmationMessageDismissCall();
  delegate()->DismissAllActiveUI();
  EXPECT_TRUE(IsDelegateStateCleared());
}

// Tests that the password is saved and confirmation message shown if the
// trusted vault was unlocked when the recovery callback completes, even if the
// store observer hasn't notified yet.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePasswordWhenVaultUnlockedOnRecoveryDone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);
  base::HistogramTester histogram_tester;

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::OnceClosure recovery_callback;
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .WillOnce([&recovery_callback](
                    content::WebContents*,
                    trusted_vault::TrustedVaultUserActionTriggerForUMA,
                    base::OnceClosure callback) {
        recovery_callback = std::move(callback);
      });
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Simulate that the trusted vault is unlocked when recovery callback
  // completes, without triggering a store observer notification.
  account_store_->ReturnErrorOnRequest(std::nullopt);
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([this]() {
    RecordPasswordSaved();
  });
  messages::MessageWrapper* confirmation_message = nullptr;
  EXPECT_CALL(*message_dispatcher_bridge(),
              EnqueueMessage(_, _, _, messages::MessagePriority::kNormal))
      .WillOnce([&confirmation_message](
                    messages::MessageWrapper* message, content::WebContents*,
                    messages::MessageScopeType, messages::MessagePriority) {
        confirmation_message = message;
        return true;
      });
  std::move(recovery_callback).Run();

  EXPECT_TRUE(is_password_saved());
  EXPECT_EQ(nullptr, GetMessageWrapper());
  ASSERT_NE(nullptr, confirmation_message);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE),
            confirmation_message->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_SAVED_CONFIRMATION_MESSAGE_DESCRIPTION),
            confirmation_message->GetDescription());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kSavedSuccessfully,
      1);

  ExpectConfirmationMessageDismissCall();
  delegate()->DismissAllActiveUI();
  EXPECT_TRUE(IsDelegateStateCleared());
}

// Tests that the password is saved after trusted vault key is retrieved.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePasswordAfterTrustedVaultKeyRetrieval) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_FALSE(is_password_saved());

  // Simulate that the trusted vault key was successfully retrieved.
  base::HistogramTester histogram_tester;
  account_store_->ReturnErrorOnRequest(std::nullopt);
  base::RunLoop run_loop;
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([&run_loop, this]() {
    RecordPasswordSaved();
    run_loop.Quit();
  });
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage)
      .WillOnce(Return(true));
  account_store_->NotifyAboutError();
  run_loop.Run();

  ExpectConfirmationMessageDismissCall();
  delegate()->DismissAllActiveUI();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kSavedSuccessfully,
      1);
}

// Tests that when updating a password, even if a trusted vault error is
// present, the password update proceeds directly without triggering trusted
// vault unlock.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UpdatePasswordWithTrustedVaultErrorSavesDirectly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(), StartTrustedVaultKeyRetrievalFlow).Times(0);
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([this]() {
    RecordPasswordSaved();
  });
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_TRUE(is_password_saved());
  DismissAllActiveUI();
}

// Tests that the password is not saved if the delegate is cleared (e.g., due
// to navigation or tab destruction in Chrome) after the user initiated the
// trusted vault unlock activity, but before it completed.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NoSaveIfDelegateClearedWhileWaitingForVaultUnlock) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_FALSE(is_password_saved());

  base::HistogramTester histogram_tester;
  // Simulate lifecycle cleanup (e.g. user navigating away) while the vault
  // unlock is in progress.
  DismissAllActiveUI();

  histogram_tester.ExpectTotalCount(
      "PasswordManager.SaveWithTrustedVaultError.Outcome", 0);

  // Simulate that the trusted vault key was resolved *after* dismissal.
  account_store_->ReturnErrorOnRequest(std::nullopt);
  account_store_->NotifyAboutError();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Password should NOT be saved.
  EXPECT_FALSE(is_password_saved());
}

// Tests that the message timeout with trusted vault error logs
// kMessageTimedOut.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessageTimedOutWithTrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::HistogramTester histogram_tester;
  DismissMessage(messages::DismissReason::TIMER);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kMessageTimedOut,
      1);
  EXPECT_FALSE(is_password_saved());
}

// Tests that dismissing the message with trusted vault error via gesture logs
// user dismissed outcome.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UserDismissedPromptWithTrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  base::HistogramTester histogram_tester;
  DismissMessage(messages::DismissReason::GESTURE);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kUserDismissedPrompt,
      1);
  EXPECT_FALSE(is_password_saved());
}

// Tests that clicking "never save" on the message with trusted vault error logs
// never for this site outcome.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NeverSaveWithTrustedVaultErrorLogsNeverForThisSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());

  EXPECT_CALL(*raw_form_manager, Blocklist);
  base::HistogramTester histogram_tester;
  TriggerNeverSaveMenuItem();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kNeverForThisSite,
      1);
  EXPECT_FALSE(is_password_saved());
}

// Tests that cancelling the password edit dialog with trusted vault error logs
// user dismissed outcome.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DialogCancelledWithTrustedVaultErrorLogsUserDismissed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  base::HistogramTester histogram_tester;
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kUserDismissedPrompt,
      1);
  EXPECT_FALSE(is_password_saved());
}

// Tests that the password is not saved when a non-resolution error is
// encountered.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       NonResolutionErrorDoesNotSavePassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_FALSE(is_password_saved());

  base::HistogramTester histogram_tester;
  // Simulate a different error occurring instead of resolution.
  account_store_->SetError(password_manager::ActionableError::kSignInNeeded);
  account_store_->NotifyAboutError();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Password should NOT be saved.
  EXPECT_FALSE(is_password_saved());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kNewStoreError,
      1);
}

// Tests that canceling the device lock UI when trusted vault unlock is needed
// logs device lock canceled outcome.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DeviceLockCanceledWithTrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());

  base::HistogramTester histogram_tester;
  test_device_lock_bridge()->SimulateDeviceLockComplete(false);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kDeviceLockCanceled,
      1);
  EXPECT_FALSE(is_password_saved());
}

// Tests that the password is saved after the trusted vault key is retrieved
// when the user accepts the password edit dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DialogSaveAfterTrustedVaultKeyRetrieval) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_CALL(*raw_form_manager, Save()).Times(0);
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));

  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  EXPECT_FALSE(is_password_saved());

  // Simulate that the trusted vault key was successfully retrieved.
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([&run_loop, this]() {
    RecordPasswordSaved();
    run_loop.Quit();
  });
  account_store_->ReturnErrorOnRequest(std::nullopt);
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage)
      .WillOnce(Return(true));
  account_store_->NotifyAboutError();
  run_loop.Run();

  ExpectConfirmationMessageDismissCall();
  delegate()->DismissAllActiveUI();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveWithTrustedVaultError.Outcome",
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kSavedSuccessfully,
      1);
}

// Tests that when updating a password from dialog, even if a trusted vault
// error is present, the password update proceeds directly without triggering
// trusted vault unlock.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DialogUpdateWithTrustedVaultErrorSavesDirectly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/true);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  EXPECT_CALL(*helper_bridge(), StartTrustedVaultKeyRetrievalFlow).Times(0);
  EXPECT_CALL(*raw_form_manager, Save()).WillOnce([this]() {
    RecordPasswordSaved();
  });

  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  EXPECT_TRUE(is_password_saved());
  DismissAllActiveUI();
}

// Tests that password form is not saved and metrics recorded correctly when the
// message is autodismissed.
TEST_F(SaveUpdatePasswordMessageDelegateTest, MetricOnAutodismissTimer) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(test_ukm_recorder,
                   PasswordFormMetricsRecorder::BubbleDismissalReason::kIgnored,
                   /*update_password=*/false);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::NO_DIRECT_INTERACTION, 1);
}

// Tests that update password message with a single PasswordForm immediately
// saves the form on Update button tap and doesn't display confirmation dialog.
TEST_F(SaveUpdatePasswordMessageDelegateTest, UpdatePasswordWithSingleForm) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  SetPendingCredentials(kUsername, kPassword);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
      /*update_password=*/true);
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), two_forms_best_matches());
  EXPECT_CALL(*form_manager, Save());
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  ExpectDismissMessageCall();
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_EQ(nullptr, GetMessageWrapper());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
      /*update_password=*/true);
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);

  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*raw_form_manager, Save());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);
  // The real password edit dialog triggers dialog dismissed delegate inside.
  // Here we use the mock that doesn't do this, so the dismiss is called
  // manually here.
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
      /*update_password=*/false);
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*raw_form_manager, Blocklist());
  TriggerNeverSaveMenuItem();

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined,
      /*update_password=*/false);
  histogram_tester.ExpectUniqueSample(
      kSaveUIDismissalReasonHistogramName,
      password_manager::metrics_util::CLICKED_NEVER, 1);
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/true);

  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*raw_form_manager, Save());
  TriggerDialogAcceptedCallback(/*username=*/kUsername,
                                /*password=*/kPassword);

  // The real password edit dialog triggers dialog dismissed delegate inside.
  // Here we use the mock that doesn't do this, so the dismiss is called
  // manually here.
  TriggerDialogDismissedCallback(/*dialog_accepted=*/true);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kAccepted,
      /*update_password=*/true);
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

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  MockPasswordFormManagerForUI* raw_form_manager = form_manager.get();
  MockPasswordEditDialog* mock_dialog = PreparePasswordEditDialog();
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*mock_dialog, ShowPasswordEditDialog);
  TriggerPasswordEditDialog(/*update_password=*/false);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  EXPECT_CALL(*raw_form_manager, Save()).Times(0);
  TriggerDialogDismissedCallback(/*dialog_accepted=*/false);

  CommitPasswordFormMetrics();
  VerifyUkmMetrics(
      test_ukm_recorder,
      PasswordFormMetricsRecorder::BubbleDismissalReason::kDeclined,
      /*update_password=*/false);
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

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown but password is not saved yet.
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_EQ(false, is_password_saved());

  // Verify that password is saved after receiving the callback that device lock
  // was set.
  test_device_lock_bridge()->SimulateDeviceLockComplete(true);
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
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown but password is not saved yet.
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_EQ(false, is_password_saved());

  // Verify that password is updated after receiving the callback that device
  // lock was set.
  test_device_lock_bridge()->SimulateDeviceLockComplete(true);
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

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  // Verify that device lock UI is shown.
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());

  // Verify that password is not saved after device lock was not set.
  test_device_lock_bridge()->SimulateDeviceLockComplete(false);
}

// Tests that password is not saved if device lock UI needs to be shown but is
// not.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockUiNotShown) {
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  // Launch save password UI and click the save button.
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick(messages::DismissReason::UNKNOWN);
}

// Tests that the password is not saved and trusted vault key retrieval flow
// starts when trusted vault key is needed, but device lock is not, during save
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_TrustedVaultKeyNeeded_DeviceLockNotNeeded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(false);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  web_contents(),
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));

  TriggerActionClick();

  EXPECT_FALSE(is_password_saved());
}

// Tests that the password is saved directly and trusted vault key retrieval
// flow is not started when trusted vault key is needed during update password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UpdatePassword_TrustedVaultKeyNeeded_DeviceLockNotNeeded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);
  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(false);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).WillOnce([this]() {
    RecordPasswordSaved();
  });
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(*helper_bridge(), StartTrustedVaultKeyRetrievalFlow).Times(0);

  TriggerActionClick();

  EXPECT_TRUE(is_password_saved());
}

// Tests helper behaviour when device lock is set successfully, but a trusted
// vault key is also needed during save password: device lock UI is shown and
// trusted vault key retrieval flow is started.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockAndTrustedVaultKeyNeeded_DeviceLockSet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  // TODO(crbug.com/483652585): Call the callback from the test bridge
  // asynchronously so the behaviour matches the state in production.
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_FALSE(is_password_saved());
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  web_contents(),
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));

  test_device_lock_bridge()->SimulateDeviceLockComplete(true);

  EXPECT_FALSE(is_password_saved());
}

// Tests helper behaviour when device lock is set successfully, but a trusted
// vault key error is also present during update password: device lock UI is
// shown, trusted vault key retrieval flow is not started, and password is
// saved.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UpdatePassword_DeviceLockAndTrustedVaultKeyNeeded_DeviceLockSet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).WillOnce([this]() {
    RecordPasswordSaved();
  });
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_FALSE(is_password_saved());
  EXPECT_CALL(*helper_bridge(), StartTrustedVaultKeyRetrievalFlow).Times(0);

  test_device_lock_bridge()->SimulateDeviceLockComplete(true);

  EXPECT_TRUE(is_password_saved());
}

// Tests helper behaviour when device lock is not set, and a trusted vault
// key is also needed during save password: device lock UI is shown and trusted
// vault key retrieval flow is not started because the device lock is not set
// up.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SavePassword_DeviceLockAndTrustedVaultKeyNeeded_DeviceLockNotSet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  web_contents(),
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .Times(0);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_FALSE(is_password_saved());

  test_device_lock_bridge()->SimulateDeviceLockComplete(false);

  EXPECT_FALSE(is_password_saved());
}

// Tests helper behaviour when device lock is not set, and a trusted vault
// key is also needed during update password: device lock UI is shown and
// trusted vault key retrieval flow is not started because it's a password
// update flow. Saving does not occur because device lock setup was not
// completed.

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       UpdatePassword_DeviceLockAndTrustedVaultKeyNeeded_DeviceLockNotSet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);
  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  web_contents(),
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _))
      .Times(0);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();

  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_FALSE(is_password_saved());

  test_device_lock_bridge()->SimulateDeviceLockComplete(false);

  EXPECT_FALSE(is_password_saved());
}

// Tests parameterized with different feature states

// Tests that message properties (title, description, icon, button text) are
// set correctly for save password message.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_SavePassword) {
  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_SavePassword_TrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  EnqueueMessage(CreateFormManager(GURL(kDefaultUrl), empty_best_matches()),
                 /*user_signed_in=*/true, /*update_password=*/false);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_BUBBLES_SUBTITLE_TRUSTED_VAULT_ERROR),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CONTINUE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       MessagePropertyValues_UpdatePassword_TrustedVaultError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/false);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, IsPasswordUpdate).WillRepeatedly(Return(true));
  EXPECT_CALL(*form_manager, IsUpdateAffectingPasswordsStoredInTheGoogleAccount)
      .WillRepeatedly(Return(false));
  const bool is_signed_in = true;
  const bool is_update = true;
  EnqueueMessage(std::move(form_manager), is_signed_in, is_update);

  // For update, the title should remain "Update password" even when trusted
  // vault key is needed.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_UPDATE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  // Even though the user is signed in, the update is local (not affecting
  // account store), so we expect the signed-out (device-only) description.
  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, /*is_signed_in=*/false,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that the description is set correctly when signed-in user saves a
// password.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SignedInDescription_SavePassword) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = false;

  AccountCapabilities account_capabilities;
  AccountCapabilitiesTestMutator mutator(&account_capabilities);
  mutator.set_can_have_email_address_displayed(false);
  std::optional<AccountInfo> account_info =
      AccountInfo::Builder(GaiaId("test_gaia"), kAccountEmail)
          .SetFullName(kAccountFullName)
          .UpdateAccountCapabilitiesWith(account_capabilities)
          .Build();

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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword, true)};
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/false);
  std::vector<PasswordForm> single_form_best_matches = {
      CreatePasswordForm(kUsername, kPassword, false)};
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
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
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  const bool is_signed_in = true;
  const bool is_update = true;

  AccountCapabilities account_capabilities;
  AccountCapabilitiesTestMutator mutator(&account_capabilities);
  mutator.set_can_have_email_address_displayed(false);
  std::optional<AccountInfo> account_info =
      AccountInfo::Builder(GaiaId("test_gaia"), kAccountEmail)
          .SetFullName(kAccountFullName)
          .UpdateAccountCapabilitiesWith(account_capabilities)
          .Build();

  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update, account_info);
  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountFullName16),
            GetMessageWrapper()->GetDescription());
  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests `IsUsingAccountStorage` returns false if the credential being
// updated comes from the local storage, despite the user being signed in,
// if the credential comes from the profile store.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       LocalCredentialNotUsingAccountStorage) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/false);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  PasswordEditDialogBridgeDelegate* edit_dialog_delegate =
      get_password_edit_dialog_bridge_delegate();

  EXPECT_FALSE(edit_dialog_delegate->IsUsingAccountStorage(kUsername));

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests `IsUsingAccountStorage` returns true if the crential comes from
// the account store.
TEST_F(SaveUpdatePasswordMessageDelegateTest, CredentialUsingAccountStorage) {
  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  PasswordEditDialogBridgeDelegate* edit_dialog_delegate =
      get_password_edit_dialog_bridge_delegate();

  EXPECT_TRUE(edit_dialog_delegate->IsUsingAccountStorage(kUsername));

  DismissMessage(messages::DismissReason::UNKNOWN);
}

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       CredentialUpdatedWithBackupPassword) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  const std::u16string backup_password = u"test_backup1";
  SetPendingCredentials(kUsername, backup_password);
  std::vector<PasswordForm> best_matches = {
      CreatePasswordForm(kUsername, kPassword)};
  best_matches[0].SetPasswordBackupNote(backup_password);
  best_matches[0].type =
      password_manager::PasswordForm::Type::kChangeSubmission;
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), best_matches);
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/true);
  EXPECT_NE(nullptr, GetMessageWrapper());
  TriggerActionClick();
  EXPECT_EQ(nullptr, GetMessageWrapper());

  CommitPasswordFormMetrics();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeRecoveryFlow",
      password_manager::metrics_util::PasswordChangeRecoveryFlowState::
          kPrimaryPasswordUpdated,
      1);
  const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
      entries = test_ukm_recorder.GetEntriesByName(
          ukm::builders::PasswordManager_ChangeRecovery::kEntryName);
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::PasswordManager_ChangeRecovery::
          kPasswordChangeRecoveryFlowName,
      static_cast<int>(
          password_manager::metrics_util::PasswordChangeRecoveryFlowState::
              kPrimaryPasswordUpdated));
}

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TrustedVaultError_FederatedCredential_ShowsSaveAccount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true,
                        url::SchemeHostPort(GURL("https://google.com")));

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/true,
                 /*update_password=*/false);

  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_ACCOUNT),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_BUBBLES_SUBTITLE_TRUSTED_VAULT_ERROR),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CONTINUE),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

TEST_F(SaveUpdatePasswordMessageDelegateTest,
       TrustedVaultError_FeatureDisabled_FallbackToStandardUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->SetError(
      password_manager::ActionableError::kTrustedVaultKeyNeeded);

  SetPendingCredentials(kUsername, kPassword, /*is_account_store=*/true);
  const bool is_signed_in = true;
  const bool is_update = false;
  EnqueueMessage(CreateFormManager(GURL(kDefaultUrl), empty_best_matches()),
                 /*user_signed_in=*/is_signed_in,
                 /*update_password=*/is_update);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            GetMessageWrapper()->GetTitle());

  EXPECT_EQ(GetExpectedUPMMessageDescription(is_update, is_signed_in,
                                             kAccountEmail16),
            GetMessageWrapper()->GetDescription());

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON),
            GetMessageWrapper()->GetPrimaryButtonText());

  DismissMessage(messages::DismissReason::UNKNOWN);
}
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       ConfirmationMessageShownAfterSave) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordSaveInContextErrorResolution);

  account_store_->ReturnErrorOnRequest(
      password_manager::PasswordStoreBackendError(
          password_manager::PasswordStoreBackendErrorType::
              kKeyRetrievalRequired));

  SetPendingCredentials(kUsername, kPassword);
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());

  // 1. Expect the first message (Save Password Prompt)
  messages::MessageWrapper* prompt_message = nullptr;
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage)
      .WillOnce([&prompt_message](messages::MessageWrapper* message,
                                  content::WebContents* web_contents,
                                  messages::MessageScopeType scope_type,
                                  messages::MessagePriority priority) {
        prompt_message = message;
        return true;
      });

  // Display the prompt
  DisplaySaveUpdatePasswordPromptInternal(std::move(form_manager),
                                          /*account_info=*/std::nullopt,
                                          /*update_password=*/false);

  ASSERT_NE(nullptr, prompt_message);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD),
            prompt_message->GetTitle());

  // Trigger Save action
  EXPECT_CALL(*helper_bridge(),
              StartTrustedVaultKeyRetrievalFlow(
                  _,
                  trusted_vault::TrustedVaultUserActionTriggerForUMA::
                      kPasswordSavePrompt,
                  _));
  TriggerActionClick();

  // The primary message should be dismissed, and no confirmation message
  // enqueued yet.
  EXPECT_EQ(nullptr, GetMessageWrapper());

  // Simulate that the trusted vault key was successfully retrieved.
  account_store_->ReturnErrorOnRequest(std::nullopt);

  // 2. Expect the confirmation message
  messages::MessageWrapper* confirmation_message = nullptr;
  base::RunLoop run_loop;
  EXPECT_CALL(*message_dispatcher_bridge(), EnqueueMessage)
      .WillOnce([&confirmation_message, &run_loop](
                    messages::MessageWrapper* message,
                    content::WebContents* web_contents,
                    messages::MessageScopeType scope_type,
                    messages::MessagePriority priority) {
        confirmation_message = message;
        run_loop.Quit();
        return true;
      });

  account_store_->NotifyAboutError();
  run_loop.Run();

  // The confirmation message should now be enqueued!
  ASSERT_NE(nullptr, confirmation_message);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE),
            confirmation_message->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_SAVED_CONFIRMATION_MESSAGE_DESCRIPTION),
            confirmation_message->GetDescription());

  // Verify dismissing the confirmation message resets it.
  EXPECT_CALL(*message_dispatcher_bridge(),
              DismissMessage(confirmation_message, _))
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  delegate()->DismissAllActiveUI();
}

// Tests that when Device Lock requirement is met while the prompt message
// dismissal callback is still in flight in Java, the password is saved,
// no premature CHECK crashes occur in ClearState, and the subsequent dismissal
// callback from Java completes safely.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DeviceLockCompletedWhilePromptDismissalInFlight) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save());
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  ASSERT_NE(nullptr, GetMessageWrapper());

  // User clicks "Save", which launches Device Lock UI. Keep Java message
  // dismissal in-flight (i.e. message_ is still non-null).
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());
  EXPECT_FALSE(is_password_saved());

  // Device Lock UI finishes before Java message dismissal callback arrives.
  test_device_lock_bridge()->SimulateDeviceLockComplete(true);
  EXPECT_TRUE(is_password_saved());

  // Now simulate Java completing the delayed message dismissal callback.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that when Device Lock requirement is not met (e.g. canceled by user)
// while the prompt message dismissal callback is still in flight in Java,
// the delegate cleans up safely without premature CHECK crashes in ClearState.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DeviceLockFailedWhilePromptDismissalInFlight) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  ASSERT_NE(nullptr, GetMessageWrapper());

  // User clicks "Save", which launches Device Lock UI with in-flight dismissal.
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());

  // Device Lock is canceled by user while message dismissal is still in flight.
  test_device_lock_bridge()->SimulateDeviceLockComplete(false);
  EXPECT_FALSE(is_password_saved());

  // Delayed message dismissal arrives from Java.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that enqueuing a second prompt while the first prompt's dismissal
// callback is still pending in Java does not trigger synchronous CHECK crashes
// and cleanly displays the second prompt.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       SuccessivePromptEnqueuedWhilePreviousPromptDismissing) {
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager1 =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager1), /*user_signed_in=*/true,
                 /*update_password=*/false);
  ASSERT_NE(nullptr, GetMessageWrapper());

  // Enqueue a second prompt. This triggers DismissAllActiveUI() for prompt #1.
  std::unique_ptr<MockPasswordFormManagerForUI> form_manager2 =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EnqueueMessage(std::move(form_manager2), /*user_signed_in=*/true,
                 /*update_password=*/false);
  ASSERT_NE(nullptr, GetMessageWrapper());

  DismissMessage(messages::DismissReason::UNKNOWN);
}

// Tests that calling DismissAllActiveUI while Device Lock UI is pending and
// message dismissal callback is in flight in Java safely cancels/aborts the
// flow, preventing saving or crashes when Device Lock completes afterwards.
TEST_F(SaveUpdatePasswordMessageDelegateTest,
       DismissAllActiveUIWhileDeviceLockPending) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  test_device_lock_bridge()->SetShouldShowDeviceLockUi(true);

  std::unique_ptr<MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(GURL(kDefaultUrl), empty_best_matches());
  EXPECT_CALL(*form_manager, Save()).Times(0);
  EnqueueMessage(std::move(form_manager), /*user_signed_in=*/false,
                 /*update_password=*/false);
  ASSERT_NE(nullptr, GetMessageWrapper());

  // User clicks "Save", which launches Device Lock UI. Keep Java message
  // dismissal in-flight (i.e. message_ is still non-null).
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_EQ(1, test_device_lock_bridge()->device_lock_ui_shown_count());

  // User closes tab / navigates while device lock is displayed on screen.
  ExpectDismissMessageCall();
  delegate()->DismissAllActiveUI();

  // Device Lock UI finishes after tab dismissal was already initiated.
  test_device_lock_bridge()->SimulateDeviceLockComplete(true);
  EXPECT_FALSE(is_password_saved());
}
