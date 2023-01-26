// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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
// display a prompt to save and update password through Messages API. The class
// is responsible for populating message properties, managing message's
// lifetime, saving password form in response to user interactions and recording
// metrics.
class SaveUpdatePasswordMessageDelegate {
 public:
  // When Chrome detects an unknown password being entered into a web page, it
  // shows the message asking if user wants to save or update (if there is
  // already some other password saved for the site) the password.
  // This enum is used to record the user decision regarding the save/update UI.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SaveUpdatePasswordMessageDismissReason {
    kAccept = 0,          // Click save/update/continue in the message
    kAcceptInDialog = 1,  // Save (or update) in dialog (if the dialog is
                          // an optional part of the workflow)
    // Clicked Accept in Username confirmation dialog.
    // This bucket is different from kAcceptInDialog in order to differentiate
    // between acceptance in the confirmation dialog, which is a required
    // part of the flow, and the save/update dialogs which are optional.
    kAcceptInUsernameConfirmDialog = 2,
    kCancel = 3,          // Dismiss the message
    kCancelInDialog = 4,  // Cancel clicked in dialog (or dialog dismissed)
    kNeverSave = 5,       // Click 'Never save for this site'
    kMaxValue = kNeverSave,
  };
  using PasswordEditDialogFactory =
      base::RepeatingCallback<std::unique_ptr<PasswordEditDialog>(
          content::WebContents*,
          PasswordEditDialog::DialogAcceptedCallback,
          PasswordEditDialog::LegacyDialogAcceptedCallback,
          PasswordEditDialog::DialogDismissedCallback)>;

  SaveUpdatePasswordMessageDelegate();
  ~SaveUpdatePasswordMessageDelegate();

  // Displays a "Save password" message for current |web_contents| and
  // |form_to_save|.
  void DisplaySaveUpdatePasswordPrompt(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool update_password);

  // Dismisses currently displayed message or dialog. Because the implementation
  // uses some of the dependencies (e.g. log manager) this method needs to be
  // called before the object is destroyed.
  void DismissSaveUpdatePasswordPrompt();

 private:
  friend class SaveUpdatePasswordMessageDelegateTest;
  enum class SavePasswordDialogMenuItem { kNeverSave = 0, kEditPassword = 1 };

  SaveUpdatePasswordMessageDelegate(
      PasswordEditDialogFactory password_edit_dialog_factory);

  void DismissSaveUpdatePasswordMessage(messages::DismissReason dismiss_reason);

  void DisplaySaveUpdatePasswordPromptInternal(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      absl::optional<AccountInfo> account_info,
      bool update_password);
  void CreateMessage(bool update_password);
  void SetupCogMenu(std::unique_ptr<messages::MessageWrapper>& message,
                    bool update_password);
  void SetupCogMenuForDialogWithDetails(
      std::unique_ptr<messages::MessageWrapper>& message,
      bool update_password);
  void HandleSaveMessageMenuItemClick(int item_id);

  // Returns the message description depending on whether the password is being
  // saved or updated and if unified password manager is enabled.
  std::u16string GetMessageDescription(
      const password_manager::PasswordForm& pending_credentials,
      bool update_password,
      bool unified_password_manager);

  std::u16string GetUnifiedPasswordManagerMessageDescription(
      bool update_password);
  std::u16string GetExploratoryStringsMessageDescription(bool update_password);

  // Returns string id for the message primary button. Takes into account
  // whether this is save or update password scenario and whether the update
  // message will be followed by a username confirmation dialog.
  int GetPrimaryButtonTextId(bool update_password,
                             bool use_followup_button_text);

  // Populates |usernames| with the list of usernames from best saved matches to
  // be presented to the user in a dropdown.
  // Returns the vector index of the currently pending username in
  // the form manager.
  unsigned int GetDisplayUsernames(std::vector<std::u16string>* usernames);

  // Following methods handle events associated with user interaction with UI.
  void HandleSaveButtonClicked();
  void HandleNeverSaveClicked();
  void HandleUpdateButtonClicked();
  void DisplayEditDialog(bool update_password);
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
  bool HasMultipleCredentialsStored();
  void CreatePasswordEditDialog();
  void HandleDialogDismissed(bool dialogAccepted);
  void HandleSavePasswordFromDialog(const std::u16string& username,
                                    const std::u16string& password);
  void HandleSavePasswordFromLegacyDialog(int username_index);

  void ClearState();

  void RecordMessageShownMetrics();
  void RecordDismissalReasonMetrics(
      password_manager::metrics_util::UIDismissalReason ui_dismissal_reason);

  void RecordSaveUpdateUIDismissalReason(
      SaveUpdatePasswordMessageDismissReason dismiss_reason);

  SaveUpdatePasswordMessageDismissReason GetPasswordEditDialogDismissReason(
      bool accepted);

  SaveUpdatePasswordMessageDismissReason
  GetSaveUpdatePasswordMessageDismissReason(
      messages::DismissReason dismiss_reason);

  static password_manager::metrics_util::UIDismissalReason
  MessageDismissReasonToPasswordManagerUIDismissalReason(
      messages::DismissReason dismiss_reason);

  PasswordEditDialogFactory password_edit_dialog_factory_;

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Can be the empty string, the account email, or the account full name.
  std::string account_email_;
  bool update_password_ = false;

  // ManagePasswordsState maintains the password form that is being
  // saved/updated. It provides helper functions for populating username list.
  ManagePasswordsState passwords_state_;

  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<PasswordEditDialog> password_edit_dialog_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_
