// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ANDROID_OFFSCREEN_RENDERING_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ACTOR_ANDROID_OFFSCREEN_RENDERING_MANAGER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/slim/layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class WebContents;
class Compositor;
}  // namespace content

namespace ui {
class WindowAndroid;
}
namespace actor {

// Manages an offscreen compositor and window for rendering WebContents
// outside of the normal view hierarchy. This is primarily used for Actor
// tasks that need to continue rendering while the activity is in a
// background state (like Picture-in-Picture).
class OffscreenRenderingManagerAndroid : public content::CompositorClient {
 public:
  OffscreenRenderingManagerAndroid(ui::WindowAndroid* window,
                                   int width,
                                   int height);

  ~OffscreenRenderingManagerAndroid() override;

  // Destroys the manager and its associated compositor.
  void Destroy(JNIEnv* env);

  // Starts offscreen rendering for the given WebContents by attaching its
  // layer to the offscreen compositor's root layer.
  void StartOffscreenRendering(JNIEnv* env,
                               content::WebContents* web_contents,
                               int width,
                               int height);

  // Stops offscreen rendering for the given WebContents.
  void StopOffscreenRendering(JNIEnv* env, content::WebContents* web_contents);

  // content::CompositorClient implementation
  void UpdateLayerTreeHost() override {}
  void DidSwapBuffers(const gfx::Size& swap_size) override {}
  void DidSwapFrame(int pending_frames) override {}
  void RecreateSurface() override {}

 private:
  // The offscreen compositor used for rendering.
  std::unique_ptr<content::Compositor> offscreen_compositor_;
  // The root layer of the offscreen compositor.
  scoped_refptr<cc::slim::Layer> root_layer_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ANDROID_OFFSCREEN_RENDERING_MANAGER_ANDROID_H_
