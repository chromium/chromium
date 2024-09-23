// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/signin_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

SigninDialog::SigninDialog(content::BrowserContext* browser_context)
    : web_view_(
          AddChildView(std::make_unique<views::WebView>(browser_context))) {
  SetHasWindowSizeControls(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_use_custom_frame(false);
  SetUseDefaultFillLayout(true);

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_view_->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      web_view_->GetWebContents())
      ->SetDelegate(this);

  Observe(web_view_->GetWebContents());
}

SigninDialog::~SigninDialog() {
  if (callback_) {
    std::move(callback_).Run(StatusCode::kUnexpectedError,
                             "authorization dialog was closed");
  }
}

void SigninDialog::StartAuthorizationProcedure(const GURL& auth_url,
                                               StatusCallback callback) {
  callback_ = std::move(callback);
  const std::string title = auth_url.GetWithEmptyPath().spec();
  SetTitle(std::u16string(title.begin(), title.end()));
  web_view_->LoadInitialURL(auth_url);
  GetWidget()->Show();
}

web_modal::WebContentsModalDialogHost*
SigninDialog::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView SigninDialog::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point SigninDialog::GetDialogPosition(const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2),
                    std::max(0, (widget_size.height() - size.height()) / 2));
}

gfx::Size SigninDialog::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void SigninDialog::AddObserver(web_modal::ModalDialogHostObserver* observer) {}

void SigninDialog::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

// views::DialogDelegate:
gfx::Size SigninDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // TODO(https://crbug.com/1223535): need to tweak this.
  // Or remove this whole class if not needed anymore.
  return gfx::Size(800, 640);
}

ui::mojom::ModalType SigninDialog::GetModalType() const {
  return ui::mojom::ModalType::kWindow;
}

views::View* SigninDialog::GetInitiallyFocusedView() {
  return web_view_.get();
}

void SigninDialog::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& url = navigation_handle->GetURL();
  // Check if the URL is a redirectURI marking the end of the process.
  if (base::StartsWith(url.possibly_invalid_spec(), kRedirectURI)) {
    GetWidget()->Close();
    std::move(callback_).Run(StatusCode::kOK, url.spec());
  }
}

BEGIN_METADATA(SigninDialog)
END_METADATA

}  // namespace ash::printing::oauth2
