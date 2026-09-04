// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/password_edit_dialog/android/password_edit_dialog_bridge.h"
#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

// This class provides simplified interface for ChromePasswordManagerClient to
// display a prompt to save and update password through Messages API. The class
// is responsible for populating message properties, managing message's
// lifetime, saving password form in response to user interactions and recording
// metrics.
class SaveUpdatePasswordMessageDelegate
    : public PasswordEditDialogBridgeDelegate,
      public password_manager::PasswordStoreInterface::Observer {
 public:
  using PasswordEditDialogFactory =
      base::RepeatingCallback<std::unique_ptr<PasswordEditDialog>(
          content::WebContents*,
          PasswordEditDialogBridgeDelegate*)>;

  SaveUpdatePasswordMessageDelegate();
  ~SaveUpdatePasswordMessageDelegate() override;

  // Test-only constructor. Allows test class to set device_lock_bridge_ and
  // password_manager_error_message_helper_bridge_.
  SaveUpdatePasswordMessageDelegate(
      base::PassKey<class SaveUpdatePasswordMessageDelegateTest>,
      PasswordEditDialogFactory password_edit_dialog_factory,
      std::unique_ptr<DeviceLockBridge> device_lock_bridge,
      std::unique_ptr<PasswordManagerErrorMessageHelperBridge>
          password_manager_error_message_helper_bridge);

  // Displays a "Save password" message for current |web_contents| and
  // |form_to_save|.
  void DisplaySaveUpdatePasswordPrompt(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool update_password,
      password_manager::PasswordManagerClient* password_manager_client);

  // Dismisses currently displayed message or dialog. Because the implementation
  // uses some of the dependencies (e.g. log manager) this method needs to be
  // called before the object is destroyed.
  void DismissAllActiveUI();

  // Implementation of PasswordEditDialogBridgeDelegate interface.
  void HandleDialogDismissed(bool dialogAccepted) override;
  void HandleSavePasswordFromDialog(const std::u16string& username,
                                    const std::u16string& password) override;
  bool IsUsingAccountStorage(const std::u16string& username) override;

  // password_manager::PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::StoredCredential>&
                            retained_credentials) override;
  void OnErrorStateChanged(
      password_manager::PasswordStoreInterface* store,
      password_manager::ActionableError changed_error) override;

 private:
  friend class SaveUpdatePasswordMessageDelegateTest;
  enum class SavePasswordDialogMenuItem { kNeverSave = 0, kEditPassword = 1 };

  // Lifecycle states of the password save/update delegate.
  enum class State {
    // No active message prompt, dialog, or background operation. Delegate is
    // inactive and ready to display a new prompt.
    kIdle,

    // The main "Save password" or "Update password" message prompt is currently
    // displayed or queued in the Messages UI.
    kSaveUpdatePromptShowing,

    // The password edit dialog is currently open, allowing the user to view or
    // edit the username and password before saving.
    kEditDialogShowing,

    // The user initiated a save/update, and the system Device Lock UI (PIN,
    // pattern, or biometrics) is currently shown to confirm identity.
    kWaitingForDeviceLock,

    // Password saving is blocked by an unrecovered trusted vault key. The
    // delegate is observing the password store while waiting for the trusted
    // vault key retrieval flow to finish.
    kWaitingForTrustedVault,

    // The trusted vault key retrieval flow completed but the vault remains
    // locked. The "Save password" message prompt is shown again to allow the
    // user to retry, while the delegate continues observing the password store
    // for background error resolution.
    kRepromptShowing,

    // The password was successfully saved after resolving a trusted vault key
    // error, and a temporary confirmation message is currently displayed.
    kConfirmationShowing,

    // The flow has completed or was dismissed, and the delegate is waiting for
    // any remaining asynchronous UI dismissal callbacks to arrive before
    // resetting to kIdle.
    kDismissing,
  };

  explicit SaveUpdatePasswordMessageDelegate(
      PasswordEditDialogFactory password_edit_dialog_factory);

  void DismissSaveUpdatePasswordMessage(messages::DismissReason dismiss_reason);

  void DisplaySaveUpdatePasswordPromptInternal(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      std::optional<AccountInfo> account_info,
      bool update_password,
      password_manager::PasswordManagerClient* password_manager_client);
  void CreateMessage(bool update_password);
  void SetupCogMenu(std::unique_ptr<messages::MessageWrapper>& message,
                    bool update_password);
  void HandleSaveMessageMenuItemClick(int item_id);

  // Returns the message title depending on whether the password is being saved
  // or updated.
  std::u16string GetMessageTitle(bool update_password,
                                 bool is_federated_credential);

  // Returns the message description depending on whether the password is being
  // saved or updated and whether the trusted vault is locked.
  std::u16string GetMessageDescription(
      const password_manager::PasswordForm& pending_credentials,
      bool update_password,
      bool is_saving_blocked_by_trusted_vault_error);

  // Gets account name or email that should be displayed in the description
  // messages. Returns a nullopt if account info should not be displayed.
  std::optional<std::string> GetAccountForMessageDescription(
      const std::optional<AccountInfo>& account_info);

  // Returns string for the message primary button. Takes into account
  // whether this is save or update password scenario and whether the update
  // message will be followed by a username confirmation dialog.
  std::u16string GetPrimaryButtonText(
      bool update_password,
      bool use_followup_button_text,
      bool is_saving_blocked_by_trusted_vault_error);

  // Populates |usernames| with the list of usernames from best saved matches to
  // be presented to the user in a dropdown.
  // Returns the vector index of the currently pending username in
  // the form manager.
  unsigned int GetDisplayUsernames(std::vector<std::u16string>* usernames);

  // Following methods handle events associated with user interaction with UI.
  void HandleSaveButtonClicked();
  void StartSavePasswordFlow();
  void SolveTrustedVaultCheck(bool is_device_lock_requirement_met);
  void OnTrustedVaultRecoveryDone();
  void SaveAfterTrustedVaultResolution();
  void SaveFormManager(bool show_confirmation_message);
  void HandleNeverSaveClicked();
  void HandleUpdateButtonClicked();
  void DisplayEditDialog(bool update_password);
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
  bool HasMultipleCredentialsStored();
  void CreatePasswordEditDialog();

  // Shows a confirmation message after the password has been saved after
  // error resolution.
  void ShowConfirmationMessage();
  void HandleConfirmationMessageDismissed(
      messages::DismissReason dismiss_reason);

  void TransitionTo(State new_state);
  void MaybeCleanUpState();
  void ClearState();

  // Returns true if password saving (not updating) is blocked by a trusted
  // vault error. Ensures the client is non-null before checking error state.
  bool IsSavingBlockedByTrustedVaultError() const;

  void RecordMessageShownMetrics(bool update_password);
  void RecordDismissalReasonMetrics(
      password_manager::metrics_util::UIDismissalReason ui_dismissal_reason);

  static password_manager::metrics_util::UIDismissalReason
  MessageDismissReasonToPasswordManagerUIDismissalReason(
      messages::DismissReason dismiss_reason);

  PasswordEditDialogFactory password_edit_dialog_factory_;

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Can be a nullopt, the account email, or the account full name.
  std::optional<std::string> account_email_;
  bool update_password_ = false;

  State state_ = State::kIdle;

  // ManagePasswordsState maintains the password form that is being
  // saved/updated. It provides helper functions for populating username list.
  ManagePasswordsState passwords_state_;

  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<messages::MessageWrapper> confirmation_message_;
  std::unique_ptr<PasswordEditDialog> password_edit_dialog_;

  std::unique_ptr<DeviceLockBridge> device_lock_bridge_;

  std::unique_ptr<PasswordManagerErrorMessageHelperBridge>
      password_manager_error_message_helper_bridge_;

  base::ScopedObservation<password_manager::PasswordStoreInterface,
                          password_manager::PasswordStoreInterface::Observer>
      account_password_store_observation_{this};

  base::WeakPtrFactory<SaveUpdatePasswordMessageDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_SAVE_UPDATE_PASSWORD_MESSAGE_DELEGATE_H_
