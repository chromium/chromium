// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_

#include <optional>
#include <string>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace context_hub {

// Resolves a sanitized human-readable label (e.g., "Submit" or "button") for
// an interactive element or text field at `point_in_widget_dips` within
// `target_rfh`'s accessibility tree when `target_rfh` is same-origin with
// `main_rfh`. Returns `std::nullopt` if `target_rfh` is null or ineligible, if
// the accessibility tree is unavailable, or if the clicked node should not be
// recorded.
//
// Requires `ui::AXMode::kWebContents` to populate semantic controls and text
// fields, and `ui::AXMode::kExtendedProperties` for Blink to serialize
// `IntAttribute::kDefaultActionVerb` (for custom/delegated clickable elements
// such as `<div onclick>`) and `BoolAttribute::kModal`.
std::optional<std::string> ResolveClickedElementLabel(
    content::RenderFrameHost& main_rfh,
    content::RenderFrameHost* target_rfh,
    const gfx::PointF& point_in_widget_dips);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_
