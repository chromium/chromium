// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/password_manager/android/add_username_dialog/add_username_dialog_bridge.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

GeneratedPasswordSavedMessageDelegate::GeneratedPasswordSavedMessageDelegate()
    : add_username_dialog_factory_(base::BindRepeating(
          []() { return std::make_unique<AddUsernameDialogBridge>(); })) {}

GeneratedPasswordSavedMessageDelegate::GeneratedPasswordSavedMessageDelegate(
    base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>,
    CreateAddUsernameDialogBridge add_username_dialog_factory)
    : add_username_dialog_factory_(std::move(add_username_dialog_factory)) {}

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
  add_username_dialog_bridge_.reset();
  saved_form_.reset();
}

void GeneratedPasswordSavedMessageDelegate::HandleUsernameAddedCallback(
    const std::u16string& username) {
  saved_form_->OnUpdateUsernameFromPrompt(username);
  saved_form_->Save();
}

void GeneratedPasswordSavedMessageDelegate::ShowPrompt(
    content::WebContents* web_contents,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form) {
  saved_form_ = std::move(saved_form);
  const std::u16string& username =
      saved_form_->GetPendingCredentials().username_value;
  if (username.empty()) {
    ShowAddUsernameDialog(web_contents);
  } else {
    ShowPasswordSavedMessage(web_contents);
  }
}

void GeneratedPasswordSavedMessageDelegate::ShowAddUsernameDialog(
    content::WebContents* web_contents) {
  const std::u16string& password =
      saved_form_->GetPendingCredentials().password_value;

  add_username_dialog_bridge_ = add_username_dialog_factory_.Run();
  // The delegate owns the bridge, so binding callbacks unretained is fine here.
  add_username_dialog_bridge_->ShowAddUsernameDialog(
      web_contents->GetTopLevelNativeWindow(), password,
      base::BindOnce(
          &GeneratedPasswordSavedMessageDelegate::HandleUsernameAddedCallback,
          base::Unretained(this)),
      base::BindOnce(
          &GeneratedPasswordSavedMessageDelegate::HandleDismissCallback,
          base::Unretained(this), messages::DismissReason::UNKNOWN));
}

void GeneratedPasswordSavedMessageDelegate::ShowPasswordSavedMessage(
    content::WebContents* web_contents) {
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
