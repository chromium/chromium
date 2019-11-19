// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host_popup_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/ui/request_pin_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

namespace {

gfx::NativeWindow GetBrowserParentWindow() {
  if (LoginDisplayHost::default_host())
    return LoginDisplayHost::default_host()->GetNativeWindow();

  Browser* browser =
      chrome::FindTabbedBrowser(ProfileManager::GetPrimaryUserProfile(), true);
  if (browser)
    return browser->window()->GetNativeWindow();

  return nullptr;
}

}  // namespace

SecurityTokenPinDialogHostPopupImpl::SecurityTokenPinDialogHostPopupImpl() =
    default;

SecurityTokenPinDialogHostPopupImpl::~SecurityTokenPinDialogHostPopupImpl() =
    default;

void SecurityTokenPinDialogHostPopupImpl::ShowSecurityTokenPinDialog(
    const std::string& caller_extension_name,
    SecurityTokenPinCodeType code_type,
    bool enable_user_input,
    SecurityTokenPinErrorLabel error_label,
    int attempts_left,
    const base::Optional<AccountId>& /*authenticating_user_account_id*/,
    SecurityTokenPinEnteredCallback pin_entered_callback,
    SecurityTokenPinDialogClosedCallback pin_dialog_closed_callback) {
  DCHECK(!caller_extension_name.empty());
  DCHECK(!enable_user_input || attempts_left);
  DCHECK_GE(attempts_left, -1);

  pin_entered_callback_ = std::move(pin_entered_callback);
  pin_dialog_closed_callback_ = std::move(pin_dialog_closed_callback);

  if (active_pin_dialog_) {
    active_pin_dialog_->SetDialogParameters(code_type, error_label,
                                            attempts_left, enable_user_input);
    active_pin_dialog_->SetExtensionName(caller_extension_name);
    active_pin_dialog_->DialogModelChanged();
  } else {
    active_pin_dialog_ = new RequestPinView(
        caller_extension_name, code_type, attempts_left,
        base::BindRepeating(&SecurityTokenPinDialogHostPopupImpl::OnPinEntered,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SecurityTokenPinDialogHostPopupImpl::OnViewDestroyed,
                       weak_ptr_factory_.GetWeakPtr()));
    // If there is no parent, falls back to the root window for new windows.
    active_window_ = views::DialogDelegate::CreateDialogWidget(
        active_pin_dialog_, /*context=*/nullptr, GetBrowserParentWindow());
    active_window_->Show();
  }
}

void SecurityTokenPinDialogHostPopupImpl::CloseSecurityTokenPinDialog() {
  if (!active_pin_dialog_)
    return;
  active_window_->Close();
  // The view destruction may happen asynchronously, so clear our state and
  // execute the callback immediately in order to follow our own API contract.
  active_pin_dialog_ = nullptr;
  active_window_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (pin_dialog_closed_callback_)
    std::move(pin_dialog_closed_callback_).Run();
}

void SecurityTokenPinDialogHostPopupImpl::OnPinEntered(
    const std::string& user_input) {
  DCHECK(active_pin_dialog_);
  DCHECK(active_window_);
  std::move(pin_entered_callback_).Run(user_input);
}

void SecurityTokenPinDialogHostPopupImpl::OnViewDestroyed() {
  DCHECK(active_pin_dialog_);
  DCHECK(active_window_);

  active_pin_dialog_ = nullptr;
  active_window_ = nullptr;
  if (pin_dialog_closed_callback_)
    std::move(pin_dialog_closed_callback_).Run();
}

}  // namespace chromeos
