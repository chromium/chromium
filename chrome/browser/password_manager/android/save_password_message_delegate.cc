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
SavePasswordMessageDelegate::~SavePasswordMessageDelegate() = default;

void SavePasswordMessageDelegate::DisplaySavePasswordPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save) {
  DCHECK_NE(nullptr, web_contents);
  DCHECK(form_to_save);

  // Dismiss previous message if it is displayed.
  DismissSavePasswordPrompt();
  DCHECK(message_ == nullptr);

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
  // All the DisplaySavePasswordPrompt parameters are passed to CreateMessage to
  // avoid a call to MessageDispatcherBridge::EnqueueMessage from test while
  // still providing decent test coverage.
  CreateMessage(web_contents, std::move(form_to_save), account_info);

  messages::MessageDispatcherBridge::EnqueueMessage(message_.get(),
                                                    web_contents_);
}

void SavePasswordMessageDelegate::DismissSavePasswordPrompt() {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::DismissMessage(message_.get(),
                                                      web_contents_);
  }
}

void SavePasswordMessageDelegate::CreateMessage(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    base::Optional<AccountInfo> account_info) {
  ui_dismissal_reason_ = password_manager::metrics_util::NO_DIRECT_INTERACTION;
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

  const base::string16 masked_password =
      base::string16(pending_credentials.password_value.size(), L'•');
  base::string16 description;
  if (account_info.has_value()) {
    description = l10n_util::GetStringFUTF16(
        IDS_SAVE_PASSWORD_SIGNED_IN_MESSAGE_DESCRIPTION,
        pending_credentials.username_value, masked_password,
        base::UTF8ToUTF16(account_info.value().email));
  } else {
    description.append(pending_credentials.username_value)
        .append(base::ASCIIToUTF16(" "))
        .append(masked_password);
  }

  message_->SetDescription(description);

  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD));
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_SETTINGS));
  message_->SetSecondaryActionText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLOCKLIST_BUTTON));

  message_->SetSecondaryActionCallback(base::BindOnce(
      &SavePasswordMessageDelegate::HandleNeverClick, base::Unretained(this)));

  // Recording metrics is not a part of message creation. It is included here to
  // ensure metrics recording test coverage.
  RecordMessageShownMetrics();
}

void SavePasswordMessageDelegate::HandleSaveClick() {
  form_to_save_->Save();
  ui_dismissal_reason_ = password_manager::metrics_util::CLICKED_ACCEPT;
}

void SavePasswordMessageDelegate::HandleNeverClick() {
  form_to_save_->Blocklist();
  ui_dismissal_reason_ = password_manager::metrics_util::CLICKED_NEVER;
  DismissSavePasswordPrompt();
}

void SavePasswordMessageDelegate::HandleDismissCallback() {
  // The message is dismissed. Record metrics and cleanup state.
  RecordDismissalReasonMetrics();
  message_.reset();
  form_to_save_.reset();
  // Following fields are also set in CreateMessage(). Resetting them here to
  // keep the state clean when no message is enqueued.
  web_contents_ = nullptr;
  ui_dismissal_reason_ = password_manager::metrics_util::NO_DIRECT_INTERACTION;
}

void SavePasswordMessageDelegate::RecordMessageShownMetrics() {
  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
    recorder->RecordPasswordBubbleShown(
        form_to_save_->GetCredentialSource(),
        password_manager::metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING);
  }
}

void SavePasswordMessageDelegate::RecordDismissalReasonMetrics() {
  password_manager::metrics_util::LogSaveUIDismissalReason(
      ui_dismissal_reason_, /*user_state=*/base::nullopt);
  if (form_to_save_->WasUnblocklisted()) {
    password_manager::metrics_util::LogSaveUIDismissalReasonAfterUnblocklisting(
        ui_dismissal_reason_);
  }
  if (auto* recorder = form_to_save_->GetMetricsRecorder()) {
    recorder->RecordUIDismissalReason(ui_dismissal_reason_);
  }
}
