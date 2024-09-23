// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_view_ext.h"

#include <map>

#include "android_webview/common/mojom/frame.mojom.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace android_webview {

// Registry for blink::WebView => AwRenderViewExt lookups
typedef std::map<blink::WebView*, AwRenderViewExt*> ViewExtMap;
ViewExtMap* GetViewExtMap() {
  static base::NoDestructor<ViewExtMap> map;
  return map.get();
}

AwRenderViewExt::AwRenderViewExt(blink::WebView* web_view,
                                 bool created_by_renderer)
    : blink::WebViewObserver(web_view),
      created_by_renderer_(created_by_renderer) {
  DCHECK(web_view != nullptr);
  DCHECK(!base::Contains(*GetViewExtMap(), web_view));
  GetViewExtMap()->emplace(web_view, this);
}

AwRenderViewExt::~AwRenderViewExt() {
  // Remove myself from the blink::WebView => AwRenderViewExt register. Ideally,
  // we'd just use GetWebView() and erase by key. However, by this time the
  // GetWebView has already been cleared so we have to iterate over all
  // WebViews in the map and wipe the one(s) that point to this
  // AwRenderViewExt

  auto* map = GetViewExtMap();
  auto it = map->begin();
  while (it != map->end()) {
    if (it->second == this) {
      it = map->erase(it);
    } else {
      ++it;
    }
  }
}

// static
void AwRenderViewExt::WebViewCreated(blink::WebView* web_view,
                                     bool created_by_renderer) {
  new AwRenderViewExt(web_view,
                      created_by_renderer);  // |web_view| takes ownership.
}

// static
AwRenderViewExt* AwRenderViewExt::FromWebView(blink::WebView* web_view) {
  DCHECK(web_view != nullptr);
  auto iter = GetViewExtMap()->find(web_view);
  CHECK(GetViewExtMap()->end() != iter, base::NotFatalUntil::M130)
      << "AwRenderViewExt should always exist for a WebView";
  AwRenderViewExt* render_view_ext = iter->second;
  return render_view_ext;
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
  blink::WebView* webview = GetWebView();
  blink::WebFrame* main_frame = webview->MainFrame();

  // Even without out-of-process iframes, we now create RemoteFrames for the
  // main frame when you navigate cross-process, to create a placeholder in the
  // old process. This is necessary to support things like postMessage across
  // windows that have references to each other. The RemoteFrame will
  // immediately go away if there aren't any active frames left in the old
  // process.
  if (!main_frame->IsWebLocalFrame())
    return;

  if (!needs_contents_size_update_)
    return;
  needs_contents_size_update_ = false;

  gfx::Size contents_size = main_frame->ToWebLocalFrame()->DocumentSize();

  // Fall back to contentsPreferredMinimumSize if the mainFrame is reporting a
  // 0x0 size (this happens during initial load).
  if (contents_size.IsEmpty()) {
    contents_size = webview->ContentsPreferredMinimumSize();
  }

  if (contents_size == last_sent_contents_size_)
    return;

  last_sent_contents_size_ = contents_size;

  mojo::AssociatedRemote<mojom::FrameHost> frame_host_remote;
  content::RenderFrame::FromWebFrame(main_frame->ToWebLocalFrame())
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&frame_host_remote);
  frame_host_remote->ContentsSizeChanged(contents_size);
}

}  // namespace android_webview
