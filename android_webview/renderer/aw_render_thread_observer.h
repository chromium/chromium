// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_THREAD_OBSERVER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_THREAD_OBSERVER_H_

#include "android_webview/common/mojom/renderer.mojom.h"
#include "base/compiler_specific.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace android_webview {

// A RenderThreadObserver implementation used for handling android_webview
// specific render-process wide IPC messages.
class AwRenderThreadObserver : public content::RenderThreadObserver,
                               public mojom::Renderer {
 public:
  AwRenderThreadObserver();
  ~AwRenderThreadObserver() override;

  // content::RenderThreadObserver implementation.
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

 private:
  // mojom::Renderer overrides:
  void ClearCache() override;
  void SetJsOnlineProperty(bool network_up) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::Renderer> receiver);

  mojo::AssociatedReceiver<mojom::Renderer> receiver_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_THREAD_OBSERVER_H_
