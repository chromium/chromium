// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/common/render_view_messages.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace android_webview {

AwRenderViewHostExt::AwRenderViewHostExt(AwRenderViewHostExtClient* client,
                                         content::WebContents* contents)
    : content::WebContentsObserver(contents),
      client_(client),
      background_color_(SK_ColorWHITE),
      has_new_hit_test_data_(false),
      frame_host_receivers_(contents, this) {
  DCHECK(client_);
}

AwRenderViewHostExt::~AwRenderViewHostExt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AwRenderViewHostExt::DocumentHasImages(DocumentHasImagesResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents()->GetRenderViewHost()) {
    std::move(result).Run(false);
    return;
  }

  if (local_main_frame_remote_) {
    local_main_frame_remote_->DocumentHasImage(std::move(result));
  } else {
    // Still have to respond to the API call WebView#docuemntHasImages.
    // Otherwise the listener of the response may be starved.
    std::move(result).Run(false);
  }
}

bool AwRenderViewHostExt::HasNewHitTestData() const {
  return has_new_hit_test_data_;
}

void AwRenderViewHostExt::MarkHitTestDataRead() {
  has_new_hit_test_data_ = false;
}

void AwRenderViewHostExt::RequestNewHitTestDataAt(
    const gfx::PointF& touch_center,
    const gfx::SizeF& touch_area) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We only need to get blink::WebView on the renderer side to invoke the
  // blink hit test Mojo method, so sending this message via LocalMainFrame
  // interface is enough.
  if (local_main_frame_remote_)
    local_main_frame_remote_->HitTest(touch_center, touch_area);
}

const mojom::HitTestData& AwRenderViewHostExt::GetLastHitTestData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *last_hit_test_data_;
}

void AwRenderViewHostExt::SetTextZoomFactor(float factor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_main_frame_remote_)
    local_main_frame_remote_->SetTextZoomFactor(factor);
}

void AwRenderViewHostExt::ResetScrollAndScaleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_main_frame_remote_)
    local_main_frame_remote_->ResetScrollAndScaleState();
}

void AwRenderViewHostExt::SetInitialPageScale(double page_scale_factor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_main_frame_remote_)
    local_main_frame_remote_->SetInitialPageScale(page_scale_factor);
}

void AwRenderViewHostExt::SetBackgroundColor(SkColor c) {
  if (background_color_ == c)
    return;
  background_color_ = c;
  if (local_main_frame_remote_) {
    local_main_frame_remote_->SetBackgroundColor(background_color_);
  }
}

void AwRenderViewHostExt::SetWillSuppressErrorPage(bool suppress) {
  will_suppress_error_page_ = suppress;
}

void AwRenderViewHostExt::SmoothScroll(int target_x,
                                       int target_y,
                                       base::TimeDelta duration) {
  if (local_main_frame_remote_)
    local_main_frame_remote_->SmoothScroll(target_x, target_y, duration);
}

void AwRenderViewHostExt::RenderFrameCreated(
    content::RenderFrameHost* frame_host) {
  if (!frame_host->GetParent()) {
    local_main_frame_remote_.reset();
    frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        local_main_frame_remote_.BindNewEndpointAndPassReceiver());

    local_main_frame_remote_->SetBackgroundColor(background_color_);
  }
}

void AwRenderViewHostExt::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (will_suppress_error_page_)
    navigation_handle->SetSilentlyIgnoreErrors();
}

void AwRenderViewHostExt::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->HasCommitted() ||
      (!navigation_handle->IsInMainFrame() &&
       !navigation_handle->HasSubframeNavigationEntryCommitted()))
    return;

  AwBrowserContext::FromWebContents(web_contents())
      ->AddVisitedURLs(navigation_handle->GetRedirectChain());
}

void AwRenderViewHostExt::OnPageScaleFactorChanged(float page_scale_factor) {
  client_->OnWebLayoutPageScaleFactorChanged(page_scale_factor);
}

void AwRenderViewHostExt::UpdateHitTestData(
    mojom::HitTestDataPtr hit_test_data) {
  content::RenderFrameHost* main_frame_host =
      frame_host_receivers_.GetCurrentTargetFrame();
  while (main_frame_host->GetParent())
    main_frame_host = main_frame_host->GetParent();

  // Make sense from any frame of the current frame tree, because a focused
  // node could be in either the mainframe or a subframe.
  if (main_frame_host != web_contents()->GetMainFrame())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_hit_test_data_ = std::move(hit_test_data);
  has_new_hit_test_data_ = true;
}

void AwRenderViewHostExt::ContentsSizeChanged(const gfx::Size& contents_size) {
  content::RenderFrameHost* render_frame_host =
      frame_host_receivers_.GetCurrentTargetFrame();

  // Only makes sense coming from the main frame of the current frame tree.
  if (render_frame_host != web_contents()->GetMainFrame())
    return;

  client_->OnWebLayoutContentsSizeChanged(contents_size);
}

}  // namespace android_webview
