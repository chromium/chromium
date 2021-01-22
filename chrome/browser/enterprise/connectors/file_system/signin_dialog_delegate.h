// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

namespace enterprise_connectors {

// Helper class used by FileSystemRenameHandler to show a sign-in diaglog of Box
// to obtain authorization when there is no valid refresh token.
class FileSystemSigninDialogDelegate
    : public views::DialogDelegateView,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public content::WebContentsObserver {
 public:
  // Called with success or failure of this authorization attempt.
  using AuthorizationCompletedCallback = base::OnceCallback<void(bool)>;

  ~FileSystemSigninDialogDelegate() override;

  static void ShowDialog(content::WebContents* web_contents,
                         AuthorizationCompletedCallback callback);

 private:
  FileSystemSigninDialogDelegate(content::BrowserContext* browser_context,
                                 AuthorizationCompletedCallback callback);

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // views::DialogDelegate:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  void DeleteDelegate() override;
  views::View* GetInitiallyFocusedView() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnGotOAuthTokens(bool success,
                        const std::string& access_token,
                        const std::string& refresh_token);

  std::unique_ptr<views::WebView> web_view_;
  std::unique_ptr<OAuth2AccessTokenFetcherImpl> token_fetcher_;
  AuthorizationCompletedCallback callback_;
  base::WeakPtrFactory<FileSystemSigninDialogDelegate> factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_
