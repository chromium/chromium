// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/common/password_manager_features.h"
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
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void GeneratedPasswordSavedMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  message_.reset();
}

void GeneratedPasswordSavedMessageDelegate::ShowPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::GENERATED_PASSWORD_SAVED,
      base::OnceCallback<void()>(),
      base::BindOnce(
          &GeneratedPasswordSavedMessageDelegate::HandleDismissCallback,
          base::Unretained(this)));

  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE));

  std::u16string description;
  description = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_GENERATED_PASSWORD_SAVED_MESSAGE_DESCRIPTION);

  message_->SetDescription(description);
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_PASSWORD_MANAGER_LOGO_24DP));
  message_->DisableIconTint();
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}
