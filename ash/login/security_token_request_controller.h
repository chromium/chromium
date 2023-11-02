// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_SECURITY_TOKEN_REQUEST_CONTROLLER_H_
#define ASH_LOGIN_SECURITY_TOKEN_REQUEST_CONTROLLER_H_

#include "ash/login/ui/pin_request_view.h"
#include "ash/public/cpp/login_types.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// SecurityTokenRequestController serves as a single point of access to ask the
// user for a PIN for a security token request.
class ASH_EXPORT SecurityTokenRequestController
    : public PinRequestView::Delegate {
 public:
  SecurityTokenRequestController();
  SecurityTokenRequestController(const SecurityTokenRequestController&) =
      delete;
  SecurityTokenRequestController& operator=(
      const SecurityTokenRequestController&) = delete;
  ~SecurityTokenRequestController() override;

  bool request_canceled() const { return request_canceled_; }
  void ResetRequestCanceled();

  // PinRequestView::Delegate interface.
  PinRequestView::SubmissionResult OnPinSubmitted(
      const std::string& pin) override;
  void OnBack() override;
  void OnHelp() override;

  // Shows the PIN dialog configured by |request|. If there already is a
  // SecurityTokenPinRequest in progress, keeps the dialog open and updates the
  // dialog's state.
  // Returns true if the dialog was opened or updated successfully, false
  // otherwise. The request will fail if the PIN UI is currently in use for
  // something other than a SecurityTokenPinRequest.
  bool SetPinUiState(SecurityTokenPinRequest request);

  // Closes the UI and resets callbacks.
  void ClosePinUi();

 private:
  // Called when the user submits the input. Will not be called if the UI is
  // closed before that happens.
  SecurityTokenPinRequest::OnPinEntered on_pin_submitted_;

  // Called when the PIN request UI gets closed by the user (back button).
  SecurityTokenPinRequest::OnUiClosed on_canceled_by_user_;

  // Whether this controller is currently using PinRequestWidget.
  bool security_token_request_in_progress_ = false;

  // Whether the user has recently canceled a PIN request.
  bool request_canceled_ = false;

  base::WeakPtrFactory<SecurityTokenRequestController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_SECURITY_TOKEN_REQUEST_CONTROLLER_H_
