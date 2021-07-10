// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_dialog_delegate.h"

#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_endpoints.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/grit/generated_resources.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

// TODO(https://crbug.com/1227477): move this class to chrome/browser/ui/views.

namespace {

// The OAuth token consumer name.
const char kOAuthConsumerName[] = "file_system_signin_dialog";

// The OAuth2 configuration of the Box App used for the file system integration
// uses https://google.com/generate_204 as the redirect URI.
//    This URI is used because:
//     1/ Chrome is a native app and does not otherwise have an app URI to
//     redirect to.
//     2/ An HTTP 204 response is optimal because it is a success.
//     3/ An HTTP 204 response is optimal because it is the smallest
//        response that can be generated (it has no body).
//     4/ This URI is used by other native apps for the same purpose.
//     5/ It is controlled by Google.
bool IsOAuth2RedirectURI(const GURL& url) {
  return url.host() == "google.com" && url.path() == "/generate_204";
}

}  // namespace

namespace enterprise_connectors {

////// Authorization Dialog ///////////////////////////////////////////////////

FileSystemSigninDialogDelegate::FileSystemSigninDialogDelegate(
    content::BrowserContext* browser_context,
    const FileSystemSettings& settings,
    AuthorizationCompletedCallback callback)
    : settings_(settings),
      web_view_(std::make_unique<views::WebView>(browser_context)),
      callback_(std::move(callback)) {
  SetHasWindowSizeControls(true);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_DIALOG_TITLE, GetProviderName()));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_use_custom_frame(false);

  SetCancelCallback(
      base::BindOnce(&FileSystemSigninDialogDelegate::OnCancellation,
                     weak_factory_.GetWeakPtr()));

  AddChildView(web_view_.get());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_view_->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      web_view_->GetWebContents())
      ->SetDelegate(this);

  Observe(web_view_->GetWebContents());

  std::string query = base::StringPrintf("client_id=%s&response_type=code",
                                         settings_.client_id.c_str());
  std::string extra_params = GetProviderSpecificUrlParameters();
  if (!extra_params.empty())
    base::StringAppendF(&query, "&%s", extra_params.c_str());

  url::Replacements<char> replacements;
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  GURL url = settings_.authorization_endpoint.ReplaceComponents(replacements);
  web_view_->LoadInitialURL(url);
}

FileSystemSigninDialogDelegate::~FileSystemSigninDialogDelegate() {
  if (callback_) {
    OnCancellation();
  }
}

web_modal::WebContentsModalDialogHost*
FileSystemSigninDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView FileSystemSigninDialogDelegate::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point FileSystemSigninDialogDelegate::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2),
                    std::max(0, (widget_size.height() - size.height()) / 2));
}

gfx::Size FileSystemSigninDialogDelegate::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void FileSystemSigninDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void FileSystemSigninDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

// views::DialogDelegate:
gfx::Size FileSystemSigninDialogDelegate::CalculatePreferredSize() const {
  // TODO(https://crbug.com/1159213): need to tweak this.
  return gfx::Size(800, 640);
}

ui::ModalType FileSystemSigninDialogDelegate::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

views::View* FileSystemSigninDialogDelegate::GetInitiallyFocusedView() {
  return static_cast<views::View*>(web_view_.get());
}

void FileSystemSigninDialogDelegate::OnCancellation() {
  ReturnCancellation(std::move(callback_));
}

void FileSystemSigninDialogDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& url = navigation_handle->GetURL();
  if (!IsOAuth2RedirectURI(url)) {
    return;
  }

  std::string auth_code;
  // Look for the auth_code.  It is found in the code= URL parameter.
  if (!net::GetValueForKeyInQuery(url, "code", &auth_code)) {
    DLOG(ERROR) << "Failed to extract authorization code from url: " << url;
    // TODO(https://crbug.com/1159179): pop dialog about authentication failure?
    return;
  }

  content::StoragePartition* partition =
      web_view_->GetBrowserContext()->GetStoragePartitionForUrl(
          GURL(kFileSystemBoxEndpointApi));
  auto url_loader = partition->GetURLLoaderFactoryForBrowserProcess();
  auto callback =
      base::BindOnce(&FileSystemSigninDialogDelegate::OnGotOAuthTokens,
                     weak_factory_.GetWeakPtr());

  // No refresh_token, so need to get both tokens with authorization code.
  token_fetcher_ = std::make_unique<AccessTokenFetcher>(
      url_loader, settings_.service_provider, settings_.token_endpoint,
      /*refresh_token=*/std::string(), auth_code, kOAuthConsumerName,
      std::move(callback));
  token_fetcher_->Start(settings_.client_id, settings_.client_secret,
                        settings_.scopes);
}

void FileSystemSigninDialogDelegate::OnGotOAuthTokens(
    const GoogleServiceAuthError& status,
    const std::string& access_token,
    const std::string& refresh_token) {
  token_fetcher_ = nullptr;
  std::move(callback_).Run(status, access_token, refresh_token);
  GetWidget()->Close();
}

std::string FileSystemSigninDialogDelegate::GetProviderSpecificUrlParameters() {
  // If an email domain is specified, use it as a hint in the box authn URL.
  // Make sure the domain has an @ prefix.
  if (settings_.service_provider == kBoxProviderName) {
    if (!settings_.email_domain.empty()) {
      // If the domain does not already start with an @ sign, prepend the
      // escaped version of it.
      return base::StringPrintf(
          "box_login=%s%s", settings_.email_domain[0] == '@' ? "" : "%40",
          net::EscapeQueryParamValue(settings_.email_domain, true).c_str());
    }
  } else {
    NOTREACHED() << "Unknown service provider: " << settings_.service_provider;
  }

  return std::string();
}

std::u16string FileSystemSigninDialogDelegate::GetProviderName() const {
  DCHECK_EQ(settings_.service_provider, kBoxProviderName);
  return l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX);
}

BEGIN_METADATA(FileSystemSigninDialogDelegate, views::DialogDelegateView)
END_METADATA

// TODO(https://crbug.com/1159185): add browser_tests for this delegate.

}  // namespace enterprise_connectors
