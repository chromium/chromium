// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_password_message_delegate.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

SavePasswordMessageDelegate::SavePasswordMessageDelegate()
    : SavePasswordMessageDelegate(
          base::BindRepeating(PasswordEditDialogBridge::Create)) {}

SavePasswordMessageDelegate::SavePasswordMessageDelegate(
    PasswordEditDialogFactory password_edit_dialog_factory)
    : password_edit_dialog_factory_(std::move(password_edit_dialog_factory)) {}

SavePasswordMessageDelegate::~SavePasswordMessageDelegate() {
  DCHECK(web_contents_ == nullptr);
}

void SavePasswordMessageDelegate::DisplaySavePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  DCHECK_NE(nullptr, web_contents);
  DCHECK(form_to_save);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // is_saving_google_account indicates whether the user is syncing
  // passwords to their Google Account.
  const bool is_saving_google_account =
      password_bubble_experiment::IsSmartLockUser(
          ProfileSyncServiceFactory::GetForProfile(profile));

  absl::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(
          profile, is_saving_google_account);
  DisplaySavePasswordPromptInternal(web_contents, std::move(form_to_save),
                                    std::move(account_info), update_password);
}

void SavePasswordMessageDelegate::DismissSavePasswordPrompt() {
  if (password_edit_dialog_ != nullptr) {
    password_edit_dialog_->Dismiss();
  }
  DismissSavePasswordMessage(messages::DismissReason::UNKNOWN);
}

void SavePasswordMessageDelegate::DismissSavePasswordMessage(
    messages::DismissReason dismiss_reason) {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, dismiss_reason);
  }
}

void SavePasswordMessageDelegate::DisplaySavePasswordPromptInternal(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    absl::optional<AccountInfo> account_info,
    bool update_password) {
  // Dismiss previous message if it is displayed.
  DismissSavePasswordPrompt();
  DCHECK(message_ == nullptr);
  DCHECK(password_edit_dialog_ == nullptr);

  web_contents_ = web_contents;
  passwords_state_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents_));
  if (update_password) {
    passwords_state_.OnUpdatePassword(std::move(form_to_save));
  } else {
    passwords_state_.OnPendingPassword(std::move(form_to_save));
  }
  account_email_ =
      account_info.has_value() ? account_info.value().email : std::string();

  CreateMessage(update_password);
  RecordMessageShownMetrics();
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

void SavePasswordMessageDelegate::CreateMessage(bool update_password) {
  // Binding with base::Unretained(this) is safe here because
  // SavePasswordMessageDelegate owns message_. Callbacks won't be called after
  // the current object is destroyed.
  messages::MessageIdentifier message_id =
      update_password ? messages::MessageIdentifier::UPDATE_PASSWORD
                      : messages::MessageIdentifier::SAVE_PASSWORD;
  message_ = std::make_unique<messages::MessageWrapper>(
      message_id,
      base::BindOnce(&SavePasswordMessageDelegate::HandleSaveButtonClicked,
                     base::Unretained(this)),
      base::BindOnce(&SavePasswordMessageDelegate::HandleMessageDismissed,
                     base::Unretained(this)));

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

  // TODO(crbug.com/1188971): There is no password when federation_origin is
  // set. Instead we should display federated provider in the description.
  // GetDisplayFederation() returns federation origin for a given form.
  const std::u16string masked_password =
      std::u16string(pending_credentials.password_value.size(), L'•');
  std::u16string description;
  if (!account_email_.empty()) {
    description = l10n_util::GetStringFUTF16(
        IDS_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_GOOGLE_ACCOUNT,
        pending_credentials.username_value, masked_password,
        base::UTF8ToUTF16(account_email_));
  } else {
    description.append(pending_credentials.username_value)
        .append(u" ")
        .append(masked_password);
  }
  message_->SetDescription(description);

  int primary_button_message_id = update_password
                                      ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                      : IDS_PASSWORD_MANAGER_SAVE_BUTTON;
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(primary_button_message_id));

  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD));

  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS));
  if (!update_password) {
    message_->SetSecondaryButtonMenuText(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON));
  }
  // TODO(crbug.com/1188971): Currently only update password message triggers
  // password edit dialog in response to tap on the gear icon. Update password
  // edit dialog for save password scenario and switch save password message
  // behavior to match update password message.
  base::OnceClosure secondary_action_callback =
      update_password
          ? base::BindOnce(
                &SavePasswordMessageDelegate::HandleDisplayEditDialog,
                base::Unretained(this))
          : base::BindOnce(&SavePasswordMessageDelegate::HandleNeverSaveClicked,
                           base::Unretained(this));
  message_->SetSecondaryActionCallback(std::move(secondary_action_callback));
}

void SavePasswordMessageDelegate::HandleMessageDismissed(
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
  ClearState();
}

void SavePasswordMessageDelegate::HandleSaveButtonClicked() {
  passwords_state_.form_manager()->Save();
}

void SavePasswordMessageDelegate::HandleNeverSaveClicked() {
  passwords_state_.form_manager()->Blocklist();
  DismissSavePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SavePasswordMessageDelegate::HandleDisplayEditDialog() {
  // Binding with base::Unretained(this) is safe here because
  // SavePasswordMessageDelegate owns password_edit_dialog_. Callbacks won't be
  // called after the SavePasswordMessageDelegate object is destroyed.
  password_edit_dialog_ = password_edit_dialog_factory_.Run(
      web_contents_,
      base::BindOnce(&SavePasswordMessageDelegate::HandleSavePasswordFromDialog,
                     base::Unretained(this)),
      base::BindOnce(&SavePasswordMessageDelegate::HandleDialogDismissed,
                     base::Unretained(this)));
  // It is important to dismiss the message after the dialog is created. The
  // code in HandleMessageDismissed checks password_edit_dialog_ to decide
  // whether to clear state.
  DismissSavePasswordMessage(messages::DismissReason::SECONDARY_ACTION);

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!password_edit_dialog_)
    return;

  std::vector<std::u16string> usernames;
  int selected_username_index = GetDisplayUsernames(&usernames);

  std::u16string origin = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(passwords_state_.form_manager()->GetURL()),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  password_edit_dialog_->Show(
      usernames, selected_username_index,
      passwords_state_.form_manager()->GetPendingCredentials().password_value,
      origin, account_email_);
}

unsigned int SavePasswordMessageDelegate::GetDisplayUsernames(
    std::vector<std::u16string>* usernames) {
  unsigned int selected_username_index = 0;
  // TODO(crbug.com/1054410): Fix the update logic to use all best matches,
  // rather than current_forms which is best_matches without PSL-matched
  // credentials.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      password_forms = passwords_state_.GetCurrentForms();
  const std::u16string& default_username =
      passwords_state_.form_manager()->GetPendingCredentials().username_value;
  if (password_forms.size() > 1) {
    // If multiple credentials can be updated, we display a dropdown with all
    // the corresponding usernames.
    for (const auto& form : password_forms) {
      usernames->push_back(GetDisplayUsername(*form));
      if (form->username_value == default_username) {
        selected_username_index = usernames->size() - 1;
      }
    }
  } else {
    usernames->push_back(GetDisplayUsername(
        passwords_state_.form_manager()->GetPendingCredentials()));
  }
  return selected_username_index;
}

void SavePasswordMessageDelegate::HandleDialogDismissed(bool dialogAccepted) {
  password_edit_dialog_.reset();
  RecordDismissalReasonMetrics(
      dialogAccepted ? password_manager::metrics_util::CLICKED_ACCEPT
                     : password_manager::metrics_util::CLICKED_CANCEL);
  ClearState();
}

void SavePasswordMessageDelegate::HandleSavePasswordFromDialog(
    int selected_username_index) {
  if (passwords_state_.GetCurrentForms().size() > 1) {
    UpdatePasswordFormUsernameAndPassword(
        passwords_state_.GetCurrentForms()[selected_username_index]
            ->username_value,
        passwords_state_.form_manager()->GetPendingCredentials().password_value,
        passwords_state_.form_manager());
  }
  passwords_state_.form_manager()->Save();
}

void SavePasswordMessageDelegate::ClearState() {
  DCHECK(message_ == nullptr);
  DCHECK(password_edit_dialog_ == nullptr);

  passwords_state_.OnInactive();
  // web_contents_ is set in DisplaySavePasswordPromptInternal(). Resetting it
  // here to keep the state clean when no message is enqueued.
  web_contents_ = nullptr;
}

void SavePasswordMessageDelegate::RecordMessageShownMetrics() {
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        passwords_state_.form_manager()->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

void SavePasswordMessageDelegate::RecordDismissalReasonMetrics(
    password_manager::metrics_util::UIDismissalReason ui_dismissal_reason) {
  password_manager::metrics_util::LogSaveUIDismissalReason(
      ui_dismissal_reason, /*user_state=*/absl::nullopt);
  if (passwords_state_.form_manager()->WasUnblocklisted()) {
    password_manager::metrics_util::LogSaveUIDismissalReasonAfterUnblocklisting(
        ui_dismissal_reason);
  }
  if (auto* recorder = passwords_state_.form_manager()->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(ui_dismissal_reason);
  }
}

// static
password_manager::metrics_util::UIDismissalReason SavePasswordMessageDelegate::
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
