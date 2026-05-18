// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/screen_switch_check_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Dialog that confirms the user wants to stop screen share/cast. Calls a
// callback with the result.
class CancelCastingDialog : public views::DialogDelegateView {
 public:
  explicit CancelCastingDialog(base::OnceCallback<void(bool)> callback)
      : callback_(std::move(callback)) {
    AddChildViewRaw(new views::MessageBoxView(
        l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_MESSAGE)));
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetTitle(l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_TITLE));
    SetShowCloseButton(false);
    SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(IDS_DESKTOP_CASTING_ACTIVE_CONTINUE));
    SetAcceptCallback(base::BindOnce(&CancelCastingDialog::OnDialogAccepted,
                                     base::Unretained(this)));
    SetCancelCallback(base::BindOnce(&CancelCastingDialog::OnDialogCancelled,
                                     base::Unretained(this)));
  }

  CancelCastingDialog(const CancelCastingDialog&) = delete;
  CancelCastingDialog& operator=(const CancelCastingDialog&) = delete;

  ~CancelCastingDialog() override = default;

  void OnDialogCancelled() { std::move(callback_).Run(false); }

  void OnDialogAccepted() {
    std::move(callback_).Run(true);
  }

 private:
  base::OnceCallback<void(bool)> callback_;
};

ScreenSwitchCheckController::ScreenSwitchCheckController() {
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenSecurityObserver(this);
}

ScreenSwitchCheckController::~ScreenSwitchCheckController() {
  Shell::Get()->system_tray_notifier()->RemoveScreenSecurityObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void ScreenSwitchCheckController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStop();
  Shell::Get()->system_tray_notifier()->NotifyRemotingScreenShareStop();
}

void ScreenSwitchCheckController::CanSwitchAwayFromActiveUser(
    base::OnceCallback<void(bool)> callback) {
  // If neither screen sharing nor capturing is going on we can immediately
  // switch users.
  if (!is_screen_accessed_ && !is_remoting_share_) {
    std::move(callback).Run(true);
    return;
  }

  if (skip_cancel_dialog_for_testing_) {
    std::move(callback).Run(true);
    return;
  }

  views::DialogDelegate::CreateDialogWidget(
      new CancelCastingDialog(std::move(callback)),
      Shell::GetPrimaryRootWindow(), nullptr)
      ->Show();
}

void ScreenSwitchCheckController::OnScreenAccessStart(
    base::OnceClosure stop_callback,
    const base::RepeatingClosure& source_callback,
    const std::u16string& access_app_name) {
  is_screen_accessed_ = true;
}

void ScreenSwitchCheckController::OnScreenAccessStop() {
  // Multiple screen capture sessions can exist, but they are stopped at once
  // for simplicity.
  is_screen_accessed_ = false;
}

void ScreenSwitchCheckController::OnRemotingScreenShareStart(
    base::OnceClosure stop_callback) {
  is_remoting_share_ = true;
}

void ScreenSwitchCheckController::OnRemotingScreenShareStop() {
  // Multiple screen share sessions can exist, but they are stopped at once for
  // simplicity.
  is_remoting_share_ = false;
}

}  // namespace ash
