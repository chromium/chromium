// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/common/aw_features.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace android_webview {

// static
void AwRenderViewHostExt::BindFrameHost(
    mojo::PendingAssociatedReceiver<mojom::FrameHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents)
    return;
  auto* aw_rvh_ext = aw_contents->render_view_host_ext();
  if (!aw_rvh_ext)
    return;
  aw_rvh_ext->frame_host_receivers_.Bind(rfh, std::move(receiver));
}

AwRenderViewHostExt::AwRenderViewHostExt(AwRenderViewHostExtClient* client,
                                         content::WebContents* contents)
    : content::WebContentsObserver(contents),
      client_(client),
      frame_host_receivers_(contents, this) {
  DCHECK(client_);
}

AwRenderViewHostExt::~AwRenderViewHostExt() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void AwRenderViewHostExt::DocumentHasImages(DocumentHasImagesResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents()->GetRenderViewHost()) {
    std::move(result).Run(false);
    return;
  }

  if (auto* local_main_frame_remote = GetLocalMainFrameRemote()) {
    local_main_frame_remote->DocumentHasImage(std::move(result));
  } else {
    // Still have to respond to the API call WebView#docuemntHasImages.
    // Otherwise the listener of the response may be starved.
    std::move(result).Run(false);
  }
}

void AwRenderViewHostExt::RequestNewHitTestDataAt(
    const gfx::PointF& touch_center,
    const gfx::SizeF& touch_area) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the new hit testing approach for touch start is enabled we just early
  // return.
  if (base::FeatureList::IsEnabled(
          features::kWebViewHitTestInBlinkOnTouchStart)) {
    return;
  }

  // The following code is broken for OOPIF and fenced frames. The hit testing
  // feature for touch start replaces this code and works correctly in those
  // scenarios. For mitigating risk we've put the old code behind a feature
  // flag.
  //
  // We only need to get blink::WebView on the renderer side to invoke the
  // blink hit test Mojo method, so sending this message via LocalMainFrame
  // interface is enough.
  if (auto* local_main_frame_remote = GetLocalMainFrameRemote())
    local_main_frame_remote->HitTest(touch_center, touch_area);
}

mojom::HitTestDataPtr AwRenderViewHostExt::TakeLastHitTestData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::move(last_hit_test_data_);
}

void AwRenderViewHostExt::SetTextZoomFactor(float factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* local_main_frame_remote = GetLocalMainFrameRemote())
    local_main_frame_remote->SetTextZoomFactor(factor);
}

void AwRenderViewHostExt::ResetScrollAndScaleState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* local_main_frame_remote = GetLocalMainFrameRemote())
    local_main_frame_remote->ResetScrollAndScaleState();
}

void AwRenderViewHostExt::SetInitialPageScale(double page_scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* local_main_frame_remote = GetLocalMainFrameRemote())
    local_main_frame_remote->SetInitialPageScale(page_scale_factor);
}

void AwRenderViewHostExt::SetWillSuppressErrorPage(bool suppress) {
  will_suppress_error_page_ = suppress;
}

void AwRenderViewHostExt::SmoothScroll(int target_x,
                                       int target_y,
                                       base::TimeDelta duration) {
  if (auto* local_main_frame_remote = GetLocalMainFrameRemote())
    local_main_frame_remote->SmoothScroll(target_x, target_y, duration);
}

void AwRenderViewHostExt::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (will_suppress_error_page_)
    navigation_handle->SetSilentlyIgnoreErrors();
}

void AwRenderViewHostExt::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->HasCommitted())
    return;

  // Only record a visit if the navigation affects user-facing session history
  // (i.e. it occurs in the primary frame tree).
  if (navigation_handle->IsInPrimaryMainFrame() ||
      (navigation_handle->GetParentFrame() &&
       navigation_handle->GetParentFrame()->GetPage().IsPrimary() &&
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    AwBrowserContext::FromWebContents(web_contents())
        ->AddVisitedURLs(navigation_handle->GetRedirectChain());
  }
}

void AwRenderViewHostExt::OnPageScaleFactorChanged(float page_scale_factor) {
  client_->OnWebLayoutPageScaleFactorChanged(page_scale_factor);
}

void AwRenderViewHostExt::UpdateHitTestData(
    mojom::HitTestDataPtr hit_test_data) {
  content::RenderFrameHost* render_frame_host =
      frame_host_receivers_.GetCurrentTargetFrame();
  // Make sense from any frame of the active frame tree, because a focused
  // node could be in either the mainframe or a subframe.
  if (!render_frame_host->IsActive())
    return;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_hit_test_data_ = std::move(hit_test_data);
}

void AwRenderViewHostExt::ContentsSizeChanged(const gfx::Size& contents_size) {
  content::RenderFrameHost* render_frame_host =
      frame_host_receivers_.GetCurrentTargetFrame();

  // Only makes sense coming from the main frame of the current frame tree.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  client_->OnWebLayoutContentsSizeChanged(contents_size);
}

void AwRenderViewHostExt::ShouldOverrideUrlLoading(
    const std::u16string& url,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    ShouldOverrideUrlLoadingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool ignore_navigation = false;
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents());
  if (client) {
    if (!client->ShouldOverrideUrlLoading(
            url, has_user_gesture, is_redirect, is_main_frame,
            net::HttpRequestHeaders(), &ignore_navigation)) {
      // If the shouldOverrideUrlLoading call caused a java exception we should
      // always return immediately here!
      return;
    }
  } else {
    LOG(WARNING) << "Failed to find the associated render view host for url: "
                 << url;
  }

  std::move(callback).Run(ignore_navigation);
}

mojom::LocalMainFrame* AwRenderViewHostExt::GetLocalMainFrameRemote() {
  // Validate the local main frame matches what we have stored for the current
  // main frame. Previously `local_main_frame_remote_` was adjusted in
  // RenderFrameCreated/RenderFrameHostChanged events but the timings of when
  // this class gets called vs others using this class might cause a TOU
  // problem, so we validate it each time before use.
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId main_frame_id = main_frame->GetGlobalId();
  if (main_frame_global_id_ == main_frame_id) {
    return local_main_frame_remote_.get();
  }

  local_main_frame_remote_.reset();

  // Avoid accessing GetRemoteAssociatedInterfaces until the renderer is
  // created.
  if (!main_frame->IsRenderFrameLive()) {
    main_frame_global_id_ = content::GlobalRenderFrameHostId();
    return nullptr;
  }

  main_frame_global_id_ = main_frame_id;
  main_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      local_main_frame_remote_.BindNewEndpointAndPassReceiver());
  return local_main_frame_remote_.get();
}

}  // namespace android_webview
