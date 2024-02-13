// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cookies/parsed_cookie.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

bool AreAllCookiesDisabled() {
  return !AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
}

}  // namespace

class AwProxyingRestrictedCookieManagerListener
    : public network::mojom::CookieChangeListener {
 public:
  AwProxyingRestrictedCookieManagerListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      base::WeakPtr<AwProxyingRestrictedCookieManager>
          aw_restricted_cookie_manager,
      mojo::PendingRemote<network::mojom::CookieChangeListener> client_listener,
      bool has_storage_access)
      : url_(url),
        site_for_cookies_(site_for_cookies),
        has_storage_access_(has_storage_access),
        aw_restricted_cookie_manager_(aw_restricted_cookie_manager),
        client_listener_(std::move(client_listener)) {}

  void OnCookieChange(const net::CookieChangeInfo& change) override {
    if (AreAllCookiesDisabled()) {
      return;
    }
    if (change.cookie.IsPartitioned() ||
        (aw_restricted_cookie_manager_ &&
         aw_restricted_cookie_manager_->AllowFullCookies(
             url_, site_for_cookies_, has_storage_access_))) {
      client_listener_->OnCookieChange(change);
    }
  }

 private:
  const GURL url_;
  const net::SiteForCookies site_for_cookies_;
  // restricted_cookie_manager in services/network follows a similar pattern of
  // using the state of "has_storage_access" at the time of the listener being
  // added so we are matching that behaviour. If the storage access was enabled
  // _after_ the listener was added, it will not be updated here.
  bool has_storage_access_;
  base::WeakPtr<AwProxyingRestrictedCookieManager>
      aw_restricted_cookie_manager_;
  mojo::Remote<network::mojom::CookieChangeListener> client_listener_;
};

// static
void AwProxyingRestrictedCookieManager::CreateAndBind(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying_rcm,
    bool is_service_worker,
    int process_id,
    int frame_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<content::GlobalRenderFrameHostToken> frame_token;
  if (!is_service_worker) {
    if (auto* rfh = content::RenderFrameHost::FromID(process_id, frame_id)) {
      frame_token = rfh->GetGlobalFrameToken();
    }
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AwProxyingRestrictedCookieManager::CreateAndBindOnIoThread,
          std::move(underlying_rcm), is_service_worker, frame_token,
          std::move(receiver)));
}

AwProxyingRestrictedCookieManager::~AwProxyingRestrictedCookieManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void AwProxyingRestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    network::mojom::CookieManagerGetOptionsPtr options,
    bool is_ad_tagged,
    GetAllForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (AreAllCookiesDisabled()) {
    std::move(callback).Run(std::vector<net::CookieWithAccessResult>());
    return;
  }

  bool allow_all_cookies =
      AllowFullCookies(url, site_for_cookies, has_storage_access);

  underlying_restricted_cookie_manager_->GetAllForUrl(
      url, site_for_cookies, top_frame_origin, has_storage_access,
      std::move(options), is_ad_tagged,
      base::BindOnce(
          [](bool allow_all_cookies,
             const std::vector<::net::CookieWithAccessResult>& cookies) {
            if (allow_all_cookies) {
              return cookies;
            }

            // We get back a const vector of cookies so we can't modify the
            // original vector. So we instead need to copy all the partitioned
            // cookies to a new list.
            std::vector<::net::CookieWithAccessResult> partitioned_cookies;

            std::copy_if(cookies.begin(), cookies.end(),
                         std::back_inserter(partitioned_cookies),
                         [](::net::CookieWithAccessResult cookie) {
                           return cookie.cookie.IsPartitioned();
                         });

            return partitioned_cookies;
          },
          allow_all_cookies)
          .Then(std::move(callback)));
}

void AwProxyingRestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    net::CookieInclusionStatus status,
    SetCanonicalCookieCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AreAllCookiesDisabled()) {
    std::move(callback).Run(false);
    return;
  }

  if (cookie.IsPartitioned() ||
      AllowFullCookies(url, site_for_cookies, has_storage_access)) {
    underlying_restricted_cookie_manager_->SetCanonicalCookie(
        cookie, url, site_for_cookies, top_frame_origin, has_storage_access,
        status, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void AwProxyingRestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
    AddChangeListenerCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  mojo::PendingRemote<network::mojom::CookieChangeListener>
      proxy_listener_remote;
  auto proxy_listener =
      std::make_unique<AwProxyingRestrictedCookieManagerListener>(
          url, site_for_cookies, weak_factory_.GetWeakPtr(),
          std::move(listener), has_storage_access);

  mojo::MakeSelfOwnedReceiver(
      std::move(proxy_listener),
      proxy_listener_remote.InitWithNewPipeAndPassReceiver());

  underlying_restricted_cookie_manager_->AddChangeListener(
      url, site_for_cookies, top_frame_origin, has_storage_access,
      std::move(proxy_listener_remote), std::move(callback));
}

void AwProxyingRestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AreAllCookiesDisabled()) {
    std::move(callback).Run();
    return;
  }

  // We have to do a quick parsing of the cookie string just to see if the
  // partitioned header is set before handing over the string to the restricted
  // cookie manager.
  net::ParsedCookie parsed_cookie(cookie);

  if (parsed_cookie.IsValid() &&
      ((parsed_cookie.IsPartitioned() && parsed_cookie.IsSecure()) ||
       AllowFullCookies(url, site_for_cookies, has_storage_access))) {
    underlying_restricted_cookie_manager_->SetCookieFromString(
        url, site_for_cookies, top_frame_origin, has_storage_access, cookie,
        std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AwProxyingRestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    bool get_version_shared_memory,
    bool is_ad_tagged,
    GetCookiesStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (AreAllCookiesDisabled()) {
    std::move(callback).Run(network::mojom::kInvalidCookieVersion,
                            base::ReadOnlySharedMemoryRegion(), "");
    return;
  }

  // In order to know if we should return cookies or not, we need to have
  // access to the canonical cookies before they are parsed into a cookie
  // line so that we can apply our filtering logic. Because of this, we don't
  // use restricted cookie managers underlying GetCookiesString method. This
  // isn't a huge loss because the GetCookiesString method mostly has logic
  // around creating shared memory which we don't use on WebView. In Android
  // Webview the access to cookies can change dynamically. For now never
  // request a shared memory region so that a full IPC is issued every time.
  // This prevents a client retaining access to the cookie value past the
  // moment where it was denied. (crbug.com/1393050): Implement a strategy so
  // that the shared memory access can be revoked from here.

  auto match_options = network::mojom::CookieManagerGetOptions::New();
  match_options->name = "";
  match_options->match_type = network::mojom::CookieMatchType::STARTS_WITH;

  auto bound_callback =
      base::BindOnce(std::move(callback), network::mojom::kInvalidCookieVersion,
                     base::ReadOnlySharedMemoryRegion());

  GetAllForUrl(url, site_for_cookies, top_frame_origin, has_storage_access,
               std::move(match_options), is_ad_tagged,
               base::BindOnce([](const std::vector<net::CookieWithAccessResult>&
                                     cookies) {
                 return net::CanonicalCookie::BuildCookieLine(cookies);
               }).Then(std::move(bound_callback)));
}

void AwProxyingRestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool has_storage_access,
    CookiesEnabledForCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(
      AllowFullCookies(url, site_for_cookies, has_storage_access));
}

AwProxyingRestrictedCookieManager::AwProxyingRestrictedCookieManager(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager>
        underlying_restricted_cookie_manager,
    bool is_service_worker,
    const std::optional<const content::GlobalRenderFrameHostToken>&
        global_frame_token)
    : underlying_restricted_cookie_manager_(
          std::move(underlying_restricted_cookie_manager)),
      is_service_worker_(is_service_worker),
      global_frame_token_(global_frame_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void AwProxyingRestrictedCookieManager::CreateAndBindOnIoThread(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying_rcm,
    bool is_service_worker,
    const std::optional<const content::GlobalRenderFrameHostToken>&
        global_frame_token,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto wrapper = base::WrapUnique(new AwProxyingRestrictedCookieManager(
      std::move(underlying_rcm), is_service_worker, global_frame_token));
  mojo::MakeSelfOwnedReceiver(std::move(wrapper), std::move(receiver));
}

bool AwProxyingRestrictedCookieManager::AllowFullCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    bool has_storage_access) const {
  if (is_service_worker_) {
    // Service worker cookies are always first-party, so only need to check
    // the global toggle.
    return AwCookieAccessPolicy::GetInstance()->GetShouldAcceptCookies();
  } else {
    return AwCookieAccessPolicy::GetInstance()->AllowCookies(
        url, site_for_cookies, global_frame_token_, has_storage_access);
  }
}

}  // namespace android_webview
