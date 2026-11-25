// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_

#include <string>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace context_hub {

// Result of resolving a clicked element in the accessibility tree.
struct ClickTargetResolution {
  enum class Status {
    // Resolved a recordable interactive element or text field (`label` is
    // non-empty).
    kResolved,
    // The accessibility tree for the clicked same-origin frame is not
    // available yet (e.g., immediately after a fresh navigation before initial
    // serialization arrives).
    kTreeUnavailable,
    // The click was rejected by frame/origin eligibility checks or hit a
    // non-recordable node (e.g., blank space, password field, disabled
    // control, scrollbar, or oversized container).
    kIgnored,
  };

  static ClickTargetResolution Resolved(std::string label);
  static ClickTargetResolution TreeUnavailable();
  static ClickTargetResolution Ignored();

  ClickTargetResolution(const ClickTargetResolution&) = delete;
  ClickTargetResolution& operator=(const ClickTargetResolution&) = delete;
  ClickTargetResolution(ClickTargetResolution&&) noexcept;
  ClickTargetResolution& operator=(ClickTargetResolution&&) noexcept;
  ~ClickTargetResolution();

  bool operator==(const ClickTargetResolution&) const = default;

  Status status;
  std::string label;

 private:
  ClickTargetResolution(Status status, std::string label);
};

// Resolves a sanitized human-readable label (e.g., "Submit" or "button") for
// an interactive element or text field at `point_in_widget_dips` within
// `target_rfh`'s accessibility tree when `target_rfh` is same-origin with
// `main_rfh`. Returns `Status::kTreeUnavailable` if an eligible same-origin
// frame's accessibility tree is not populated yet, or `Status::kIgnored` if
// `target_rfh` is null or ineligible or if the clicked node should not be
// recorded.
//
// Requires `ui::AXMode::kWebContents` to populate semantic controls and text
// fields, and `ui::AXMode::kExtendedProperties` for Blink to serialize
// `IntAttribute::kDefaultActionVerb` (for custom/delegated clickable elements
// such as `<div onclick>`) and `BoolAttribute::kModal`.
ClickTargetResolution ResolveClickedElementLabel(
    content::RenderFrameHost& main_rfh,
    content::RenderFrameHost* target_rfh,
    const gfx::PointF& point_in_widget_dips);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_AX_CLICK_TARGET_RESOLVER_H_
