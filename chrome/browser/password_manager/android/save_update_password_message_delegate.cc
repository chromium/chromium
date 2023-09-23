// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "save_update_password_message_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using password_manager::features::kPasswordEditDialogWithDetails;

namespace {

// Duration of message before timeout; 20 seconds.
const int kMessageDismissDurationMs = 20000;

// Log the outcome of the save/update password workflow.
// It differentiates whether the the flow was accepted/cancelled immediately
// or after calling the password edit dialog.
void LogSaveUpdatePasswordMessageDismissalReason(
    SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
        reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SaveUpdateUIDismissalReasonAndroid", reason);
}

// Log the outcome of the save password workflow.
// It differentiates whether the password was saved/canceled immediately or
// after calling the password edit dialog.
void LogSavePasswordMessageDismissalReason(
    SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
        reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SaveUpdateUIDismissalReasonAndroid.Save", reason);
}

// Log the outcome of the update password workflow.
// It differentiates whether the password was updated/canceled immediately or
// after calling the password edit dialog.
void LogUpdatePasswordMessageDismissalReason(
    SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
        reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SaveUpdateUIDismissalReasonAndroid.Update", reason);
}

// Log the outcome of the update password workflow with multiple credentials
// saved for current site.
// Canceled in message | confirmed | canceled in dialog
void LogConfirmUsernameMessageDismissalReason(
    SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
        reason) {
  base::UmaHistogramEnumeration(
      "PasswordManager.SaveUpdateUIDismissalReasonAndroid."
      "UpdateWithUsernameConfirmation",
      reason);
}

void TryToShowPasswordMigrationWarning(
    base::RepeatingCallback<
        void(gfx::NativeWindow,
             Profile*,
             password_manager::metrics_util::PasswordMigrationWarningTriggers)>
        callback,
    raw_ptr<content::WebContents> web_contents) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    callback.Run(
        web_contents->GetTopLevelNativeWindow(),
        Profile::FromBrowserContext(web_contents->GetBrowserContext()),
        password_manager::metrics_util::PasswordMigrationWarningTriggers::
            kPasswordSaveUpdateMessage);
  }
}

}  // namespace

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate()
    : SaveUpdatePasswordMessageDelegate(
          base::BindRepeating(PasswordEditDialogBridge::Create),
          base::BindRepeating(&local_password_migration::ShowWarning)) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    PasswordEditDialogFactory password_edit_dialog_factory,
    base::RepeatingCallback<
        void(gfx::NativeWindow,
             Profile*,
             password_manager::metrics_util::PasswordMigrationWarningTriggers)>
        create_migration_warning_callback)
    : password_edit_dialog_factory_(std::move(password_edit_dialog_factory)),
      create_migration_warning_callback_(
          std::move(create_migration_warning_callback)),
      device_lock_bridge_(std::make_unique<DeviceLockBridge>()) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    base::PassKey<class SaveUpdatePasswordMessageDelegateTest>,
    PasswordEditDialogFactory password_edit_dialog_factory,
    base::RepeatingCallback<
        void(gfx::NativeWindow,
             Profile*,
             password_manager::metrics_util::PasswordMigrationWarningTriggers)>
        create_migration_warning_callback,
    std::unique_ptr<DeviceLockBridge> device_lock_bridge)
    : SaveUpdatePasswordMessageDelegate(password_edit_dialog_factory,
                                        create_migration_warning_callback) {
  device_lock_bridge_ = std::move(device_lock_bridge);
}

SaveUpdatePasswordMessageDelegate::~SaveUpdatePasswordMessageDelegate() {
  DCHECK(web_contents_ == nullptr);
}

void SaveUpdatePasswordMessageDelegate::DisplaySaveUpdatePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password,
    password_manager::PasswordManagerClient* password_manager_client) {
  DCHECK_NE(nullptr, web_contents);
  DCHECK(form_to_save);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  absl::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(profile);
  DisplaySaveUpdatePasswordPromptInternal(
      web_contents, std::move(form_to_save), std::move(account_info),
      update_password, password_manager_client);
}

void SaveUpdatePasswordMessageDelegate::DismissSaveUpdatePasswordPrompt() {
  if (password_edit_dialog_ != nullptr) {
    password_edit_dialog_->Dismiss();
  }
  DismissSaveUpdatePasswordMessage(messages::DismissReason::UNKNOWN);
}

void SaveUpdatePasswordMessageDelegate::DismissSaveUpdatePasswordMessage(
    messages::DismissReason dismiss_reason) {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

void SaveUpdatePasswordMessageDelegate::DisplaySaveUpdatePasswordPromptInternal(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    absl::optional<AccountInfo> account_info,
    bool update_password,
    password_manager::PasswordManagerClient* password_manager_client) {
  // Dismiss previous message if it is displayed.
  DismissSaveUpdatePasswordPrompt();
  DCHECK(message_ == nullptr);
  DCHECK(password_edit_dialog_ == nullptr);

  web_contents_ = web_contents;
  passwords_state_.set_client(password_manager_client);
  if (update_password) {
    passwords_state_.OnUpdatePassword(std::move(form_to_save));
  } else {
    passwords_state_.OnPendingPassword(std::move(form_to_save));
  }

  if (account_info.has_value()) {
    account_email_ = account_info->CanHaveEmailAddressDisplayed()
                         ? account_info.value().email
                         : account_info.value().full_name;
  } else {
    account_email_ = std::string();
  }

  CreateMessage(update_password);
  RecordMessageShownMetrics();
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
}

void SaveUpdatePasswordMessageDelegate::CreateMessage(bool update_password) {
  // Binding with base::Unretained(this) is safe here because
  // SaveUpdatePasswordMessageDelegate owns message_. Callbacks won't be called
  // after the current object is destroyed.
  messages::MessageIdentifier message_id =
      update_password ? messages::MessageIdentifier::UPDATE_PASSWORD
                      : messages::MessageIdentifier::SAVE_PASSWORD;
  base::OnceClosure callback =
      update_password
          ? base::BindOnce(
                &SaveUpdatePasswordMessageDelegate::HandleUpdateButtonClicked,
                base::Unretained(this))
          : base::BindOnce(
                &SaveUpdatePasswordMessageDelegate::HandleSaveButtonClicked,
                base::Unretained(this));
  message_ = std::make_unique<messages::MessageWrapper>(
      message_id, std::move(callback),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::HandleMessageDismissed,
                     base::Unretained(this)));

  message_->SetDuration(kMessageDismissDurationMs);

  const password_manager::PasswordForm& pending_credentials =
      passwords_state_.form_manager()->GetPendingCredentials();

  int title_message_id;
  if (update_password) {
    title_message_id = IDS_UPDATE_PASSWORD;
  } else if (pending_credentials.federation_origin.opaque()) {
    title_message_id = IDS_SAVE_PASSWORD;
  } else {
    title_message_id = IDS_SAVE_ACCOUNT;
  }
  message_->SetTitle(l10n_util::GetStringUTF16(title_message_id));

  std::u16string description =
      GetMessageDescription(pending_credentials, update_password);
  message_->SetDescription(description);

  update_password_ = update_password;

  bool use_followup_button = HasMultipleCredentialsStored();
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      GetPrimaryButtonTextId(update_password, use_followup_button)));

    message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
        IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP));
    message_->DisableIconTint();

  // With kPasswordEditDialogWithDetails feature on: the cog button is always
  // shown for the save message and for the update message when there is
  // just one password stored for the web site. When there are multiple
  // credentials stored, the dialog will be called anyway from the followup
  // button, so there are no options to put under the cog.
  // With kPasswordEditDialogWithDetails feature off: the cog button is
  // shown only for the Save password message.
  if (base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails) &&
      (!update_password || !use_followup_button)) {
    SetupCogMenuForDialogWithDetails(message_, update_password);
  } else if (!base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails) &&
             !update_password) {
    SetupCogMenu(message_, update_password);
  }
}

void SaveUpdatePasswordMessageDelegate::SetupCogMenu(
    std::unique_ptr<messages::MessageWrapper>& message,
    bool update_password) {
  message->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MESSAGE_NEVER_SAVE_MENU_ITEM));

  message->SetSecondaryActionCallback(base::BindRepeating(
      &SaveUpdatePasswordMessageDelegate::HandleNeverSaveClicked,
      base::Unretained(this)));
}

void SaveUpdatePasswordMessageDelegate::SetupCogMenuForDialogWithDetails(
    std::unique_ptr<messages::MessageWrapper>& message,
    bool update_password) {
  message->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  if (update_password) {
    message->SetSecondaryActionCallback(base::BindRepeating(
        &SaveUpdatePasswordMessageDelegate::DisplayEditDialog,
        base::Unretained(this), update_password));
  } else {
    message_->SetSecondaryMenuItemSelectedCallback(base::BindRepeating(
        &SaveUpdatePasswordMessageDelegate::HandleSaveMessageMenuItemClick,
        base::Unretained(this)));
    message_->AddSecondaryMenuItem(
        static_cast<int>(SavePasswordDialogMenuItem::kNeverSave),
        /*resource_id=*/0,
        l10n_util::GetStringUTF16(IDS_PASSWORD_MESSAGE_NEVER_SAVE_MENU_ITEM),
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MESSAGE_NEVER_SAVE_MENU_ITEM_DESC));
    message_->AddSecondaryMenuItem(
        static_cast<int>(SavePasswordDialogMenuItem::kEditPassword),
        /*resource_id=*/0,
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_MESSAGE_EDIT_PASSWORD_MENU_ITEM));
  }
}

void SaveUpdatePasswordMessageDelegate::HandleSaveMessageMenuItemClick(
    int item_id) {
  switch (static_cast<SavePasswordDialogMenuItem>(item_id)) {
    case SavePasswordDialogMenuItem::kNeverSave:
      HandleNeverSaveClicked();
      break;
    case SavePasswordDialogMenuItem::kEditPassword:
      DisplayEditDialog(/*update_password=*/false);
      break;
  }
}

std::u16string SaveUpdatePasswordMessageDelegate::GetMessageDescription(
    const password_manager::PasswordForm& pending_credentials,
    bool update_password) {
  if (!account_email_.empty()) {
    return l10n_util::GetStringFUTF16(
        update_password
            ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION
            : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION,
        base::UTF8ToUTF16(account_email_));
  }
  return l10n_util::GetStringUTF16(
      update_password
          ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION
          : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION);
}

int SaveUpdatePasswordMessageDelegate::GetPrimaryButtonTextId(
    bool update_password,
    bool use_followup_button_text) {
  if (!update_password)
    return IDS_PASSWORD_MANAGER_SAVE_BUTTON;
  if (!use_followup_button_text)
    return IDS_PASSWORD_MANAGER_UPDATE_BUTTON;
  return IDS_PASSWORD_MANAGER_CONTINUE_BUTTON;
}

unsigned int SaveUpdatePasswordMessageDelegate::GetDisplayUsernames(
    std::vector<std::u16string>* usernames) {
  unsigned int selected_username_index = 0;
  // TODO(crbug.com/1054410): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      password_forms = passwords_state_.GetCurrentForms();
  const std::u16string& default_username =
      passwords_state_.form_manager()->GetPendingCredentials().username_value;
  for (const auto& form : password_forms) {
    const std::u16string username =
        base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails)
            ? form->username_value
            : GetDisplayUsername(*form);
    usernames->push_back(username);
    if (form->username_value == default_username) {
      selected_username_index = usernames->size() - 1;
    }
  }
  return selected_username_index;
}

void SaveUpdatePasswordMessageDelegate::HandleSaveButtonClicked() {
  SavePassword();
}

void SaveUpdatePasswordMessageDelegate::SavePassword() {
  if (!device_lock_bridge_->ShouldShowDeviceLockUi()) {
    passwords_state_.form_manager()->Save();
    return;
  }
  if (auto* window = web_contents_->GetNativeView()->GetWindowAndroid()) {
    device_lock_bridge_->LaunchDeviceLockUiBeforeRunningCallback(
        window,
        base::BindOnce(
            &SaveUpdatePasswordMessageDelegate::SavePasswordAfterDeviceLockUi,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void SaveUpdatePasswordMessageDelegate::SavePasswordAfterDeviceLockUi(
    bool is_device_lock_set) {
  CHECK(device_lock_bridge_->RequiresDeviceLock());
  if (is_device_lock_set) {
    passwords_state_.form_manager()->Save();
    TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                      web_contents_);
  }
  ClearState();
}

void SaveUpdatePasswordMessageDelegate::HandleNeverSaveClicked() {
  passwords_state_.form_manager()->Blocklist();
  DismissSaveUpdatePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegate::HandleUpdateButtonClicked() {
  std::vector<std::u16string> usernames;
  if (HasMultipleCredentialsStored()) {
    DisplayEditDialog(/*update_password=*/true);
  } else {
    SavePassword();
  }
}

void SaveUpdatePasswordMessageDelegate::DisplayEditDialog(
    bool update_password) {
  const std::u16string& current_username =
      passwords_state_.form_manager()->GetPendingCredentials().username_value;
  const std::u16string& current_password =
      passwords_state_.form_manager()->GetPendingCredentials().password_value;

  CreatePasswordEditDialog();

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!password_edit_dialog_)
    return;

  std::vector<std::u16string> usernames;
  int selected_username_index = GetDisplayUsernames(&usernames);
  if (base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails)) {
    password_edit_dialog_->ShowPasswordEditDialog(
        usernames, current_username, current_password, account_email_);
  } else {
    password_edit_dialog_->ShowLegacyPasswordEditDialog(
        usernames, selected_username_index, account_email_);
  }

  DismissSaveUpdatePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  message_.reset();
  if (password_edit_dialog_) {
    // The user triggered password edit dialog. Don't cleanup internal
    // datastructures, dialog dismiss callback will perform cleanup.
    return;
  }
  // Record metrics and cleanup state.
  RecordDismissalReasonMetrics(
      MessageDismissReasonToPasswordManagerUIDismissalReason(dismiss_reason));
  if (base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails)) {
    RecordSaveUpdateUIDismissalReason(
        GetSaveUpdatePasswordMessageDismissReason(dismiss_reason));
  }

  // If Device Lock UI needs to be shown and can be (i.e. WindowAndroid is
  // available), these lines are handled in the SavePasswordAfterDeviceLockUi()
  // callback.
  if (!(device_lock_bridge_->ShouldShowDeviceLockUi() &&
        web_contents_->GetNativeView()->GetWindowAndroid())) {
    if (dismiss_reason == messages::DismissReason::PRIMARY_ACTION) {
      TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                        web_contents_);
    }
    ClearState();
  }
}

bool SaveUpdatePasswordMessageDelegate::HasMultipleCredentialsStored() {
  // TODO(crbug.com/1054410): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      password_forms = passwords_state_.GetCurrentForms();
  return password_forms.size() > 1;
}

void SaveUpdatePasswordMessageDelegate::CreatePasswordEditDialog() {
  // Binding with base::Unretained(this) is safe here because
  // SaveUpdatePasswordMessageDelegate owns password_edit_dialog_. Callbacks
  // won't be called after the SaveUpdatePasswordMessageDelegate object is
  // destroyed.
  password_edit_dialog_ = password_edit_dialog_factory_.Run(
      web_contents_.get(),
      base::BindOnce(
          &SaveUpdatePasswordMessageDelegate::HandleSavePasswordFromDialog,
          base::Unretained(this)),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::
                         HandleSavePasswordFromLegacyDialog,
                     base::Unretained(this)),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::HandleDialogDismissed,
                     base::Unretained(this)));
}

void SaveUpdatePasswordMessageDelegate::HandleDialogDismissed(
    bool dialog_accepted) {
  RecordDismissalReasonMetrics(
      dialog_accepted ? password_manager::metrics_util::CLICKED_ACCEPT
                      : password_manager::metrics_util::CLICKED_CANCEL);
  if (base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails)) {
    RecordSaveUpdateUIDismissalReason(
        GetPasswordEditDialogDismissReason(dialog_accepted));
  }

  password_edit_dialog_.reset();

  // If Device Lock UI needs to be shown and can be (i.e. WindowAndroid is
  // available), these lines are handled in the SavePasswordAfterDeviceLockUi()
  // callback.
  if (!(device_lock_bridge_->ShouldShowDeviceLockUi() &&
        web_contents_->GetNativeView()->GetWindowAndroid())) {
    TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                      web_contents_);
    ClearState();
  }
}

void SaveUpdatePasswordMessageDelegate::HandleSavePasswordFromDialog(
    const std::u16string& username,
    const std::u16string& password) {
  UpdatePasswordFormUsernameAndPassword(username, password,
                                        passwords_state_.form_manager());
  SavePassword();
}

void SaveUpdatePasswordMessageDelegate::HandleSavePasswordFromLegacyDialog(
    int username_index) {
  UpdatePasswordFormUsernameAndPassword(
      passwords_state_.GetCurrentForms()[username_index]->username_value,
      passwords_state_.form_manager()->GetPendingCredentials().password_value,
      passwords_state_.form_manager());
  SavePassword();
}

void SaveUpdatePasswordMessageDelegate::ClearState() {
  DCHECK(message_ == nullptr);
  DCHECK(password_edit_dialog_ == nullptr);

  passwords_state_.OnInactive();
  // web_contents_ is set in DisplaySaveUpdatePasswordPromptInternal().
  // Resetting it here to keep the state clean when no message is enqueued.
  web_contents_ = nullptr;
}

void SaveUpdatePasswordMessageDelegate::RecordMessageShownMetrics() {
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        passwords_state_.form_manager()->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

void SaveUpdatePasswordMessageDelegate::RecordDismissalReasonMetrics(
    password_manager::metrics_util::UIDismissalReason ui_dismissal_reason) {
  auto submission_event =
      passwords_state_.form_manager()->GetPendingCredentials().submission_event;
  if (update_password_) {
    password_manager::metrics_util::LogUpdateUIDismissalReason(
        ui_dismissal_reason, submission_event);
  } else {
    password_manager::metrics_util::LogSaveUIDismissalReason(
        ui_dismissal_reason, submission_event,
        /*user_state=*/absl::nullopt);
  }
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(ui_dismissal_reason);
  }
}

void SaveUpdatePasswordMessageDelegate::RecordSaveUpdateUIDismissalReason(
    SaveUpdatePasswordMessageDismissReason dismiss_reason) {
  LogSaveUpdatePasswordMessageDismissalReason(dismiss_reason);
  if (update_password_ && HasMultipleCredentialsStored()) {
    LogConfirmUsernameMessageDismissalReason(dismiss_reason);
    return;
  }
  if (update_password_) {
    LogUpdatePasswordMessageDismissalReason(dismiss_reason);
    return;
  }
  LogSavePasswordMessageDismissalReason(dismiss_reason);
}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
SaveUpdatePasswordMessageDelegate::GetPasswordEditDialogDismissReason(
    bool accepted) {
  DCHECK(password_edit_dialog_ != nullptr);

  if (update_password_ && HasMultipleCredentialsStored()) {
    return accepted ? SaveUpdatePasswordMessageDismissReason::
                          kAcceptInUsernameConfirmDialog
                    : SaveUpdatePasswordMessageDismissReason::kCancelInDialog;
  }
  return accepted ? SaveUpdatePasswordMessageDismissReason::kAcceptInDialog
                  : SaveUpdatePasswordMessageDismissReason::kCancelInDialog;
}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDismissReason
SaveUpdatePasswordMessageDelegate::GetSaveUpdatePasswordMessageDismissReason(
    messages::DismissReason dismiss_reason) {
  DCHECK(password_edit_dialog_ == nullptr);

  SaveUpdatePasswordMessageDismissReason save_update_dismiss_reason;
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
      save_update_dismiss_reason =
          SaveUpdatePasswordMessageDismissReason::kAccept;
      break;
    // This method is not called when the Edit password button is clicked.
    case messages::DismissReason::SECONDARY_ACTION:
      save_update_dismiss_reason =
          SaveUpdatePasswordMessageDismissReason::kNeverSave;
      break;
    default:
      save_update_dismiss_reason =
          SaveUpdatePasswordMessageDismissReason::kCancel;
      break;
  }
  return save_update_dismiss_reason;
}

// static
password_manager::metrics_util::UIDismissalReason
SaveUpdatePasswordMessageDelegate::
    MessageDismissReasonToPasswordManagerUIDismissalReason(
        messages::DismissReason dismiss_reason) {
  password_manager::metrics_util::UIDismissalReason ui_dismissal_reason;
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
      ui_dismissal_reason = password_manager::metrics_util::CLICKED_ACCEPT;
      break;
    case messages::DismissReason::SECONDARY_ACTION:
      ui_dismissal_reason = password_manager::metrics_util::CLICKED_NEVER;
      break;
    case messages::DismissReason::GESTURE:
      ui_dismissal_reason = password_manager::metrics_util::CLICKED_CANCEL;
      break;
    default:
      ui_dismissal_reason =
          password_manager::metrics_util::NO_DIRECT_INTERACTION;
      break;
  }
  return ui_dismissal_reason;
}
