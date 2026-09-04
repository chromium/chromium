// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"
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
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "save_update_password_message_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

using password_manager::PasswordForm;
using password_manager::PasswordString;

// Duration of message before timeout; 20 seconds.
const int kMessageDismissDurationMs = 20000;

}  // namespace

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate()
    : SaveUpdatePasswordMessageDelegate(
          base::BindRepeating(PasswordEditDialogBridge::Create)) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    PasswordEditDialogFactory password_edit_dialog_factory)
    : password_edit_dialog_factory_(std::move(password_edit_dialog_factory)),
      device_lock_bridge_(std::make_unique<DeviceLockBridge>()),
      password_manager_error_message_helper_bridge_(
          std::make_unique<PasswordManagerErrorMessageHelperBridgeImpl>()) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    base::PassKey<class SaveUpdatePasswordMessageDelegateTest>,
    PasswordEditDialogFactory password_edit_dialog_factory,
    std::unique_ptr<DeviceLockBridge> device_lock_bridge,
    std::unique_ptr<PasswordManagerErrorMessageHelperBridge>
        password_manager_error_message_helper_bridge)
    : SaveUpdatePasswordMessageDelegate(password_edit_dialog_factory) {
  device_lock_bridge_ = std::move(device_lock_bridge);
  password_manager_error_message_helper_bridge_ =
      std::move(password_manager_error_message_helper_bridge);
}

SaveUpdatePasswordMessageDelegate::~SaveUpdatePasswordMessageDelegate() =
    default;

void SaveUpdatePasswordMessageDelegate::DisplaySaveUpdatePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password,
    password_manager::PasswordManagerClient* password_manager_client) {
  CHECK_NE(nullptr, web_contents, base::NotFatalUntil::M152);
  CHECK(form_to_save, base::NotFatalUntil::M152);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  std::optional<AccountInfo> account_info = GetAccountInfoForPasswordMessages(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
  DisplaySaveUpdatePasswordPromptInternal(
      web_contents, std::move(form_to_save), std::move(account_info),
      update_password, password_manager_client);
}

void SaveUpdatePasswordMessageDelegate::DismissAllActiveUI() {
  // If no prompt or background flow is active, there is nothing to dismiss.
  // WebContentsDestroyed() calls this method for every closed tab.
  if (state_ == State::kIdle) {
    return;
  }
  // This dismissal is programmatic (e.g. WebContents being destroyed or a new
  // prompt replacing this one), not user-initiated.
  weak_ptr_factory_.InvalidateWeakPtrs();
  account_password_store_observation_.Reset();

  // Record dismissal metrics only for the currently active UI to avoid
  // duplicate logging when both dialog and message pointers are non-null
  // (e.g. while message dismissal is in-flight after dialog was opened).
  if (state_ == State::kEditDialogShowing ||
      state_ == State::kSaveUpdatePromptShowing ||
      state_ == State::kRepromptShowing) {
    RecordDismissalReasonMetrics(
        password_manager::metrics_util::NO_DIRECT_INTERACTION);
  }

  if (password_edit_dialog_ != nullptr) {
    password_edit_dialog_->Dismiss();
  }
  if (message_ != nullptr) {
    DismissSaveUpdatePasswordMessage(messages::DismissReason::UNKNOWN);
    // InvalidateWeakPtrs() above prevents HandleMessageDismissed from running,
    // so message_ must be reset explicitly.
    message_.reset();
  }
  if (confirmation_message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        confirmation_message_.get(), messages::DismissReason::UNKNOWN);
    confirmation_message_.reset();
  }
  ClearState();
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
  DismissAllActiveUI();
  CHECK_EQ(state_, State::kIdle);
  CHECK(password_manager_client);

  web_contents_ = web_contents;
  passwords_state_.set_client(password_manager_client);

  if (password_manager_client->GetAccountPasswordStore() &&
      !account_password_store_observation_.IsObserving()) {
    account_password_store_observation_.Observe(
        password_manager_client->GetAccountPasswordStore());
  }
  if (update_password) {
    passwords_state_.OnUpdatePassword(std::move(form_to_save));
  } else {
    passwords_state_.OnPendingPassword(std::move(form_to_save));
  }

  account_email_ = GetAccountForMessageDescription(account_info);

  CreateMessage(update_password);
  RecordMessageShownMetrics(update_password);
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
  TransitionTo(State::kSaveUpdatePromptShowing);
}

void SaveUpdatePasswordMessageDelegate::CreateMessage(bool update_password) {
  messages::MessageIdentifier message_id =
      update_password ? messages::MessageIdentifier::UPDATE_PASSWORD
                      : messages::MessageIdentifier::SAVE_PASSWORD;
  base::OnceClosure callback =
      update_password
          ? base::BindOnce(
                &SaveUpdatePasswordMessageDelegate::HandleUpdateButtonClicked,
                weak_ptr_factory_.GetWeakPtr())
          : base::BindOnce(
                &SaveUpdatePasswordMessageDelegate::HandleSaveButtonClicked,
                weak_ptr_factory_.GetWeakPtr());
  message_ = std::make_unique<messages::MessageWrapper>(
      message_id, std::move(callback),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::HandleMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));

  message_->SetDuration(kMessageDismissDurationMs);

  const password_manager::PasswordForm& pending_credentials =
      passwords_state_.form_manager()->GetPendingCredentials();

  message_->SetTitle(GetMessageTitle(
      update_password, pending_credentials.IsFederatedCredential()));

  update_password_ = update_password;

  const bool is_saving_blocked_by_trusted_vault_error =
      IsSavingBlockedByTrustedVaultError();

  std::u16string description =
      GetMessageDescription(pending_credentials, update_password,
                            is_saving_blocked_by_trusted_vault_error);
  message_->SetDescription(description);

  bool use_followup_button = HasMultipleCredentialsStored();
  message_->SetPrimaryButtonText(
      GetPrimaryButtonText(update_password, use_followup_button,
                           is_saving_blocked_by_trusted_vault_error));

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
        weak_ptr_factory_.GetWeakPtr(), update_password));
  } else {
    message_->SetSecondaryMenuItemSelectedCallback(base::BindRepeating(
        &SaveUpdatePasswordMessageDelegate::HandleSaveMessageMenuItemClick,
        weak_ptr_factory_.GetWeakPtr()));
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

std::u16string SaveUpdatePasswordMessageDelegate::GetMessageTitle(
    bool update_password,
    bool is_federated_credential) {
  if (update_password) {
    return l10n_util::GetStringUTF16(IDS_UPDATE_PASSWORD);
  }
  if (!is_federated_credential) {
    return l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD);
  }
  return l10n_util::GetStringUTF16(IDS_SAVE_ACCOUNT);
}

std::u16string SaveUpdatePasswordMessageDelegate::GetMessageDescription(
    const password_manager::PasswordForm& pending_credentials,
    bool update_password,
    bool is_saving_blocked_by_trusted_vault_error) {
  if (is_saving_blocked_by_trusted_vault_error) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_BUBBLES_SUBTITLE_TRUSTED_VAULT_ERROR);
  }

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

  return std::string(account_info->CanHaveEmailAddressDisplayed()
                         ? account_info->GetEmail()
                         : account_info->GetFullName().value_or(""));
}

std::u16string SaveUpdatePasswordMessageDelegate::GetPrimaryButtonText(
    bool update_password,
    bool use_followup_button_text,
    bool is_saving_blocked_by_trusted_vault_error) {
  if (is_saving_blocked_by_trusted_vault_error) {
    return l10n_util::GetStringUTF16(IDS_CONTINUE);
  }
  if (!update_password) {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON);
  }
  if (!use_followup_button_text) {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON);
  }
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONTINUE_BUTTON);
}

unsigned int SaveUpdatePasswordMessageDelegate::GetDisplayUsernames(
    std::vector<std::u16string>* usernames) {
  unsigned int selected_username_index = 0;
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
  StartSavePasswordFlow();
}

void SaveUpdatePasswordMessageDelegate::StartSavePasswordFlow() {
  if (!device_lock_bridge_->ShouldShowDeviceLockUi()) {
    SolveTrustedVaultCheck(/*is_device_lock_requirement_met=*/true);
    return;
  }

  if (!web_contents_ || !web_contents_->GetNativeView()->GetWindowAndroid()) {
    SolveTrustedVaultCheck(/*is_device_lock_requirement_met=*/false);
    return;
  }

  TransitionTo(State::kWaitingForDeviceLock);
  device_lock_bridge_->LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      web_contents_->GetNativeView()->GetWindowAndroid(),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::SolveTrustedVaultCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SaveUpdatePasswordMessageDelegate::SolveTrustedVaultCheck(
    bool is_device_lock_requirement_met) {
  if (state_ != State::kWaitingForDeviceLock &&
      state_ != State::kSaveUpdatePromptShowing &&
      state_ != State::kEditDialogShowing &&
      state_ != State::kRepromptShowing) {
    return;
  }

  if (!is_device_lock_requirement_met) {
    if (IsSavingBlockedByTrustedVaultError()) {
      password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
          password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
              kDeviceLockCanceled);
    }
    if (password_edit_dialog_ != nullptr) {
      password_edit_dialog_->Dismiss();
    }
    DismissSaveUpdatePasswordMessage(messages::DismissReason::UNKNOWN);
    TransitionTo(State::kDismissing);
    return;
  }

  if (IsSavingBlockedByTrustedVaultError()) {
    TransitionTo(State::kWaitingForTrustedVault);
    password_manager_error_message_helper_bridge_
        ->StartTrustedVaultKeyRetrievalFlow(
            web_contents_,
            trusted_vault::TrustedVaultUserActionTriggerForUMA::
                kPasswordSavePrompt,
            base::BindOnce(
                &SaveUpdatePasswordMessageDelegate::OnTrustedVaultRecoveryDone,
                weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  SaveFormManager(/*show_confirmation_message=*/false);
  TransitionTo(State::kDismissing);
}

void SaveUpdatePasswordMessageDelegate::OnTrustedVaultRecoveryDone() {
  // While the key retrieval flow was running, the error may have already been
  // resolved (or a new store error encountered) via OnErrorStateChanged(),
  // transitioning away from `kWaitingForTrustedVault`.
  if (state_ != State::kWaitingForTrustedVault) {
    return;
  }

  // Check if the trusted vault error is still present and reshow the message.
  // If the trusted vault error is resolved, save the pending credential and
  // show confirmation.
  if (IsSavingBlockedByTrustedVaultError()) {
    CreateMessage(update_password_);
    RecordMessageShownMetrics(update_password_);
    messages::MessageDispatcherBridge::Get()->EnqueueMessage(
        message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
        messages::MessagePriority::kUrgent);
    TransitionTo(State::kRepromptShowing);
  } else {
    SaveAfterTrustedVaultResolution();
  }
}

void SaveUpdatePasswordMessageDelegate::SaveAfterTrustedVaultResolution() {
  SaveFormManager(/*show_confirmation_message=*/true);
  password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
      password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
          kSavedSuccessfully);
  if (confirmation_message_ != nullptr) {
    TransitionTo(State::kConfirmationShowing);
  } else {
    TransitionTo(State::kDismissing);
  }
  // Dismiss the message prompt if it is currently displayed (e.g. if the error
  // was resolved in the background while the reprompt was showing).
  DismissSaveUpdatePasswordMessage(messages::DismissReason::UNKNOWN);
}

void SaveUpdatePasswordMessageDelegate::SaveFormManager(
    bool show_confirmation_message) {
  passwords_state_.form_manager()->Save();

  const password_manager::StoredCredential* changed_credential_with_backup =
      password_manager_util::FindChangedPasswordLoginWithBackup(
          *passwords_state_.form_manager());
  if (changed_credential_with_backup &&
      changed_credential_with_backup->GetPasswordBackup() ==
          passwords_state_.form_manager()
              ->GetPendingCredentials()
              .password_value) {
    password_manager::metrics_util::LogPrimaryPasswordUpdatedWithBackup(
        web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  if (show_confirmation_message &&
      base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSaveInContextErrorResolution)) {
    ShowConfirmationMessage();
  }
}

void SaveUpdatePasswordMessageDelegate::ShowConfirmationMessage() {
  CHECK(confirmation_message_ == nullptr);

  if (web_contents_ == nullptr) {
    return;
  }

  confirmation_message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::PASSWORD_SAVED_CONFIRMATION,
      /*action_callback=*/base::DoNothing(),
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::
                         HandleConfirmationMessageDismissed,
                     weak_ptr_factory_.GetWeakPtr()));

  confirmation_message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE));

  std::u16string description = l10n_util::GetStringUTF16(
      IDS_PASSWORD_SAVED_CONFIRMATION_MESSAGE_DESCRIPTION);
  confirmation_message_->SetDescription(description);

  confirmation_message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_OK));

  // TODO(crbug.com/545522304): Check if we need to change the icon for
  // non-branded builds.

  // IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP maps to the Google Password Manager
  // logo on branded builds, and to a generic key icon on non-branded builds.
  confirmation_message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP));
  confirmation_message_->DisableIconTint();

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      confirmation_message_.get(), web_contents_,
      messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
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
    StartSavePasswordFlow();
  }
}

void SaveUpdatePasswordMessageDelegate::DisplayEditDialog(
    bool update_password) {
  const password_manager::PasswordForm& password_form =
      passwords_state_.form_manager()->GetPendingCredentials();
  const std::u16string& current_username = password_form.username_value;

  CreatePasswordEditDialog();

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/40672358 for details.
  if (!password_edit_dialog_) {
    DismissAllActiveUI();
    return;
  }

  TransitionTo(State::kEditDialogShowing);
  std::vector<std::u16string> usernames;
  GetDisplayUsernames(&usernames);
  password_edit_dialog_->ShowPasswordEditDialog(
      usernames, current_username, password_form.password_value.value(),
      account_email_);

  DismissSaveUpdatePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  message_.reset();
  if (state_ == State::kEditDialogShowing) {
    // The user triggered password edit dialog. Don't cleanup internal
    // datastructures, dialog dismiss callback will perform cleanup.
    return;
  }

  // Record metrics.
  RecordDismissalReasonMetrics(
      MessageDismissReasonToPasswordManagerUIDismissalReason(dismiss_reason));

  if (IsSavingBlockedByTrustedVaultError()) {
    if (dismiss_reason == messages::DismissReason::TIMER) {
      password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
          password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
              kMessageTimedOut);
    } else if (dismiss_reason == messages::DismissReason::SECONDARY_ACTION) {
      password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
          password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
              kNeverForThisSite);
    } else if (dismiss_reason == messages::DismissReason::GESTURE ||
               dismiss_reason == messages::DismissReason::CLOSE_BUTTON) {
      password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
          password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
              kUserDismissedPrompt);
    }
  }

  if (state_ == State::kSaveUpdatePromptShowing ||
      state_ == State::kRepromptShowing) {
    TransitionTo(State::kDismissing);
  } else {
    MaybeCleanUpState();
  }
}

bool SaveUpdatePasswordMessageDelegate::HasMultipleCredentialsStored() {
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
  password_edit_dialog_.reset();

  RecordDismissalReasonMetrics(
      dialog_accepted ? password_manager::metrics_util::CLICKED_ACCEPT
                      : password_manager::metrics_util::CLICKED_CANCEL);

  if (!dialog_accepted && IsSavingBlockedByTrustedVaultError()) {
    password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
        password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
            kUserDismissedPrompt);
  }

  if (state_ == State::kEditDialogShowing) {
    TransitionTo(State::kDismissing);
  } else {
    MaybeCleanUpState();
  }
}

void SaveUpdatePasswordMessageDelegate::HandleSavePasswordFromDialog(
    const std::u16string& username,
    const std::u16string& password) {
  // TODO(crbug.com/513276101): Explicit construction of std::u16string
  // R-Value to be removed once argument converted to PasswordString
  UpdatePasswordFormUsernameAndPassword(
      username, PasswordString(std::u16string(password)),
      passwords_state_.form_manager());
  StartSavePasswordFlow();
}

void SaveUpdatePasswordMessageDelegate::HandleConfirmationMessageDismissed(
    messages::DismissReason dismiss_reason) {
  confirmation_message_.reset();
  TransitionTo(State::kDismissing);
}

void SaveUpdatePasswordMessageDelegate::TransitionTo(State new_state) {
  state_ = new_state;
  MaybeCleanUpState();
}

void SaveUpdatePasswordMessageDelegate::MaybeCleanUpState() {
  if (state_ == State::kDismissing && message_ == nullptr &&
      confirmation_message_ == nullptr && password_edit_dialog_ == nullptr) {
    ClearState();
  }
}

bool SaveUpdatePasswordMessageDelegate::IsUsingAccountStorage(
    const std::u16string& username) {
  if (!account_email_) {
    return false;
  }

  // After UPM, an updated credential can be saved either to the local or
  // account storage, so the credential itself needs to be checked to determine
  // whether account storage messaging needs to be displayed.

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
  CHECK(message_ == nullptr);
  CHECK(confirmation_message_ == nullptr);
  CHECK(password_edit_dialog_ == nullptr);

  account_password_store_observation_.Reset();
  state_ = State::kIdle;
  // ManagePasswordsState::OnInactive() requires a non-null client
  // (DCHECK(client_)). Only reset passwords_state_ if it was initialized.
  if (passwords_state_.client()) {
    passwords_state_.OnInactive();
  }
  // web_contents_ is set in DisplaySaveUpdatePasswordPromptInternal().
  // Resetting it here to keep the state clean when no message is enqueued.
  web_contents_ = nullptr;
}

bool SaveUpdatePasswordMessageDelegate::IsSavingBlockedByTrustedVaultError()
    const {
  // Updating passwords does not require resolving trusted vault keys in
  // context. Verify `passwords_state_.client()` is non-null before passing
  // to `password_manager_util::IsSavingBlockedByTrustedVaultError`.
  return !update_password_ && passwords_state_.client() &&
         password_manager_util::IsSavingBlockedByTrustedVaultError(
             passwords_state_.client(), passwords_state_.form_manager());
}

void SaveUpdatePasswordMessageDelegate::RecordMessageShownMetrics(
    bool update_password) {
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        passwords_state_.form_manager()->GetCredentialSource(),
        update_password
            ? password_manager::metrics_util::
                  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE
            : password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

void SaveUpdatePasswordMessageDelegate::RecordDismissalReasonMetrics(
    password_manager::metrics_util::UIDismissalReason ui_dismissal_reason) {
  // Asynchronous message dismissals from Java can arrive after the tab has
  // closed and internal state has already been cleared.
  if (!passwords_state_.form_manager()) {
    return;
  }
  if (update_password_) {
    password_manager::metrics_util::LogUpdateUIDismissalReason(
        ui_dismissal_reason);
  } else {
    std::optional<password_manager::ActionableError> saving_blocked_error;
    if (IsSavingBlockedByTrustedVaultError()) {
      saving_blocked_error =
          password_manager::ActionableError::kTrustedVaultKeyNeeded;
    }
    password_manager::metrics_util::LogSaveUIDismissalReason(
        ui_dismissal_reason, /*user_state=*/std::nullopt,
        /*log_adoption_metric=*/false, saving_blocked_error);
  }
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(ui_dismissal_reason);
  }
}

void SaveUpdatePasswordMessageDelegate::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& changes) {}

void SaveUpdatePasswordMessageDelegate::OnLoginsRetained(
    password_manager::PasswordStoreInterface* store,
    const std::vector<password_manager::StoredCredential>&
        retained_credentials) {}

void SaveUpdatePasswordMessageDelegate::OnErrorStateChanged(
    password_manager::PasswordStoreInterface* store,
    password_manager::ActionableError changed_error) {
  // Only handle the error while waiting for the trusted vault key retrieval
  // flow or when in `kRepromptShowing` (where the flow may have already
  // completed). The current implementation always skips trusted vault unlock
  // for update password flow so that doesn't need to be explicitly checked
  // here.
  if (state_ != State::kWaitingForTrustedVault &&
      state_ != State::kRepromptShowing) {
    return;
  }

  if (changed_error == password_manager::ActionableError::kNoError &&
      !IsSavingBlockedByTrustedVaultError()) {
    SaveAfterTrustedVaultResolution();
  } else if (changed_error != password_manager::ActionableError::kNoError &&
             changed_error !=
                 password_manager::ActionableError::kTrustedVaultKeyNeeded) {
    password_manager::metrics_util::LogSaveWithTrustedVaultErrorOutcome(
        password_manager::metrics_util::SaveWithTrustedVaultErrorOutcome::
            kNewStoreError);
    TransitionTo(State::kDismissing);
    DismissSaveUpdatePasswordMessage(messages::DismissReason::UNKNOWN);
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
