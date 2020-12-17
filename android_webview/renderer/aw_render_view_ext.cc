// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/common/mojom/frame.mojom.h"
#include "android_webview/common/render_view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace android_webview {

AwRenderViewExt::AwRenderViewExt(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  DCHECK(render_view != nullptr);
}

AwRenderViewExt::~AwRenderViewExt() {}

// static
void AwRenderViewExt::RenderViewCreated(content::RenderView* render_view) {
  new AwRenderViewExt(render_view);  // |render_view| takes ownership.
}

void AwRenderViewExt::DidCommitCompositorFrame() {
  UpdateContentsSize();
}

void AwRenderViewExt::DidUpdateMainFrameLayout() {
  // The size may have changed.
  needs_contents_size_update_ = true;
}

void AwRenderViewExt::OnDestruct() {
  delete this;
}

void AwRenderViewExt::UpdateContentsSize() {
  blink::WebView* webview = render_view()->GetWebView();
  content::RenderFrame* main_render_frame = render_view()->GetMainRenderFrame();

  // Even without out-of-process iframes, we now create RemoteFrames for the
  // main frame when you navigate cross-process, to create a placeholder in the
  // old process. This is necessary to support things like postMessage across
  // windows that have references to each other. The RemoteFrame will
  // immediately go away if there aren't any active frames left in the old
  // process. RenderView's main frame pointer will become null in the old
  // process when it is no longer the active main frame.
  if (!webview || !main_render_frame)
    return;

  if (!needs_contents_size_update_)
    return;
  needs_contents_size_update_ = false;

  gfx::Size contents_size = main_render_frame->GetWebFrame()->DocumentSize();

  // Fall back to contentsPreferredMinimumSize if the mainFrame is reporting a
  // 0x0 size (this happens during initial load).
  if (contents_size.IsEmpty()) {
    contents_size = webview->ContentsPreferredMinimumSize();
  }

  if (contents_size == last_sent_contents_size_)
    return;

  last_sent_contents_size_ = contents_size;

  mojo::AssociatedRemote<mojom::FrameHost> frame_host_remote;
  main_render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &frame_host_remote);
  frame_host_remote->ContentsSizeChanged(contents_size);
}

}  // namespace android_webview
