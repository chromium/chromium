// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

GeneratedPasswordSavedMessageDelegate::GeneratedPasswordSavedMessageDelegate() =
    default;

GeneratedPasswordSavedMessageDelegate::
    ~GeneratedPasswordSavedMessageDelegate() {
  DismissPromptInternal();
}

void GeneratedPasswordSavedMessageDelegate::DismissPromptInternal() {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, messages::DismissReason::UNKNOWN);
  }
}

void GeneratedPasswordSavedMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  web_contents_ = nullptr;
  message_.reset();
}

void GeneratedPasswordSavedMessageDelegate::ShowPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {
  web_contents_ = web_contents;
  message_ = std::make_unique<messages::MessageWrapper>(
      base::OnceCallback<void()>(),
      base::BindOnce(
          &GeneratedPasswordSavedMessageDelegate::HandleDismissCallback,
          base::Unretained(this)));

  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE));

  std::u16string description;
  const password_manager::PasswordForm& pending_credentials =
      saved_form->GetPendingCredentials();
  const std::u16string masked_password =
      std::u16string(pending_credentials.password_value.size(), L'•');
  description.append(GetDisplayUsername(pending_credentials))
      .append(u" ")
      .append(masked_password);

  message_->SetDescription(description);
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_SAVE_PASSWORD));
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION);
}