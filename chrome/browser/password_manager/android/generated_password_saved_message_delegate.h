// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "components/messages/android/message_wrapper.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"

namespace content {
class WebContents;
}  // namespace content

class AddUsernameDialogBridge;

// This class provides simple API to show message UI prompt when the suggested
// password is saved.
class GeneratedPasswordSavedMessageDelegate {
 public:
  using CreateAddUsernameDialogBridge =
      base::RepeatingCallback<std::unique_ptr<AddUsernameDialogBridge>()>;

  GeneratedPasswordSavedMessageDelegate();
  GeneratedPasswordSavedMessageDelegate(
      base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>,
      CreateAddUsernameDialogBridge add_username_dialog_factory);
  ~GeneratedPasswordSavedMessageDelegate();
  GeneratedPasswordSavedMessageDelegate(
      const GeneratedPasswordSavedMessageDelegate&) = delete;
  GeneratedPasswordSavedMessageDelegate& operator=(
      const GeneratedPasswordSavedMessageDelegate&) = delete;

  void ShowPrompt(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form);

 private:
  friend class GeneratedPasswordSavedMessageDelegateTest;

  void ShowPasswordSavedMessage(content::WebContents* web_contents);
  void ShowAddUsernameDialog(content::WebContents* web_contents);

  void HandleDismissCallback(messages::DismissReason dismiss_reason);
  void DismissPromptInternal();
  void HandleUsernameAddedCallback(const std::u16string& username);

  std::unique_ptr<password_manager::PasswordFormManagerForUI> saved_form_;
  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<AddUsernameDialogBridge> add_username_dialog_bridge_;
  CreateAddUsernameDialogBridge add_username_dialog_factory_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GENERATED_PASSWORD_SAVED_MESSAGE_DELEGATE_H_
