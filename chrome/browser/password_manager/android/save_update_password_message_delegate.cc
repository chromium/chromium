// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/messages_feature.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using password_manager::features::kPasswordEditDialogWithDetails;

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate()
    : SaveUpdatePasswordMessageDelegate(
          base::BindRepeating(PasswordEditDialogBridge::Create)) {}

SaveUpdatePasswordMessageDelegate::SaveUpdatePasswordMessageDelegate(
    PasswordEditDialogFactory password_edit_dialog_factory)
    : password_edit_dialog_factory_(std::move(password_edit_dialog_factory)) {}

SaveUpdatePasswordMessageDelegate::~SaveUpdatePasswordMessageDelegate() {
  DCHECK(web_contents_ == nullptr);
}

void SaveUpdatePasswordMessageDelegate::DisplaySaveUpdatePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  DCHECK_NE(nullptr, web_contents);
  DCHECK(form_to_save);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  absl::optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(profile);
  DisplaySaveUpdatePasswordPromptInternal(web_contents, std::move(form_to_save),
                                          std::move(account_info),
                                          update_password);
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
    bool update_password) {
  // Dismiss previous message if it is displayed.
  DismissSaveUpdatePasswordPrompt();
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
      message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
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

  // The message duration experiment is controlled by a parameter, associated
  // with MessagesForAndroidPasswords feature and thus should be adjusted for
  // both save and update password prompt.
  int message_dismiss_duration_ms =
      messages::GetSavePasswordMessageDismissDurationMs();
  if (message_dismiss_duration_ms != 0)
    message_->SetDuration(message_dismiss_duration_ms);

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

  std::u16string description = GetMessageDescription(
      pending_credentials, update_password,
      password_manager::features::UsesUnifiedPasswordManagerUi());
  message_->SetDescription(description);

  update_password_ = update_password;

  bool use_followup_button_text = false;
  if (update_password) {
    std::vector<std::u16string> usernames;
    GetDisplayUsernames(&usernames);
    use_followup_button_text = usernames.size() > 1;
  }
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      GetPrimaryButtonTextId(update_password, use_followup_button_text)));

  if (password_manager::features::UsesUnifiedPasswordManagerUi()) {
    message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
        IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP));
    message_->DisableIconTint();
  } else {
    message_->SetIconResourceId(
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD));
  }

  if (!update_password)
    SetupCogMenu(message_);
}

void SaveUpdatePasswordMessageDelegate::SetupCogMenu(
    std::unique_ptr<messages::MessageWrapper>& message) {
  message->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  if (base::FeatureList::IsEnabled(kPasswordEditDialogWithDetails)) {
    message->SetSecondaryActionCallback(base::BindRepeating(
        &SaveUpdatePasswordMessageDelegate::DisplayEditDialog,
        base::Unretained(this)));
    return;
  }

  message->SetSecondaryButtonMenuText(l10n_util::GetStringUTF16(
      password_manager::features::UsesUnifiedPasswordManagerUi()
          ? IDS_PASSWORD_MESSAGE_NEVER_SAVE_MENU_ITEM
          : IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON));
  message->SetSecondaryActionCallback(base::BindRepeating(
      &SaveUpdatePasswordMessageDelegate::HandleNeverSaveClicked,
      base::Unretained(this)));
}

std::u16string SaveUpdatePasswordMessageDelegate::GetMessageDescription(
    const password_manager::PasswordForm& pending_credentials,
    bool update_password,
    bool unified_password_manager) {
  std::u16string description;
  if (unified_password_manager) {
    if (!account_email_.empty()) {
      description = l10n_util::GetStringFUTF16(
          update_password
              ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION
              : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION,
          base::UTF8ToUTF16(account_email_));
    } else {
      description = l10n_util::GetStringUTF16(
          update_password
              ? IDS_PASSWORD_MANAGER_UPDATE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION
              : IDS_PASSWORD_MANAGER_SAVE_PASSWORD_SIGNED_OUT_MESSAGE_DESCRIPTION);
    }
    return description;
  }

  if (!account_email_.empty()) {
    description = l10n_util::GetStringFUTF16(
        update_password
            ? IDS_UPDATE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_GOOGLE_ACCOUNT
            : IDS_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_GOOGLE_ACCOUNT,
        base::UTF8ToUTF16(account_email_));
  } else {
    // TODO(crbug.com/1188971): There is no password when federation_origin is
    // set. Instead we should display federated provider in the description.
    // GetDisplayFederation() returns federation origin for a given form.
    const std::u16string masked_password =
        std::u16string(pending_credentials.password_value.size(), L'•');
    description.append(pending_credentials.username_value)
        .append(u" ")
        .append(masked_password);
  }
  return description;
}

int SaveUpdatePasswordMessageDelegate::GetPrimaryButtonTextId(
    bool update_password,
    bool use_followup_button_text) {
  if (!update_password)
    return IDS_PASSWORD_MANAGER_SAVE_BUTTON;
  if (!use_followup_button_text)
    return IDS_PASSWORD_MANAGER_UPDATE_BUTTON;
  if (messages::UseFollowupButtonTextForUpdatePasswordButton())
    return IDS_PASSWORD_MANAGER_UPDATE_WITH_FOLLOWUP_BUTTON;
  return IDS_PASSWORD_MANAGER_CONTINUE_BUTTON;
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
  ClearState();
}

void SaveUpdatePasswordMessageDelegate::HandleSaveButtonClicked() {
  passwords_state_.form_manager()->Save();
}

void SaveUpdatePasswordMessageDelegate::DisplayEditDialog() {
  const std::u16string& current_username =
      passwords_state_.form_manager()->GetPendingCredentials().username_value;
  const std::u16string& current_password =
      passwords_state_.form_manager()->GetPendingCredentials().password_value;
  DisplaySavePasswordDialog(std::move(current_username),
                            std::move(current_password));
  DismissSaveUpdatePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegate::DisplaySavePasswordDialog(
    std::u16string current_username,
    std::u16string current_password) {
  CreatePasswordEditDialog();

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!password_edit_dialog_)
    return;

  password_edit_dialog_->ShowSavePasswordDialog(
      current_username, current_password, account_email_);
}

void SaveUpdatePasswordMessageDelegate::HandleNeverSaveClicked() {
  passwords_state_.form_manager()->Blocklist();
  DismissSaveUpdatePasswordMessage(messages::DismissReason::SECONDARY_ACTION);
}

void SaveUpdatePasswordMessageDelegate::HandleUpdateButtonClicked() {
  std::vector<std::u16string> usernames;
  int selected_username_index = GetDisplayUsernames(&usernames);
  if (usernames.size() > 1) {
    DisplayUpdatePasswordDialog(std::move(usernames), selected_username_index);
  } else {
    passwords_state_.form_manager()->Save();
  }
}

void SaveUpdatePasswordMessageDelegate::DisplayUpdatePasswordDialog(
    std::vector<std::u16string> usernames,
    int selected_username_index) {
  CreatePasswordEditDialog();

  // Password edit dialog factory method can return nullptr when web_contents
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!password_edit_dialog_)
    return;

  password_edit_dialog_->ShowUpdatePasswordDialog(
      usernames, selected_username_index,
      passwords_state_.form_manager()->GetPendingCredentials().password_value,
      account_email_);
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
      base::BindOnce(&SaveUpdatePasswordMessageDelegate::HandleDialogDismissed,
                     base::Unretained(this)));
}

void SaveUpdatePasswordMessageDelegate::HandleDialogDismissed(
    bool dialogAccepted) {
  password_edit_dialog_.reset();
  RecordDismissalReasonMetrics(
      dialogAccepted ? password_manager::metrics_util::CLICKED_ACCEPT
                     : password_manager::metrics_util::CLICKED_CANCEL);
  ClearState();
}

void SaveUpdatePasswordMessageDelegate::HandleSavePasswordFromDialog(
    const std::u16string& username,
    const std::u16string& password) {
  UpdatePasswordFormUsernameAndPassword(username, password,
                                        passwords_state_.form_manager());
  passwords_state_.form_manager()->Save();
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
    if (passwords_state_.form_manager()->WasUnblocklisted()) {
      password_manager::metrics_util::
          LogSaveUIDismissalReasonAfterUnblocklisting(ui_dismissal_reason);
    }
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
