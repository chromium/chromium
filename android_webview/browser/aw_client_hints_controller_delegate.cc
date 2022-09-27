// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_client_hints_controller_delegate.h"

#include "base/notreached.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

AwClientHintsControllerDelegate::AwClientHintsControllerDelegate() {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}
AwClientHintsControllerDelegate::~AwClientHintsControllerDelegate() {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

network::NetworkQualityTracker*
AwClientHintsControllerDelegate::GetNetworkQualityTracker() {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
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
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
  return blink::UserAgentMetadata();
}

void AwClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

void AwClientHintsControllerDelegate::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>&) {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

void AwClientHintsControllerDelegate::ClearAdditionalClientHints() {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

void AwClientHintsControllerDelegate::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
}

gfx::Size
AwClientHintsControllerDelegate::GetMostRecentMainFrameViewportSize() {
  // TODO(crbug.com/921655): Actually implement function.
  NOTIMPLEMENTED();
  return gfx::Size(0, 0);
}

}  // namespace android_webview
