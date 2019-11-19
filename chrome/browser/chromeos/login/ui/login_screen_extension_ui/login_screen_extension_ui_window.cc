// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_window.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_create_options.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_web_dialog_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

LoginScreenExtensionUiWindowFactory::LoginScreenExtensionUiWindowFactory() =
    default;
LoginScreenExtensionUiWindowFactory::~LoginScreenExtensionUiWindowFactory() =
    default;

std::unique_ptr<LoginScreenExtensionUiWindow>
LoginScreenExtensionUiWindowFactory::Create(
    LoginScreenExtensionUiCreateOptions* create_options) {
  return std::make_unique<LoginScreenExtensionUiWindow>(create_options);
}

LoginScreenExtensionUiWindow::LoginScreenExtensionUiWindow(
    LoginScreenExtensionUiCreateOptions* create_options)
    : dialog_delegate_(
          new LoginScreenExtensionUiDialogDelegate(create_options)),
      dialog_view_(new LoginScreenExtensionUiWebDialogView(
          ProfileHelper::GetSigninProfile(),
          dialog_delegate_,
          std::make_unique<ChromeWebContentsHandler>())) {
  dialog_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = dialog_view_;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_LockScreenContainer);
  dialog_widget_->Init(std::move(params));
  dialog_widget_->set_movement_disabled(true);
  dialog_delegate_->set_native_window(dialog_widget_->GetNativeWindow());
  dialog_widget_->Show();
}

LoginScreenExtensionUiWindow::~LoginScreenExtensionUiWindow() {
  dialog_delegate_->set_can_close(true);
  dialog_widget_->Close();
}

LoginScreenExtensionUiDialogDelegate*
LoginScreenExtensionUiWindow::GetDialogDelegateForTesting() {
  return dialog_delegate_;
}

views::Widget* LoginScreenExtensionUiWindow::GetDialogWidgetForTesting() {
  return dialog_widget_;
}

}  // namespace chromeos
