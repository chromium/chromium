// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

namespace prefs {
const char kClientHintsCachedPerOriginMap[] =
    "aw_client_hints_cached_per_origin_map";
}  // namespace prefs

AwClientHintsControllerDelegate::AwClientHintsControllerDelegate(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

AwClientHintsControllerDelegate::~AwClientHintsControllerDelegate() = default;

network::NetworkQualityTracker*
AwClientHintsControllerDelegate::GetNetworkQualityTracker() {
  // Android WebViews lack a Network Quality Tracker.
  return nullptr;
}

void AwClientHintsControllerDelegate::GetAllowedClientHintsFromSource(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) {
  // Ensure this origin can have hints stored.
  const GURL url = origin.GetURL();
  if (!url.is_valid() || !network::IsUrlPotentiallyTrustworthy(url)) {
    return;
  }

  // Add stored hints to the enabled list.
  if (pref_service_->HasPrefPath(prefs::kClientHintsCachedPerOriginMap)) {
    auto* const client_hints_list =
        pref_service_->GetDict(prefs::kClientHintsCachedPerOriginMap)
            .FindList(origin.Serialize());
    if (client_hints_list) {
      for (const auto& client_hint : *client_hints_list) {
        DCHECK(client_hint.is_int());
        network::mojom::WebClientHintsType client_hint_mojo =
            static_cast<network::mojom::WebClientHintsType>(
                client_hint.GetInt());
        if (network::mojom::IsKnownEnumValue(client_hint_mojo)) {
          client_hints->SetIsEnabled(client_hint_mojo, true);
        }
      }
    }
  }

  // Add additional hints to the enabled list.
  for (auto hint : additional_hints_) {
    client_hints->SetIsEnabled(hint, true);
  }
}

bool AwClientHintsControllerDelegate::IsJavaScriptAllowed(
    const GURL& url,
    content::RenderFrameHost* parent_rfh) {
  // Javascript can only be disabled per-frame, so if we're pre-loading
  // and/or there is no frame Javascript is considered enabled.
  if (!parent_rfh) {
    return true;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          parent_rfh->GetOutermostMainFrame());
  if (!web_contents) {
    return true;
  }
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents) {
    return true;
  }
  return aw_contents->IsJavaScriptAllowed();
}

bool AwClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  // This function is related to an OT for the Sec-CH-UA-Reduced client hint
  // and as this doesn't affect WebView at the moment, we have no reason to
  // implement it.
  return false;
}

blink::UserAgentMetadata
AwClientHintsControllerDelegate::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata(pref_service_);
}

void AwClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  // Ensure this origin can have hints stored and check the number of hints.
  const GURL primary_url = primary_origin.GetURL();
  if (!primary_url.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(primary_url)) {
    return;
  }
  if (!IsJavaScriptAllowed(primary_url, parent_rfh)) {
    return;
  }
  if (client_hints.size() >
      (static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) +
       1)) {
    return;
  }

  // Assemble and store the list if no issues.
  const auto& persistence_started = base::TimeTicks::Now();
  base::Value::List client_hints_list;
  client_hints_list.reserve(client_hints.size());
  for (const auto& entry : client_hints) {
    client_hints_list.Append(static_cast<int>(entry));
  }
  base::Value::Dict ch_per_origin;
  if (pref_service_->HasPrefPath(prefs::kClientHintsCachedPerOriginMap)) {
    ch_per_origin =
        pref_service_->GetDict(prefs::kClientHintsCachedPerOriginMap).Clone();
  }
  ch_per_origin.Set(primary_origin.Serialize(), std::move(client_hints_list));
  pref_service_->SetDict(prefs::kClientHintsCachedPerOriginMap,
                         std::move(ch_per_origin));
  network::LogClientHintsPersistenceMetrics(persistence_started,
                                            client_hints.size());
}

void AwClientHintsControllerDelegate::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& hints) {
  additional_hints_ = hints;
}

void AwClientHintsControllerDelegate::ClearAdditionalClientHints() {
  additional_hints_.clear();
}

void AwClientHintsControllerDelegate::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Size
AwClientHintsControllerDelegate::GetMostRecentMainFrameViewportSize() {
  return viewport_size_;
}

}  // namespace android_webview
