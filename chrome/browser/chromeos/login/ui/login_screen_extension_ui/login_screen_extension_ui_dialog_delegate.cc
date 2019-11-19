// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_create_options.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {
const double kRelativeScreenWidth = 0.9;
const double kRelativeScreenHeight = 0.8;
}  // namespace

LoginScreenExtensionUiDialogDelegate::LoginScreenExtensionUiDialogDelegate(
    LoginScreenExtensionUiCreateOptions* create_options)
    : extension_name_(create_options->extension_name),
      content_url_(create_options->content_url),
      can_close_(create_options->can_be_closed_by_user),
      close_callback_(std::move(create_options->close_callback)) {}

LoginScreenExtensionUiDialogDelegate::~LoginScreenExtensionUiDialogDelegate() =
    default;

ui::ModalType LoginScreenExtensionUiDialogDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 LoginScreenExtensionUiDialogDelegate::GetDialogTitle() const {
  return l10n_util::GetStringFUTF16(IDS_LOGIN_EXTENSION_UI_DIALOG_TITLE,
                                    base::UTF8ToUTF16(extension_name_));
}

GURL LoginScreenExtensionUiDialogDelegate::GetDialogContentURL() const {
  return content_url_;
}

void LoginScreenExtensionUiDialogDelegate::GetDialogSize(
    gfx::Size* size) const {
  gfx::Size screen_size = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(native_window_)
                              .size();
  *size = gfx::Size(kRelativeScreenWidth * screen_size.width(),
                    kRelativeScreenHeight * screen_size.height());
}

bool LoginScreenExtensionUiDialogDelegate::CanCloseDialog() const {
  return can_close_;
}

bool LoginScreenExtensionUiDialogDelegate::CanResizeDialog() const {
  return false;
}

void LoginScreenExtensionUiDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

std::string LoginScreenExtensionUiDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void LoginScreenExtensionUiDialogDelegate::OnDialogClosed(
    const std::string& json_retval) {
  std::move(close_callback_).Run();
  delete this;
}

void LoginScreenExtensionUiDialogDelegate::OnCloseContents(
    content::WebContents* source,
    bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool LoginScreenExtensionUiDialogDelegate::ShouldShowDialogTitle() const {
  return true;
}

bool LoginScreenExtensionUiDialogDelegate::ShouldCenterDialogTitleText() const {
  return true;
}

}  // namespace chromeos
