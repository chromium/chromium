// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "base/notreached.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

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
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

bool AwClientHintsControllerDelegate::IsJavaScriptAllowed(
    const GURL& url,
    content::RenderFrameHost* parent_rfh) {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
  return false;
}

bool AwClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url) {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
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
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
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
