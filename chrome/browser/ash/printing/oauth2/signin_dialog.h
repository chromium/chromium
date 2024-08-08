// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_SIGNIN_DIALOG_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_SIGNIN_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

class URL;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash::printing::oauth2 {

// Helper class allowing a user to go through OAuth2 authorization procedure.
// The procedure is started by opening a given URL. It is supposed to show a
// sign-in dialog provided by an external Authorization Server. The end of the
// authorization procedure is signaled by the server by redirecting the dialog
// in the last HTTP response to `redirectURI` (it is defined in constants.h).
class SigninDialog : public views::DialogDelegateView,
                     public ChromeWebModalDialogManagerDelegate,
                     public web_modal::WebContentsModalDialogHost,
                     public content::WebContentsObserver {
  METADATA_HEADER(SigninDialog, views::DialogDelegateView)

 public:
  explicit SigninDialog(content::BrowserContext* browser_context);

  SigninDialog(const SigninDialog&) = delete;
  SigninDialog& operator=(const SigninDialog&) = delete;

  ~SigninDialog() override;

  // Open the dialog and navigate to the given `auth_url`. `callback` is called
  // when the authorization procedure is completed or when a user closes the
  // dialog.
  void StartAuthorizationProcedure(const GURL& auth_url,
                                   StatusCallback callback);

 private:
  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  ui::mojom::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;

  // content::WebContentsObserver:
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;

  raw_ptr<views::WebView> web_view_;
  StatusCallback callback_;
  base::WeakPtrFactory<SigninDialog> weak_factory_{this};
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_SIGNIN_DIALOG_H_
