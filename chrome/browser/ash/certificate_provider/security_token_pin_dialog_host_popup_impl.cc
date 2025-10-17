// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/certificate_provider/security_token_pin_dialog_host_popup_impl.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/views/notifications/request_pin_view_chromeos.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

namespace {

gfx::NativeWindow GetBrowserParentWindow() {
  if (ash::LoginDisplayHost::default_host())
    return ash::LoginDisplayHost::default_host()->GetNativeWindow();

  // TODO(crbug.com/450116701): Avoid profile (another chrome/browser
  // dependency) altogether and use SessionManager::GetPrimarySession()
  // and Session::account_id().
  AccountId primary_user_profile_account_id =
      CHECK_DEREF(ash::AnnotatedAccountId::Get(
          ProfileManager::GetPrimaryUserProfile()->GetOriginalProfile()));

  gfx::NativeWindow window = nullptr;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [&](ash::BrowserDelegate& browser) {
        if (browser.GetAccountId() != primary_user_profile_account_id) {
          return ash::BrowserController::kContinueIteration;
        }
        if (browser.GetType() != ash::BrowserType::kNormal) {
          return ash::BrowserController::kContinueIteration;
        }
        if (aura::Window* native_window = browser.GetNativeWindow();
            !chromeos::DesksHelper::Get(native_window)
                 ->BelongsToActiveDesk(native_window)) {
          return ash::BrowserController::kContinueIteration;
        }
        if (browser.IsAttemptingToClose() || browser.IsClosing()) {
          return ash::BrowserController::kContinueIteration;
        }

        window = browser.GetNativeWindow();
        return ash::BrowserController::kBreakIteration;
      });
  return window;
}

}  // namespace

SecurityTokenPinDialogHostPopupImpl::SecurityTokenPinDialogHostPopupImpl() =
    default;

SecurityTokenPinDialogHostPopupImpl::~SecurityTokenPinDialogHostPopupImpl() =
    default;

void SecurityTokenPinDialogHostPopupImpl::ShowSecurityTokenPinDialog(
    const std::string& caller_extension_name,
    security_token_pin::CodeType code_type,
    bool enable_user_input,
    security_token_pin::ErrorLabel error_label,
    int attempts_left,
    const std::optional<AccountId>& /*authenticating_user_account_id*/,
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
