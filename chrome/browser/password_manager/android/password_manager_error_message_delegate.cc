// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/signin/signin_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

PasswordManagerErrorMessageDelegate::PasswordManagerErrorMessageDelegate(
    std::unique_ptr<PasswordManagerSignInHelperBridge> bridge_)
    : sign_in_bridge_(std::move(bridge_)) {}

PasswordManagerErrorMessageDelegate::~PasswordManagerErrorMessageDelegate() =
    default;

void PasswordManagerErrorMessageDelegate::DisplayPasswordManagerErrorMessage(
    content::WebContents* web_contents,
    bool save_password) {
  DCHECK(web_contents);

  // Dismiss previous message if it is displayed.
  DismissPasswordManagerErrorMessage(messages::DismissReason::UNKNOWN);

  CreateMessage(web_contents, save_password);
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
}

void PasswordManagerErrorMessageDelegate::DismissPasswordManagerErrorMessage(
    messages::DismissReason dismiss_reason) {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

void PasswordManagerErrorMessageDelegate::CreateMessage(
    content::WebContents* web_contents,
    bool save_password) {
  messages::MessageIdentifier message_id =
      messages::MessageIdentifier::PASSWORD_MANAGER_ERROR;
  // Binding with base::Unretained(this) is safe here because
  // PasswordManagerErrorMessageDelegate owns `message_`. Callbacks won't be
  // called after the current object is destroyed.
  // It's safe to give a raw pointer to WebContents to the `callback` because
  // WebContents transitively owns the MessageWrapper so the `message_` can't
  // outlive `web_contents`.
  base::OnceClosure callback = base::BindOnce(
      &PasswordManagerErrorMessageDelegate::HandleSignInButtonClicked,
      base::Unretained(this), web_contents);

  message_ = std::make_unique<messages::MessageWrapper>(
      message_id, std::move(callback),
      base::BindOnce(
          &PasswordManagerErrorMessageDelegate::HandleMessageDismissed,
          base::Unretained(this)));

  int title_message_id = save_password ? IDS_SIGN_IN_TO_SAVE_PASSWORDS
                                       : IDS_SIGN_IN_TO_USE_PASSWORDS;
  message_->SetTitle(l10n_util::GetStringUTF16(title_message_id));

  std::u16string description =
      l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_DESCRIPTION);
  message_->SetDescription(description);

  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_PASSWORD_ERROR_SIGN_IN_BUTTON_TITLE));

  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDORID_MESSAGE_PASSWORD_MANAGER_ERROR));
  message_->DisableIconTint();
}

void PasswordManagerErrorMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  RecordDismissalReasonMetrics(dismiss_reason);
  message_.reset();
}

void PasswordManagerErrorMessageDelegate::HandleSignInButtonClicked(
    content::WebContents* web_contents) {
  sign_in_bridge_->startUpdateAccountCredentialsFlow(
      base::android::AttachCurrentThread(), web_contents);
  DismissPasswordManagerErrorMessage(messages::DismissReason::PRIMARY_ACTION);
}

void PasswordManagerErrorMessageDelegate::RecordDismissalReasonMetrics(
    messages::DismissReason dismiss_reason) {
  base::UmaHistogramEnumeration("PasswordManager.ErrorMessageDismissalReason",
                                dismiss_reason, messages::DismissReason::COUNT);
}
