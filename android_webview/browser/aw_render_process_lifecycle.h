// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_LIFECYCLE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_LIFECYCLE_H_

#include <string_view>

namespace content {
class RenderProcessHost;
}

namespace android_webview {

// Describes where a renderer process currently sits in WebView's renderer
// lifecycle. WebView keeps renderers around after their last WebView goes
// away so that they can be reused.
// LINT.IfChange(AwRenderProcessLifecycleState)
enum class AwRenderProcessLifecycleState {
  // No RenderProcessHost is available for the renderer, e.g. because the
  // process exited before its state could be looked up.
  kUnknown,
  // A pre-warmed spare renderer that has never hosted a WebView.
  kSpareNeverUsed,
  // A renderer that was previously used but currently has no AwContents, and
  // is being kept alive so the next WebView can reuse it. See
  // AwRenderProcessKeepAlive.
  kSpareKeptAlive,
  // A renderer that is in use.
  kActive,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/memory/histograms.xml:AwRenderProcessLifecycleState)

// Returns the name of `state`, e.g. "Active".
std::string_view AwRenderProcessLifecycleStateToString(
    AwRenderProcessLifecycleState state);

// Returns the lifecycle state of `host`. Must be called on the UI thread.
AwRenderProcessLifecycleState GetAwRenderProcessLifecycleState(
    content::RenderProcessHost* host);

// Returns the name of `host`'s current lifecycle state.
// Must be called on the UI thread.
std::string_view GetAwRenderProcessLifecycleStateString(
    content::RenderProcessHost* host);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_LIFECYCLE_H_
