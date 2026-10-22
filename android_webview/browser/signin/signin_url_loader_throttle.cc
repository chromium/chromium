// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/signin/signin_url_loader_throttle.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/signin/aw_google_accounts_delegate.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace android_webview {

namespace {
constexpr char kWebViewMirrorHeaderSource[] = "AndroidWebView";

AwGoogleAccountsDelegate* GetDelegateFromWebContents(
    const content::WebContents::Getter& web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    return nullptr;
  }

  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents) {
    return nullptr;
  }
  return aw_contents->maybe_google_accounts_delegate();
}

void ProcessMirrorHeader(
    SigninUrlLoaderThrottle::GoogleAccountsDelegateGetter delegate_getter,
    const signin::ManageAccountsParams& params,
    bool is_outermost_main_frame,
    bool is_navigation_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AwGoogleAccountsDelegate* delegate = delegate_getter.Run();
  if (delegate) {
    delegate->OnGoogleAccountsRequest(params, is_outermost_main_frame,
                                      is_navigation_request);
  }
}

bool IsFullCookieAccessAllowed(AwBrowserContext* aw_browser_context,
                               const GURL& url,
                               const net::SiteForCookies& site_for_cookies,
                               const url::Origin& top_frame_origin,
                               net::CookieSettingOverrides overrides) {
  CookieManager* cookie_manager = aw_browser_context->GetCookieManager();
  AwCookieAccessPolicy* policy = cookie_manager->cookie_access_policy();
  // Check if cookies are globally accepted.
  if (!policy->GetShouldAcceptCookies()) {
    return false;
  }

  // Check if the URL is allowed cookies in this context (including 3P check).
  // Note: top_frame_origin is considered, third-party cookies default to
  // false if no frame token is provided.
  net::NetworkDelegate::PrivacySetting privacy_setting =
      AwCookieAccessPolicy::CanAccessCookies(
          url, site_for_cookies, policy->GetShouldAcceptCookies(),
          /*accept_third_party_cookies=*/false);
  return privacy_setting == net::NetworkDelegate::PrivacySetting::kStateAllowed;
}

}  // namespace

SigninUrlLoaderThrottle::~SigninUrlLoaderThrottle() = default;

// static
std::unique_ptr<SigninUrlLoaderThrottle> SigninUrlLoaderThrottle::Create(
    content::BrowserContext* browser_context,
    content::WebContents::Getter web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GoogleAccountsDelegateGetter delegate_getter =
      base::BindRepeating(&GetDelegateFromWebContents, web_contents_getter);
  if (!delegate_getter.Run()) {
    return nullptr;
  }

  return base::WrapUnique(new SigninUrlLoaderThrottle(
      browser_context,
      base::BindRepeating(
          &IsFullCookieAccessAllowed,
          base::Unretained(static_cast<AwBrowserContext*>(browser_context))),
      std::move(delegate_getter)));
}

// static
std::unique_ptr<SigninUrlLoaderThrottle>
SigninUrlLoaderThrottle::CreateForTesting(
    content::BrowserContext* browser_context,
    signin::CookieSettingsCallback cookie_settings_callback,
    GoogleAccountsDelegateGetter delegate_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!delegate_getter.Run()) {
    return nullptr;
  }

  return base::WrapUnique(new SigninUrlLoaderThrottle(
      browser_context, std::move(cookie_settings_callback),
      std::move(delegate_getter)));
}

void SigninUrlLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  AwGoogleAccountsDelegate* delegate = delegate_getter_.Run();
  if (!delegate) {
    return;
  }

  request_url_ = request->url;
  is_navigation_request_ =
      request->destination == network::mojom::RequestDestination::kDocument ||
      request->destination == network::mojom::RequestDestination::kIframe;
  is_outermost_main_frame_ = request->is_outermost_main_frame;

  net::HttpRequestHeaders modified_request_headers;
  std::vector<std::string> to_be_removed_request_headers;
  ProcessRequest(GURL(), &request->headers, &to_be_removed_request_headers,
                 &modified_request_headers);
  request_headers_ = request->headers;
}

void SigninUrlLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  if (!delegate_getter_.Run()) {
    return;
  }

  ProcessResponse(response_head.headers.get());
  ProcessRequest(redirect_info->new_url, &request_headers_,
                 &headers_update_params->removed_headers,
                 &headers_update_params->modified_headers);

  // Update the current request_url_ to point to the redirect URL,
  // for the next call to WillRedirectRequest or WillProcessResponse.
  request_url_ = redirect_info->new_url;
}

void SigninUrlLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (!delegate_getter_.Run()) {
    return;
  }

  ProcessResponse(response_head->headers.get());
}

SigninUrlLoaderThrottle::SigninUrlLoaderThrottle(
    content::BrowserContext* browser_context,
    signin::CookieSettingsCallback cookie_settings_callback,
    GoogleAccountsDelegateGetter delegate_getter)
    : browser_context_(browser_context),
      cookie_settings_callback_(std::move(cookie_settings_callback)),
      delegate_getter_(std::move(delegate_getter)) {}

void SigninUrlLoaderThrottle::ProcessRequest(
    const GURL& new_url,
    net::HttpRequestHeaders* original_headers,
    std::vector<std::string>* headers_to_remove,
    net::HttpRequestHeaders* modified_headers) {
  AwGoogleAccountsDelegate* delegate = delegate_getter_.Run();
  if (!delegate) {
    return;
  }

  signin::RequestAdapter request_adapter(request_url_, *original_headers,
                                         modified_headers, headers_to_remove);
  signin::AppendOrRemoveMirrorRequestHeader(
      &request_adapter, new_url, GaiaId(delegate->GetGaiaId()),
      /*is_child_account=*/signin::Tribool::kFalse,
      signin::AccountConsistencyMethod::kMirror, cookie_settings_callback_,
      signin::PROFILE_MODE_INCOGNITO_DISABLED |
          signin::PROFILE_MODE_ADD_ACCOUNT_DISABLED,
      kWebViewMirrorHeaderSource, /*force_account_consistency=*/true);

  original_headers->MergeFrom(*modified_headers);
  for (const std::string& name : *headers_to_remove) {
    original_headers->RemoveHeader(name);
  }
}

void SigninUrlLoaderThrottle::ProcessResponse(
    const net::HttpResponseHeaders* headers) {
  if (!gaia::HasGaiaSchemeHostPort(request_url_) || !headers) {
    return;
  }

  std::optional<std::string> header_value =
      headers->GetNormalizedHeader(signin::kChromeManageAccountsHeader);
  if (!header_value) {
    return;
  }

  // Only process one mirror header per request (multiple headers on the same
  // redirect chain are ignored).
  if (response_header_processed_) {
    LOG(ERROR) << "Multiple X-Chrome-Manage-Accounts headers on a redirect "
               << "chain, ignoring";
    return;
  }

  signin::ManageAccountsParams params =
      signin::BuildManageAccountsParams(*header_value);
  if (params.service_type == signin::GAIA_SERVICE_TYPE_NONE) {
    return;
  }

  response_header_processed_ = true;

  // Post a task even if we are already on the UI thread to avoid making any
  // requests while processing a throttle event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ProcessMirrorHeader, delegate_getter_, std::move(params),
                     is_outermost_main_frame_, is_navigation_request_));
}

}  // namespace android_webview
