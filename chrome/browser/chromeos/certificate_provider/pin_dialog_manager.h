// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_PIN_DIALOG_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_PIN_DIALOG_MANAGER_H_

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host_popup_impl.h"
#include "chromeos/constants/security_token_pin_types.h"
#include "components/account_id/account_id.h"

namespace chromeos {

// Manages the state of the dialog that requests the PIN from user. Used by the
// extensions that need to request the PIN. Implemented as requirement for
// crbug.com/612886
class PinDialogManager final {
 public:
  enum class RequestPinResult {
    kSuccess,
    kInvalidId,
    kOtherFlowInProgress,
    kDialogDisplayedAlready,
  };

  enum class StopPinRequestResult {
    kSuccess,
    kNoActiveDialog,
    kNoUserInput,
  };

  using RequestPinCallback =
      base::OnceCallback<void(const std::string& user_input)>;
  using StopPinRequestCallback = base::OnceClosure;

  PinDialogManager();
  PinDialogManager(const PinDialogManager&) = delete;
  PinDialogManager& operator=(const PinDialogManager&) = delete;
  ~PinDialogManager();

  // Stores internally the |signRequestId| along with current timestamp.
  void AddSignRequestId(
      const std::string& extension_id,
      int sign_request_id,
      const base::Optional<AccountId>& authenticating_user_account_id);

  // Removes the specified sign request, aborting both the current and the
  // future PIN dialogs related to it.
  void AbortSignRequest(const std::string& extension_id, int sign_request_id);

  // Creates and displays a new PIN dialog, or reuses the old dialog with just
  // updating the parameters if active one exists.
  // |extension_id| - the ID of the extension requesting the dialog.
  // |extension_name| - the name of the extension requesting the dialog.
  // |sign_request_id| - the ID given by Chrome when the extension was asked to
  //     sign the data. It should be a valid, not expired ID at the time the
  //     extension is requesting PIN the first time.
  // |code_type| - the type of input requested: either "PIN" or "PUK".
  // |error_label| - the error template to be displayed inside the dialog. If
  //     |kNone|, no error is displayed.
  // |attempts_left| - the number of attempts the user has to try the code. It
  //     is informational only, and enforced on Chrome side only in case it's
  //     zero. In that case the textfield is disabled and the user can't provide
  //     any input to extension. If -1 the textfield from the dialog is enabled
  //     but no information about the attepts left is not given to the user.
  // |callback| - used to notify about the user input in the text_field from the
  //     dialog.
  // Returns |kSuccess| if the dialog is displayed and extension owns it.
  // Otherwise the specific error is returned.
  RequestPinResult RequestPin(const std::string& extension_id,
                              const std::string& extension_name,
                              int sign_request_id,
                              SecurityTokenPinCodeType code_type,
                              SecurityTokenPinErrorLabel error_label,
                              int attempts_left,
                              RequestPinCallback callback);

  // Updates the existing dialog with the error message. Returns whether the
  // provided |extension_id| matches the extension owning the active dialog.
  // When it is, the |callback| will be executed once the UI is completed (e.g.,
  // the dialog with the error message is closed by the user).
  StopPinRequestResult StopPinRequestWithError(
      const std::string& extension_id,
      SecurityTokenPinErrorLabel error_label,
      StopPinRequestCallback callback);

  // Returns whether the last PIN dialog from this extension was closed by the
  // user.
  bool LastPinDialogClosed(const std::string& extension_id) const;

  // Called when extension calls the stopPinRequest method. The active dialog is
  // closed if the |extension_id| matches the |active_dialog_extension_id_|.
  // Returns whether the dialog was closed.
  bool CloseDialog(const std::string& extension_id);

  // Resets the manager data related to the extension.
  void ExtensionUnloaded(const std::string& extension_id);

  // Dynamically adds the dialog host that can be used by this instance for
  // showing new dialogs. There may be multiple hosts added, in which case the
  // most recently added is used. Before any hosts have been added, the default
  // (popup-based) host is used.
  void AddPinDialogHost(SecurityTokenPinDialogHost* pin_dialog_host);
  // Removes the previously added dialog host. If a dialog is still opened in
  // this host, closes it beforehand.
  void RemovePinDialogHost(SecurityTokenPinDialogHost* pin_dialog_host);

  SecurityTokenPinDialogHostPopupImpl* default_dialog_host_for_testing() {
    return &default_dialog_host_;
  }

 private:
  struct SignRequestState {
    SignRequestState(
        base::Time begin_time,
        const base::Optional<AccountId>& authenticating_user_account_id);
    SignRequestState(const SignRequestState&);
    SignRequestState& operator=(const SignRequestState&);
    ~SignRequestState();

    base::Time begin_time;
    base::Optional<AccountId> authenticating_user_account_id;
  };

  // Holds information related to the currently opened PIN dialog.
  struct ActiveDialogState {
    ActiveDialogState(SecurityTokenPinDialogHost* host,
                      const std::string& extension_id,
                      const std::string& extension_name,
                      int sign_request_id,
                      SecurityTokenPinCodeType code_type);
    ~ActiveDialogState();

    // Remember the host that was used to open the active dialog, as new hosts
    // could have been added since the dialog was opened, but we want to
    // continue calling the same host when dealing with the same active dialog.
    SecurityTokenPinDialogHost* const host;

    const std::string extension_id;
    const std::string extension_name;
    const int sign_request_id;
    const SecurityTokenPinCodeType code_type;
    RequestPinCallback request_pin_callback;
    StopPinRequestCallback stop_pin_request_callback;
  };

  using ExtensionNameRequestIdPair = std::pair<std::string, int>;

  // Returns the sign request state for the given key, or null if not found.
  SignRequestState* FindSignRequestState(const std::string& extension_id,
                                         int sign_request_id);

  // The callback that gets invoked once the user sends some input into the PIN
  // dialog.
  void OnPinEntered(const std::string& user_input);
  // The callback that gets invoked once the PIN dialog gets closed.
  void OnPinDialogClosed();

  // Returns the dialog host that should own the new dialog. Currently returns
  // the most recently added dialog host (falling back to the default one when
  // no host has been added).
  SecurityTokenPinDialogHost* GetHostForNewDialog();

  // Closes the active dialog, if there's any, and runs the necessary callbacks.
  void CloseActiveDialog();

  // Tells whether user closed the last request PIN dialog issued by an
  // extension. The extension_id is the key and value is true if user closed the
  // dialog. Used to determine if the limit of dialogs rejected by the user has
  // been exceeded.
  std::unordered_map<std::string, bool> last_response_closed_;

  // The map from extension_id and an active sign request id to the state of the
  // request.
  std::map<ExtensionNameRequestIdPair, SignRequestState> sign_requests_;

  SecurityTokenPinDialogHostPopupImpl default_dialog_host_;
  // The list of dynamically added dialog hosts, in the same order as they were
  // added.
  std::vector<SecurityTokenPinDialogHost*> added_dialog_hosts_;

  // There can be only one active dialog to request the PIN at any point of
  // time.
  base::Optional<ActiveDialogState> active_dialog_state_;

  base::WeakPtrFactory<PinDialogManager> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_PIN_DIALOG_MANAGER_H_
