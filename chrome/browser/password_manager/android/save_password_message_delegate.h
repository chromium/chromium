// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}  // namespace content

// This class provides simplified interface for ChromePasswordManagerClient to
// display a prompt to save password through Messages API. The class is
// responsible for populating message properties, managing message's lifetime,
// saving password form in response to user interactions and recording metrics.
class SavePasswordMessageDelegate {
 public:
  using PasswordEditDialogFactory =
      base::RepeatingCallback<std::unique_ptr<PasswordEditDialog>(
          content::WebContents*,
          PasswordEditDialog::DialogAcceptedCallback,
          PasswordEditDialog::DialogDismissedCallback)>;

  SavePasswordMessageDelegate();
  ~SavePasswordMessageDelegate();

  // Displays a "Save password" message for current |web_contents| and
  // |form_to_save|.
  void DisplaySavePasswordPrompt(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool update_password);

  // Dismisses currently displayed message or dialog. Because the implementation
  // uses some of the dependencies (e.g. log manager) this method needs to be
  // called before the object is destroyed.
  void DismissSavePasswordPrompt();

 private:
  friend class SavePasswordMessageDelegateTest;

  SavePasswordMessageDelegate(
      PasswordEditDialogFactory password_edit_dialog_factory);

  void DismissSavePasswordMessage(messages::DismissReason dismiss_reason);

  void DisplaySavePasswordPromptInternal(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      absl::optional<AccountInfo> account_info,
      bool update_password);
  void CreateMessage(bool update_password);

  // Populates |usernames| with the list of usernames from best saved matches to
  // be presented to the user in a dropdown. Returns the index of the username
  // that matches the one from pending credentials.
  unsigned int GetDisplayUsernames(std::vector<std::u16string>* usernames);

  // Following methods handle events associated with user interaction with UI.
  void HandleSaveButtonClicked();
  void HandleNeverSaveClicked();
  void HandleDisplayEditDialog();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
  void HandleSavePasswordFromDialog(int selected_username);
  void HandleDialogDismissed(bool dialogAccepted);

  void ClearState();

  void RecordMessageShownMetrics();
  void RecordDismissalReasonMetrics(
      password_manager::metrics_util::UIDismissalReason ui_dismissal_reason);

  static password_manager::metrics_util::UIDismissalReason
  MessageDismissReasonToPasswordManagerUIDismissalReason(
      messages::DismissReason dismiss_reason);

  PasswordEditDialogFactory password_edit_dialog_factory_;

  content::WebContents* web_contents_ = nullptr;
  std::string account_email_;

  // ManagePasswordsState maintains the password form that is being
  // saved/updated. It provides helper functions for populating username list.
  ManagePasswordsState passwords_state_;

  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<PasswordEditDialog> password_edit_dialog_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_PASSWORD_MESSAGE_DELEGATE_H_
