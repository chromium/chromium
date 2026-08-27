// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_frame_observer.h"

#include "android_webview/common/aw_features.h"
#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

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

void AwRenderFrameObserver::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // ReadyToCommitNavigation is only called for navigations that have succeeded
  // and will commit. It does not fire for same-document navigations.
  if (base::FeatureList::IsEnabled(
          features::kWebViewBoostRendererPriorityOnNavigation) &&
      render_frame()->IsMainFrame() && !IsPrerendering() &&
      !navigation_priority_lease_) {
    // ThreadType::kAudioProcessing is currently not used for Android. We are
    // using it as an intermediate solution to evaluate the impact of boosting
    // the renderer main thread priority without introducing a new thread type.
    navigation_priority_lease_.emplace(base::ThreadType::kAudioProcessing);
    // Reset priority in 10 seconds in case DidDispatchDOMContentLoadedEvent
    // does not fire.
    reset_priority_timer_.Start(FROM_HERE, base::Seconds(10), this,
                                &AwRenderFrameObserver::ResetPriority);
  }
}

void AwRenderFrameObserver::DidDispatchDOMContentLoadedEvent() {
  if (base::FeatureList::IsEnabled(
          features::kWebViewBoostRendererPriorityOnNavigation) &&
      render_frame()->IsMainFrame()) {
    ResetPriority();
  }
}

void AwRenderFrameObserver::ResetPriority() {
  reset_priority_timer_.Stop();
  navigation_priority_lease_.reset();
}

bool AwRenderFrameObserver::IsPrerendering() const {
  auto* frame = render_frame();
  return frame && frame->GetWebFrame() &&
         frame->GetWebFrame()->GetDocument().IsPrerendering();
}

void AwRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace android_webview
