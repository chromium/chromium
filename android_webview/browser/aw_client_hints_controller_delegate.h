// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include "components/prefs/pref_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {
class EnabledClientHints;
struct UserAgentMetadata;
}  // namespace blink

namespace content {
class RenderFrameHost;
}  // namespace content

namespace network {
class NetworkQualityTracker;
}  // namespace network

namespace url {
class GURL;
class Origin;
}  // namespace url

namespace android_webview {

extern const char kAndroidWebViewProductName[];

namespace prefs {
extern const char kClientHintsCachedPerOriginMap[];
}  // namespace prefs

// Lifetime: Profile
class AwClientHintsControllerDelegate
    : public content::ClientHintsControllerDelegate {
 public:
  // The supplied PrefService should be the PrefService tied to the
  // AwBrowserContext (profile), not the browser-wide local_state. Note that
  // this is not comparible to the PrefService used by
  // client_hints::ClientHints::ClientHints() - these PrefServices are used for
  // different purposes.
  explicit AwClientHintsControllerDelegate(PrefService* context_pref_service);
  ~AwClientHintsControllerDelegate() override;

  // Add an unique brand to the brand list to allow users distinguish Android
  // and Android WebView using user-agent client hints.
  // `only_low_entropy_ch` indicates whether components should only populate the
  // low entropy client hints.
  static blink::UserAgentMetadata GetUserAgentMetadataOverrideBrand(
      bool only_low_entropy_ch = false);

  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url,
                           content::RenderFrameHost* parent_rfh) override;

  bool AreThirdPartyCookiesBlocked(const GURL& url,
                                   content::RenderFrameHost* rfh) override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void PersistClientHints(const url::Origin& primary_origin,
                          content::RenderFrameHost* parent_rfh,
                          const std::vector<network::mojom::WebClientHintsType>&
                              client_hints) override;

  void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>& hints) override;

  void ClearAdditionalClientHints() override;

  void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) override;

  gfx::Size GetMostRecentMainFrameViewportSize() override;

 private:
  std::vector<network::mojom::WebClientHintsType> additional_hints_;
  raw_ptr<PrefService> context_pref_service_;
  gfx::Size viewport_size_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
