// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"

#include <memory>
#include <optional>
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
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "save_update_password_message_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

using password_manager::PasswordForm;
using password_manager::UsesSplitStoresAndUPMForLocal;

// Duration of message before timeout; 20 seconds.
const int kMessageDismissDurationMs = 20000;

constexpr base::TimeDelta kUpdateGMSCoreMessageDisplayDelay =
    base::Milliseconds(500);

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

void TryToShowAccessLossWarning(content::WebContents* web_contents,
                                PasswordAccessLossWarningBridge* bridge) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    if (profile && bridge->ShouldShowAccessLossNoticeSheet(
                       prefs, /*called_at_startup=*/false)) {
      bridge->MaybeShowAccessLossNoticeSheet(
          prefs, web_contents->GetTopLevelNativeWindow(), profile,
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kPasswordSaveUpdateMessage);
    }
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
      device_lock_bridge_(std::make_unique<DeviceLockBridge>()),
      access_loss_bridge_(
          std::make_unique<PasswordAccessLossWarningBridgeImpl>()) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    base::PassKey<class SaveUpdatePasswordMessageDelegateTest>,
    PasswordEditDialogFactory password_edit_dialog_factory,
    base::RepeatingCallback<
        void(gfx::NativeWindow,
             Profile*,
             password_manager::metrics_util::PasswordMigrationWarningTriggers)>
        create_migration_warning_callback,
    std::unique_ptr<DeviceLockBridge> device_lock_bridge,
    std::unique_ptr<PasswordAccessLossWarningBridge> access_loss_bridge)
    : SaveUpdatePasswordMessageDelegate(password_edit_dialog_factory,
                                        create_migration_warning_callback) {
  device_lock_bridge_ = std::move(device_lock_bridge);
  access_loss_bridge_ = std::move(access_loss_bridge);
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

  std::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(
          SyncServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile));
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
    std::optional<AccountInfo> account_info,
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

  account_email_ = GetAccountForMessageDescription(account_info);

  CreateMessage(update_password);
  RecordMessageShownMetrics();
  password_manager::metrics_util::LogFormSubmissionsVsSavePromptsHistogram(
      password_manager::metrics_util::SaveFlowStep::kSavePromptShown);
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
  } else if (!pending_credentials.IsFederatedCredential()) {
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

  // The cog button is always shown for the save message and for the update
  // message when there is just one password stored for the web site. When
  // there are multiple credentials stored, the dialog will be called anyway
  // from the followup button, so there are no options to put under the cog.
  if (!update_password || !use_followup_button) {
    SetupCogMenu(message_, update_password);
  }
}

void SaveUpdatePasswordMessageDelegate::SetupCogMenu(
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
  // If password is being updated in the account storage, the description should
  // contain for which account the update is made.
  if (IsUsingAccountStorage(pending_credentials.username_value)) {
    return l10n_util::GetStringFUTF16(
        update_password
            ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION
            : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION,
        base::UTF8ToUTF16(account_email_.value()));
  }
  return l10n_util::GetStringUTF16(
      update_password
          ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION
          : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION);
}

std::optional<std::string>
SaveUpdatePasswordMessageDelegate::GetAccountForMessageDescription(
    const std::optional<AccountInfo>& account_info) {
  if (!account_info.has_value()) {
    return std::nullopt;
  }

  return account_info->CanHaveEmailAddressDisplayed()
             ? account_info.value().email
             : account_info.value().full_name;
}

int SaveUpdatePasswordMessageDelegate::GetPrimaryButtonTextId(
    bool update_password,
    bool use_followup_button_text) {
  if (!update_password) {
    return IDS_PASSWORD_MANAGER_SAVE_BUTTON;
  }
  if (!use_followup_button_text) {
    return IDS_PASSWORD_MANAGER_UPDATE_BUTTON;
  }
  return IDS_PASSWORD_MANAGER_CONTINUE_BUTTON;
}

unsigned int SaveUpdatePasswordMessageDelegate::GetDisplayUsernames(
    std::vector<std::u16string>* usernames) {
  unsigned int selected_username_index = 0;
  // TODO(crbug.com/40675711): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      password_forms = passwords_state_.GetCurrentForms();
  const std::u16string& default_username =
      passwords_state_.form_manager()->GetPendingCredentials().username_value;
  for (const auto& form : password_forms) {
    usernames->push_back(form->username_value);
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &SaveUpdatePasswordMessageDelegate::MaybeNudgeToUpdateGmsCore,
            weak_ptr_factory_.GetWeakPtr()),
        kUpdateGMSCoreMessageDisplayDelay);
    return;
  }
  device_lock_bridge_->LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      web_contents_->GetNativeView()->GetWindowAndroid(),
      base::BindOnce(
          &SaveUpdatePasswordMessageDelegate::SavePasswordAfterDeviceLockUi,
          weak_ptr_factory_.GetWeakPtr()));
}

void SaveUpdatePasswordMessageDelegate::SavePasswordAfterDeviceLockUi(
    bool is_device_lock_requirement_met) {
  CHECK(device_lock_bridge_->RequiresDeviceLock());
  if (is_device_lock_requirement_met) {
    passwords_state_.form_manager()->Save();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &SaveUpdatePasswordMessageDelegate::MaybeNudgeToUpdateGmsCore,
            weak_ptr_factory_.GetWeakPtr()),
        kUpdateGMSCoreMessageDisplayDelay);
    TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                      web_contents_);
    TryToShowAccessLossWarning(web_contents_, access_loss_bridge_.get());
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
  const password_manager::PasswordForm& password_form =
      passwords_state_.form_manager()->GetPendingCredentials();
  const std::u16string& current_username = password_form.username_value;
  const std::u16string& current_password = password_form.password_value;

  CreatePasswordEditDialog();

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!password_edit_dialog_) {
    return;
  }

  std::vector<std::u16string> usernames;
  GetDisplayUsernames(&usernames);
  password_edit_dialog_->ShowPasswordEditDialog(
      usernames, current_username, current_password, account_email_);

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

  // If Device Lock UI needs to be shown and can be (i.e. WindowAndroid is
  // available), these lines are handled in the SavePasswordAfterDeviceLockUi()
  // callback.
  if (!(device_lock_bridge_->ShouldShowDeviceLockUi() &&
        web_contents_->GetNativeView()->GetWindowAndroid())) {
    if (dismiss_reason == messages::DismissReason::PRIMARY_ACTION) {
      TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                        web_contents_);
      TryToShowAccessLossWarning(web_contents_, access_loss_bridge_.get());
    }
    ClearState();
  }
}

bool SaveUpdatePasswordMessageDelegate::HasMultipleCredentialsStored() {
  // TODO(crbug.com/40675711): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      password_forms = passwords_state_.GetCurrentForms();
  return password_forms.size() > 1;
}

void SaveUpdatePasswordMessageDelegate::CreatePasswordEditDialog() {
  password_edit_dialog_ =
      password_edit_dialog_factory_.Run(web_contents_.get(), this);
}

void SaveUpdatePasswordMessageDelegate::HandleDialogDismissed(
    bool dialog_accepted) {
  RecordDismissalReasonMetrics(
      dialog_accepted ? password_manager::metrics_util::CLICKED_ACCEPT
                      : password_manager::metrics_util::CLICKED_CANCEL);

  password_edit_dialog_.reset();

  // If Device Lock UI needs to be shown and can be (i.e. WindowAndroid is
  // available), these lines are handled in the SavePasswordAfterDeviceLockUi()
  // callback.
  if (!(device_lock_bridge_->ShouldShowDeviceLockUi() &&
        web_contents_->GetNativeView()->GetWindowAndroid())) {
    TryToShowPasswordMigrationWarning(create_migration_warning_callback_,
                                      web_contents_);
    TryToShowAccessLossWarning(web_contents_, access_loss_bridge_.get());
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

bool SaveUpdatePasswordMessageDelegate::IsUsingAccountStorage(
    const std::u16string& username) {
  if (!account_email_) {
    return false;
  }

  // Pre-UPM the profile storage was used in fact as the account store (when
  // sync is on). So this is the cut-off for the users who are not using UPM
  // (this evaluates to using account store when the user is syncing and using
  // profile store when they are not syncing).
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!UsesSplitStoresAndUPMForLocal(profile->GetPrefs())) {
    return account_email_.has_value();
  }

  // Copy the pending password form here and assign the new username.
  password_manager::PasswordForm updated_credentials =
      passwords_state_.form_manager()->GetPendingCredentials();
  updated_credentials.username_value = username;
  return (passwords_state_.form_manager()->GetPasswordStoreForSaving(
              updated_credentials) &
          PasswordForm::Store::kAccountStore) ==
         PasswordForm::Store::kAccountStore;
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
  if (update_password_) {
    password_manager::metrics_util::LogUpdateUIDismissalReason(
        ui_dismissal_reason);
  } else {
    password_manager::metrics_util::LogSaveUIDismissalReason(
        ui_dismissal_reason, /*user_state=*/std::nullopt,
        /*log_adoption_metric=*/false);
  }
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(ui_dismissal_reason);
  }
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

void SaveUpdatePasswordMessageDelegate::MaybeNudgeToUpdateGmsCore() {
  if (passwords_state_.client()
          ->GetPasswordFeatureManager()
          ->ShouldUpdateGmsCore()) {
    passwords_state_.client()->ShowPasswordManagerErrorMessage(
        password_manager::ErrorMessageFlowType::kSaveFlow,
        password_manager::PasswordStoreBackendErrorType::
            kGMSCoreOutdatedSavingPossible);
  }
}
