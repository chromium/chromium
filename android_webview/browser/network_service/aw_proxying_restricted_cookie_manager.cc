// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_restricted_cookie_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/cookie_manager.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cookies/parsed_cookie.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

using PrivacySetting = net::NetworkDelegate::PrivacySetting;

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
      net::StorageAccessApiStatus storage_access_api_status)
      : url_(url),
        site_for_cookies_(site_for_cookies),
        storage_access_api_status_(storage_access_api_status),
        aw_restricted_cookie_manager_(aw_restricted_cookie_manager),
        client_listener_(std::move(client_listener)) {}

  void OnCookieChange(const net::CookieChangeInfo& change) override {
    if (aw_restricted_cookie_manager_) {
      PrivacySetting cookieState = aw_restricted_cookie_manager_->AllowCookies(
          url_, site_for_cookies_, storage_access_api_status_);

      if (cookieState == PrivacySetting::kStateAllowed ||
          (cookieState == PrivacySetting::kPartitionedStateAllowedOnly &&
           change.cookie.IsPartitioned())) {
        client_listener_->OnCookieChange(change);
      }
    }
  }

 private:
  const GURL url_;
  const net::SiteForCookies site_for_cookies_;
  // restricted_cookie_manager in services/network follows a similar pattern of
  // using the state of "storage_access_api_status" at the time of the listener
  // being added so we are matching that behaviour. If the storage access was
  // enabled _after_ the listener was added, it will not be updated here.
  net::StorageAccessApiStatus storage_access_api_status_;
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
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
    AwCookieAccessPolicy* aw_cookie_access_policy) {
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
          std::move(receiver), aw_cookie_access_policy));
}

AwProxyingRestrictedCookieManager::~AwProxyingRestrictedCookieManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void AwProxyingRestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    network::mojom::CookieManagerGetOptionsPtr options,
    bool is_ad_tagged,
    bool force_disable_third_party_cookies,
    GetAllForUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  PrivacySetting cookieState =
      AllowCookies(url, site_for_cookies, storage_access_api_status);

  if (cookieState == PrivacySetting::kStateDisallowed) {
    std::move(callback).Run(std::vector<net::CookieWithAccessResult>());
    return;
  }

  bool disable_3pcs =
      force_disable_third_party_cookies ||
      cookieState == PrivacySetting::kPartitionedStateAllowedOnly;

  underlying_restricted_cookie_manager_->GetAllForUrl(
      url, site_for_cookies, top_frame_origin, storage_access_api_status,
      std::move(options), is_ad_tagged, disable_3pcs, std::move(callback));
}

void AwProxyingRestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    net::CookieInclusionStatus status,
    SetCanonicalCookieCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  PrivacySetting cookieState =
      AllowCookies(url, site_for_cookies, storage_access_api_status);

  if (cookieState == PrivacySetting::kStateDisallowed) {
    std::move(callback).Run(false);
    return;
  }

  if (cookie.IsPartitioned() || cookieState == PrivacySetting::kStateAllowed) {
    underlying_restricted_cookie_manager_->SetCanonicalCookie(
        cookie, url, site_for_cookies, top_frame_origin,
        storage_access_api_status, status, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void AwProxyingRestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
    AddChangeListenerCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  mojo::PendingRemote<network::mojom::CookieChangeListener>
      proxy_listener_remote;
  auto proxy_listener =
      std::make_unique<AwProxyingRestrictedCookieManagerListener>(
          url, site_for_cookies, weak_factory_.GetWeakPtr(),
          std::move(listener), storage_access_api_status);

  mojo::MakeSelfOwnedReceiver(
      std::move(proxy_listener),
      proxy_listener_remote.InitWithNewPipeAndPassReceiver());

  underlying_restricted_cookie_manager_->AddChangeListener(
      url, site_for_cookies, top_frame_origin, storage_access_api_status,
      std::move(proxy_listener_remote), std::move(callback));
}

void AwProxyingRestrictedCookieManager::SetCookieFromString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    const std::string& cookie,
    SetCookieFromStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  PrivacySetting cookieState =
      AllowCookies(url, site_for_cookies, storage_access_api_status);

  if (cookieState == PrivacySetting::kStateDisallowed) {
    std::move(callback).Run();
    return;
  }

  // We have to do a quick parsing of the cookie string just to see if the
  // partitioned header is set before handing over the string to the restricted
  // cookie manager.
  net::ParsedCookie parsed_cookie(cookie);

  if (cookieState == PrivacySetting::kStateAllowed ||
      (parsed_cookie.IsValid() && parsed_cookie.IsPartitioned() &&
       parsed_cookie.IsSecure())) {
    underlying_restricted_cookie_manager_->SetCookieFromString(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        cookie, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AwProxyingRestrictedCookieManager::GetCookiesString(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    bool get_version_shared_memory,
    bool is_ad_tagged,
    bool force_disable_third_party_cookies,
    GetCookiesStringCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  PrivacySetting cookieState =
      AllowCookies(url, site_for_cookies, storage_access_api_status);

  if (cookieState == PrivacySetting::kStateDisallowed) {
    std::move(callback).Run(network::mojom::kInvalidCookieVersion,
                            base::ReadOnlySharedMemoryRegion(), "");
    return;
  }

  bool disable_3pcs =
      force_disable_third_party_cookies ||
      cookieState == PrivacySetting::kPartitionedStateAllowedOnly;

  // In Android Webview the access to cookies can change dynamically. For
  // now never request a shared memory region so that a full IPC is issued
  // every time. This prevents a client retaining access to the cookie value
  // past the moment where it was denied. (crbug.com/1393050): Implement a
  // strategy so that the shared memory access can be revoked from here.
  underlying_restricted_cookie_manager_->GetCookiesString(
      url, site_for_cookies, top_frame_origin, storage_access_api_status,
      /*get_version_shared_memory=*/false, is_ad_tagged, disable_3pcs,
      std::move(callback));
}

void AwProxyingRestrictedCookieManager::CookiesEnabledFor(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::StorageAccessApiStatus storage_access_api_status,
    CookiesEnabledForCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(
      AllowCookies(url, site_for_cookies, storage_access_api_status) ==
      PrivacySetting::kStateAllowed);
}

AwProxyingRestrictedCookieManager::AwProxyingRestrictedCookieManager(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager>
        underlying_restricted_cookie_manager,
    bool is_service_worker,
    const std::optional<const content::GlobalRenderFrameHostToken>&
        global_frame_token,
    AwCookieAccessPolicy* cookie_access_policy)
    : underlying_restricted_cookie_manager_(
          std::move(underlying_restricted_cookie_manager)),
      is_service_worker_(is_service_worker),
      global_frame_token_(global_frame_token),
      cookie_access_policy_(*cookie_access_policy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void AwProxyingRestrictedCookieManager::CreateAndBindOnIoThread(
    mojo::PendingRemote<network::mojom::RestrictedCookieManager> underlying_rcm,
    bool is_service_worker,
    const std::optional<const content::GlobalRenderFrameHostToken>&
        global_frame_token,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
    AwCookieAccessPolicy* cookie_access_policy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto wrapper = base::WrapUnique(new AwProxyingRestrictedCookieManager(
      std::move(underlying_rcm), is_service_worker, global_frame_token,
      cookie_access_policy));
  mojo::MakeSelfOwnedReceiver(std::move(wrapper), std::move(receiver));
}

PrivacySetting AwProxyingRestrictedCookieManager::AllowCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    net::StorageAccessApiStatus storage_access_api_status) const {
  if (is_service_worker_) {
    // Service worker cookies are always first-party, so only need to check
    // the global toggle.
    return cookie_access_policy_->GetShouldAcceptCookies()
               ? PrivacySetting::kStateAllowed
               : PrivacySetting::kStateDisallowed;
  } else {
    return cookie_access_policy_->AllowCookies(
        url, site_for_cookies, global_frame_token_, storage_access_api_status);
  }
}

}  // namespace android_webview
