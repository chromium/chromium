// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/captive_portal_dialog_delegate.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

CaptivePortalDialogDelegate::CaptivePortalDialogDelegate(
    views::WebDialogView* host_dialog_view)
    : host_view_(host_dialog_view),
      web_contents_(host_dialog_view->web_contents()) {
  view_ =
      new views::WebDialogView(ProfileHelper::GetSigninProfile(), this,
                               std::make_unique<ChromeWebContentsHandler>());
  view_->SetVisible(false);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_LockSystemModalContainer);

  widget_ = new views::Widget;
  widget_->Init(std::move(params));
  widget_->SetBounds(display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(widget_->GetNativeWindow())
                         .work_area());
  widget_->SetOpacity(0);

  // Set this as the web modal delegate so that captive portal dialog can
  // appear.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
      ->SetDelegate(this);
}

CaptivePortalDialogDelegate::~CaptivePortalDialogDelegate() {
  // TODO(jamescook): Clean up modal dialog delegate and observers.
}

void CaptivePortalDialogDelegate::Show() {
  widget_->Show();
}

void CaptivePortalDialogDelegate::Hide() {
  widget_->Hide();
}

void CaptivePortalDialogDelegate::Close() {
  widget_->Close();
}

ui::ModalType CaptivePortalDialogDelegate::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

std::u16string CaptivePortalDialogDelegate::GetDialogTitle() const {
  return std::u16string();
}

GURL CaptivePortalDialogDelegate::GetDialogContentURL() const {
  return GURL();
}

void CaptivePortalDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void CaptivePortalDialogDelegate::GetDialogSize(gfx::Size* size) const {
  *size = display::Screen::GetScreen()
              ->GetDisplayNearestWindow(widget_->GetNativeWindow())
              .work_area()
              .size();
}

std::string CaptivePortalDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void CaptivePortalDialogDelegate::OnDialogClosed(
    const std::string& json_retval) {
  delete this;
}

void CaptivePortalDialogDelegate::OnCloseContents(content::WebContents* source,
                                                  bool* out_close_dialog) {
  *out_close_dialog = false;
}

bool CaptivePortalDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

web_modal::WebContentsModalDialogHost*
CaptivePortalDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView CaptivePortalDialogDelegate::GetHostView() const {
  if (widget_->IsVisible())
    return widget_->GetNativeWindow();
  else
    return host_view_->GetWidget()->GetNativeWindow();
}

gfx::Point CaptivePortalDialogDelegate::GetDialogPosition(
    const gfx::Size& size) {
  // Center the dialog in the screen.
  gfx::Size host_size = GetHostView()->GetBoundsInScreen().size();
  return gfx::Point(host_size.width() / 2 - size.width() / 2,
                    host_size.height() / 2 - size.height() / 2);
}

gfx::Size CaptivePortalDialogDelegate::GetMaximumDialogSize() {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(widget_->GetNativeWindow())
      .work_area()
      .size();
}

void CaptivePortalDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  // TODO(jamescook): Store observers in a list.
}

void CaptivePortalDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  // TODO(jamescook): Store observers in a list.
}

base::WeakPtr<CaptivePortalDialogDelegate>
CaptivePortalDialogDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace chromeos
