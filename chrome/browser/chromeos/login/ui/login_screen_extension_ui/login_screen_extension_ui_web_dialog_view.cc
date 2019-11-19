// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_web_dialog_view.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

LoginScreenExtensionUiWebDialogView::LoginScreenExtensionUiWebDialogView(
    content::BrowserContext* context,
    LoginScreenExtensionUiDialogDelegate* delegate,
    std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
        handler)
    : views::WebDialogView(context, delegate, std::move(handler)),
      delegate_(delegate) {
  if (LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->AddSystemTrayFocusObserver(this);
  }
}

LoginScreenExtensionUiWebDialogView::~LoginScreenExtensionUiWebDialogView() {
  if (LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->RemoveSystemTrayFocusObserver(this);
  }
}

bool LoginScreenExtensionUiWebDialogView::ShouldShowCloseButton() const {
  return !delegate_ || delegate_->CanCloseDialog();
}

bool LoginScreenExtensionUiWebDialogView::TakeFocus(
    content::WebContents* source,
    bool reverse) {
  ash::LoginScreen::Get()->FocusLoginShelf(reverse);
  return true;
}

bool LoginScreenExtensionUiWebDialogView::ShouldCenterWindowTitleText() const {
  return !delegate_ || delegate_->ShouldCenterDialogTitleText();
}

void LoginScreenExtensionUiWebDialogView::OnFocusLeavingSystemTray(
    bool reverse) {
  web_contents()->FocusThroughTabTraversal(reverse);
  web_contents()->Focus();
}

}  // namespace chromeos
