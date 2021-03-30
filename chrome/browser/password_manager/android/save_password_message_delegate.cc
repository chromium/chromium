// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/save_password_message_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/password_infobar_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

SavePasswordMessageDelegate::SavePasswordMessageDelegate() = default;

SavePasswordMessageDelegate::~SavePasswordMessageDelegate() {
  DismissSavePasswordPrompt();
}

void SavePasswordMessageDelegate::DisplaySavePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  DCHECK_NE(nullptr, web_contents);
  DCHECK(form_to_save);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // is_saving_google_account indicates whether the user is syncing
  // passwords to their Google Account.
  const bool is_saving_google_account =
      password_bubble_experiment::IsSmartLockUser(
          ProfileSyncServiceFactory::GetForProfile(profile));

  base::Optional<AccountInfo> account_info =
      password_manager::GetAccountInfoForPasswordMessages(
          profile, is_saving_google_account);
  DisplaySavePasswordPromptInternal(web_contents, std::move(form_to_save),
                                    account_info);
}

void SavePasswordMessageDelegate::DismissSavePasswordPrompt() {
  DismissSavePasswordPromptInternal(messages::DismissReason::UNKNOWN);
}

void SavePasswordMessageDelegate::DismissSavePasswordPromptInternal(
    messages::DismissReason dismiss_reason) {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, dismiss_reason);
  }
}

void SavePasswordMessageDelegate::DisplaySavePasswordPromptInternal(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    base::Optional<AccountInfo> account_info) {
  // Dismiss previous message if it is displayed.
  DismissSavePasswordPrompt();
  DCHECK(message_ == nullptr);

  web_contents_ = web_contents;
  form_to_save_ = std::move(form_to_save);

  // Binding with base::Unretained(this) is safe here because
  // SavePasswordMessageDelegate owns message_. Callbacks won't be called after
  // the current object is destroyed.
  message_ = std::make_unique<messages::MessageWrapper>(
      base::BindOnce(&SavePasswordMessageDelegate::HandleSaveClick,
                     base::Unretained(this)),
      base::BindOnce(&SavePasswordMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));

  const password_manager::PasswordForm& pending_credentials =
      form_to_save_->GetPendingCredentials();

  int title_message_id = pending_credentials.federation_origin.opaque()
                             ? IDS_SAVE_PASSWORD
                             : IDS_SAVE_ACCOUNT;

  message_->SetTitle(l10n_util::GetStringUTF16(title_message_id));

  const std::u16string masked_password =
      std::u16string(pending_credentials.password_value.size(), L'•');
  std::u16string description;
  if (account_info.has_value()) {
    description = l10n_util::GetStringFUTF16(
        IDS_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION_GOOGLE_ACCOUNT,
        pending_credentials.username_value, masked_password,
        base::UTF8ToUTF16(account_info.value().email));
  } else {
    description.append(pending_credentials.username_value)
        .append(u" ")
        .append(masked_password);
  }

  message_->SetDescription(description);

  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD));
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS));
  message_->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON));

  message_->SetSecondaryActionCallback(base::BindOnce(
      &SavePasswordMessageDelegate::HandleNeverClick, base::Unretained(this)));

  RecordMessageShownMetrics();
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::NAVIGATION);
}

void SavePasswordMessageDelegate::HandleSaveClick() {
  form_to_save_->Save();
}

void SavePasswordMessageDelegate::HandleNeverClick() {
  form_to_save_->Blocklist();
  DismissSavePasswordPromptInternal(messages::DismissReason::SECONDARY_ACTION);
}

void SavePasswordMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  // The message is dismissed. Record metrics and cleanup state.
  RecordDismissalReasonMetrics(
      MessageDismissReasonToPasswordManagerUIDismissalReason(dismiss_reason));
  message_.reset();
  form_to_save_.reset();
  // web_contents_ is set in DisplaySavePasswordPromptInternal(). Resetting it
  // here to keep the state clean when no message is enqueued.
  web_contents_ = nullptr;
}

void SavePasswordMessageDelegate::RecordMessageShownMetrics() {
  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        form_to_save_->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

void SavePasswordMessageDelegate::RecordDismissalReasonMetrics(
    password_manager::metrics_util::UIDismissalReason ui_dismissal_reason) {
  password_manager::metrics_util::LogSaveUIDismissalReason(
      ui_dismissal_reason, /*user_state=*/base::nullopt);
  if (form_to_save_->WasUnblocklisted()) {
    password_manager::metrics_util::LogSaveUIDismissalReasonAfterUnblocklisting(
        ui_dismissal_reason);
  }
  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
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
