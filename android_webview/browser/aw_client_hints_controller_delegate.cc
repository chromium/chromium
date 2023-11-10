// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
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

// Android WebView product name for building the default user agent string.
const char kAndroidWebViewProductName[] = "Android WebView";

namespace prefs {
const char kClientHintsCachedPerOriginMap[] =
    "aw_client_hints_cached_per_origin_map";
}  // namespace prefs

AwClientHintsControllerDelegate::AwClientHintsControllerDelegate(
    PrefService* context_pref_service)
    : context_pref_service_(context_pref_service) {}

AwClientHintsControllerDelegate::~AwClientHintsControllerDelegate() = default;

blink::UserAgentMetadata
AwClientHintsControllerDelegate::GetUserAgentMetadataOverrideBrand(
    bool only_low_entropy_ch) {
  // embedder_support::GetUserAgentMetadata() can accept a browser local_state
  // PrefService argument, but doesn't need one. Either way, it shouldn't be the
  // context_pref_service_ that this class holds.
  auto metadata = embedder_support::GetUserAgentMetadata(only_low_entropy_ch);
  std::string major_version = version_info::GetMajorVersionNumber();

  // Use the major version number as a greasing seed
  int major_version_number;
  bool parse_result = base::StringToInt(major_version, &major_version_number);
  DCHECK(parse_result);

  // The old grease brand algorithm will removed soon, we should always use the
  // updated algorithm.
  bool enable_updated_grease_by_policy = true;
  // Regenerate the brand version lists with Android WebView product name.
  metadata.brand_version_list = embedder_support::GenerateBrandVersionList(
      major_version_number, kAndroidWebViewProductName, major_version,
      std::nullopt, std::nullopt, enable_updated_grease_by_policy,
      blink::UserAgentBrandVersionType::kMajorVersion);

  if (!only_low_entropy_ch) {
    metadata.brand_full_version_list =
        embedder_support::GenerateBrandVersionList(
            major_version_number, kAndroidWebViewProductName,
            metadata.full_version, std::nullopt, std::nullopt,
            enable_updated_grease_by_policy,
            blink::UserAgentBrandVersionType::kFullVersion);
  }

  return metadata;
}

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
  if (context_pref_service_->HasPrefPath(
          prefs::kClientHintsCachedPerOriginMap)) {
    auto* const client_hints_list =
        context_pref_service_->GetDict(prefs::kClientHintsCachedPerOriginMap)
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
  return GetUserAgentMetadataOverrideBrand();
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
  if (context_pref_service_->HasPrefPath(
          prefs::kClientHintsCachedPerOriginMap)) {
    ch_per_origin =
        context_pref_service_->GetDict(prefs::kClientHintsCachedPerOriginMap)
            .Clone();
  }
  ch_per_origin.Set(primary_origin.Serialize(), std::move(client_hints_list));
  context_pref_service_->SetDict(prefs::kClientHintsCachedPerOriginMap,
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
