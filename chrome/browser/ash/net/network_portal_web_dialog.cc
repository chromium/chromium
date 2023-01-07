// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_web_dialog.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const float kNetworkPortalWebDialogScale = .8;

gfx::Size GetPortalDialogSize() {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  return gfx::Size(display.size().width() * kNetworkPortalWebDialogScale,
                   display.size().height() * kNetworkPortalWebDialogScale);
}

}  // namespace

NetworkPortalWebDialog::NetworkPortalWebDialog(const GURL& url,
                                               base::WeakPtr<Delegate> delegate)
    : url_(url), delegate_(delegate), widget_(nullptr) {
  set_can_resize(false);
}

NetworkPortalWebDialog::~NetworkPortalWebDialog() {
  if (delegate_)
    delegate_->OnDialogDestroyed(this);
}

void NetworkPortalWebDialog::Close() {
  if (widget_)
    widget_->Close();
}

void NetworkPortalWebDialog::SetWidget(views::Widget* widget) {
  widget_ = widget;
}

ui::ModalType NetworkPortalWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

std::u16string NetworkPortalWebDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CAPTIVE_PORTAL_AUTHORIZATION_DIALOG_NAME);
}

GURL NetworkPortalWebDialog::GetDialogContentURL() const {
  return url_;
}

void NetworkPortalWebDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void NetworkPortalWebDialog::GetDialogSize(gfx::Size* size) const {
  *size = GetPortalDialogSize();
}

std::string NetworkPortalWebDialog::GetDialogArgs() const {
  return std::string();
}

void NetworkPortalWebDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void NetworkPortalWebDialog::OnCloseContents(content::WebContents* /* source */,
                                             bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool NetworkPortalWebDialog::ShouldShowDialogTitle() const {
  return true;
}

}  // namespace ash
