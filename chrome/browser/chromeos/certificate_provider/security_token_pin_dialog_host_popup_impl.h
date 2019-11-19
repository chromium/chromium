// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_POPUP_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_POPUP_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host.h"

namespace views {
class Widget;
}

namespace chromeos {

class RequestPinView;

// The default implementation of the PIN dialog host. It renders the PIN dialog
// as a popup with the RequestPinView view.
class SecurityTokenPinDialogHostPopupImpl final
    : public SecurityTokenPinDialogHost {
 public:
  SecurityTokenPinDialogHostPopupImpl();
  SecurityTokenPinDialogHostPopupImpl(
      const SecurityTokenPinDialogHostPopupImpl&) = delete;
  SecurityTokenPinDialogHostPopupImpl& operator=(
      const SecurityTokenPinDialogHostPopupImpl&) = delete;
  ~SecurityTokenPinDialogHostPopupImpl() override;

  // SecurityTokenPinDialogHost:
  void ShowSecurityTokenPinDialog(
      const std::string& caller_extension_name,
      SecurityTokenPinCodeType code_type,
      bool enable_user_input,
      SecurityTokenPinErrorLabel error_label,
      int attempts_left,
      const base::Optional<AccountId>& authenticating_user_account_id,
      SecurityTokenPinEnteredCallback pin_entered_callback,
      SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) override;
  void CloseSecurityTokenPinDialog() override;

  RequestPinView* active_view_for_testing() { return active_pin_dialog_; }
  views::Widget* active_window_for_testing() { return active_window_; }

 private:
  // Called every time the user submits some input.
  void OnPinEntered(const std::string& user_input);
  // Called when the |active_pin_dialog_| view is being destroyed.
  void OnViewDestroyed();

  SecurityTokenPinEnteredCallback pin_entered_callback_;
  SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback_;

  // Owned by |active_window_|.
  RequestPinView* active_pin_dialog_ = nullptr;
  // Owned by the UI code (NativeWidget).
  views::Widget* active_window_ = nullptr;

  base::WeakPtrFactory<SecurityTokenPinDialogHostPopupImpl> weak_ptr_factory_{
      this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SECURITY_TOKEN_PIN_DIALOG_HOST_POPUP_IMPL_H_
