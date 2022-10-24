// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_

#include <vector>

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_response.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(FileSystemSigninDialogDelegate);

  // Called with success or failure of this authorization attempt.
  // The tokens passed to this callback have not been saved.  The callback
  // is expected to save them if needed.
  using AuthorizationCompletedCallback = AccessTokenFetcher::TokenCallback;

  ~FileSystemSigninDialogDelegate() override;
  FileSystemSigninDialogDelegate(content::BrowserContext* browser_context,
                                 const FileSystemSettings& settings,
                                 absl::optional<std::string> account_login,
                                 AuthorizationCompletedCallback callback);
  // Visible for testing.
  void OnGotOAuthTokens(const GoogleServiceAuthError& status,
                        const std::string& access_token,
                        const std::string& refresh_token);

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

  // views::DialogDelegate:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;

  void OnCancellation();
  void OnGotCurrentUserResponse(BoxApiCallResponse response,
                                base::Value::Dict user_info);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Invoke the callback with the status of the auth flow and tokens if
  // obtained.
  void OnAuthFlowDone(const GoogleServiceAuthError& status);

  // Return extra URL parameters that are specific to a given service provider.
  // May return the empty string if there are none.
  std::string GetProviderSpecificUrlParameters();

  // Return display name for the service provider.
  std::u16string GetProviderName() const;

  // Get a URLLoaderFactory for OAuth2Flows.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  const FileSystemSettings settings_;
  absl::optional<std::string> account_login_;
  std::string access_token_;
  std::string refresh_token_;
  std::unique_ptr<views::WebView> web_view_;
  std::unique_ptr<OAuth2AccessTokenFetcherImpl> token_fetcher_;
  AuthorizationCompletedCallback callback_;
  std::unique_ptr<OAuth2ApiCallFlow> current_api_call_;
  base::WeakPtrFactory<FileSystemSigninDialogDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_DIALOG_DELEGATE_H_
