// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_

#include <optional>
#include <string>

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_delegate.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"

class GURL;

namespace android_webview {

// A RestrictedCookieManager conditionally returns cookies from an underlying
// RestrictedCookieManager, after consulting WebView's cookie settings.
// We need to do this because Chromium typically configures this per
// BrowserContext but Android developers can set cookie permissions per WebView.
// To work around this, we need to feed down to the restricted cookie manager
// if we wish to disable 3PCs _per_ request.
class AwProxyingRestrictedCookieManager
    : public network::mojom::RestrictedCookieManager {
 public:
  // Creates a AwProxyingRestrictedCookieManager that lives on IO thread,
  // binding it to handle communications from |receiver|. The requests will be
  // delegated to |underlying_rcm|. The resulting object will be owned by the
  // pipe corresponding to |request| and will in turn own |underlying_rcm|.
  //
  // Expects to be called on the UI thread.
  static void CreateAndBind(
      mojo::PendingRemote<network::mojom::RestrictedCookieManager>
          underlying_rcm,
      bool is_service_worker,
      int process_id,
      int frame_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      AwCookieAccessPolicy* aw_cookie_access_policy);

  AwProxyingRestrictedCookieManager(const AwProxyingRestrictedCookieManager&) =
      delete;
  AwProxyingRestrictedCookieManager& operator=(
      const AwProxyingRestrictedCookieManager&) = delete;

  ~AwProxyingRestrictedCookieManager() override;

  // network::mojom::RestrictedCookieManager interface:
  void GetAllForUrl(const GURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const url::Origin& top_frame_origin,
                    net::StorageAccessApiStatus storage_access_api_status,
                    network::mojom::CookieManagerGetOptionsPtr options,
                    bool is_ad_tagged,
                    bool force_disable_third_party_cookies,
                    GetAllForUrlCallback callback) override;
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          net::StorageAccessApiStatus storage_access_api_status,
                          net::CookieInclusionStatus status,
                          SetCanonicalCookieCallback callback) override;
  void AddChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojo::PendingRemote<network::mojom::CookieChangeListener> listener,
      AddChangeListenerCallback callback) override;

  void SetCookieFromString(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      const std::string& cookie,
      SetCookieFromStringCallback callback) override;

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        net::StorageAccessApiStatus storage_access_api_status,
                        bool get_version_shared_memory,
                        bool is_ad_tagged,
                        bool force_disable_third_party_cookies,
                        GetCookiesStringCallback callback) override;

  void CookiesEnabledFor(const GURL& url,
                         const net::SiteForCookies& site_for_cookies,
                         const url::Origin& top_frame_origin,
                         net::StorageAccessApiStatus storage_access_api_status,
                         CookiesEnabledForCallback callback) override;

  // This one is internal.
  net::NetworkDelegate::PrivacySetting AllowCookies(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      net::StorageAccessApiStatus storage_access_api_status) const;

 private:
  AwProxyingRestrictedCookieManager(
      mojo::PendingRemote<network::mojom::RestrictedCookieManager>
          underlying_restricted_cookie_manager,
      bool is_service_worker,
      const std::optional<const content::GlobalRenderFrameHostToken>&
          global_frame_token,
      AwCookieAccessPolicy* cookie_access_policy);

  static void CreateAndBindOnIoThread(
      mojo::PendingRemote<network::mojom::RestrictedCookieManager>
          underlying_rcm,
      bool is_service_worker,
      const std::optional<const content::GlobalRenderFrameHostToken>&
          global_frame_token,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
      AwCookieAccessPolicy* cookie_access_policy);

  mojo::Remote<network::mojom::RestrictedCookieManager>
      underlying_restricted_cookie_manager_;
  bool is_service_worker_;
  std::optional<const content::GlobalRenderFrameHostToken> global_frame_token_;

  raw_ref<AwCookieAccessPolicy> cookie_access_policy_;

  base::WeakPtrFactory<AwProxyingRestrictedCookieManager> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_PROXYING_RESTRICTED_COOKIE_MANAGER_H_
