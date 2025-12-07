// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_frame_observer.h"

#include "base/android/orderfile/orderfile_buildflags.h"

#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
#include "base/android/orderfile/orderfile_instrumentation.h"  // nogncheck
#endif

namespace android_webview {

AwRenderFrameObserver::AwRenderFrameObserver(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

AwRenderFrameObserver::~AwRenderFrameObserver() = default;

void AwRenderFrameObserver::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  // Ensures that StartDelayedDump is called only once for the first navigation
  // during the WebView Renderer process lifetime.
  [[maybe_unused]] static bool call_once = [] {
    base::android::orderfile::StartDelayedDump();
    return true;
  }();
#endif
}

void AwRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace android_webview
