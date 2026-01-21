// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/overscroll_glow.h"
#include "ui/android/window_android_observer.h"

namespace ui {
class ResourceManager;
class WindowAndroid;
}  // namespace ui

namespace android {

// Native layer that handles the overscroll glow animation effect for
// gesture navigation.
class OverscrollSceneLayer : public SceneLayer,
                             public ui::WindowAndroidObserver,
                             public ui::OverscrollGlowClient {
 public:
  OverscrollSceneLayer(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj,
                       const base::android::JavaRef<jobject>& jwindow);

  OverscrollSceneLayer(const OverscrollSceneLayer&) = delete;
  OverscrollSceneLayer& operator=(const OverscrollSceneLayer&) = delete;

  ~OverscrollSceneLayer() override;

  void Prepare(JNIEnv* env,
               float start_x,
               float start_y,
               int32_t width,
               int32_t height);
  bool Update(JNIEnv* env,
              const base::android::JavaRef<jobject>& jresource_manager,
              float accumulated_overscroll_x,
              float delta_x);
  void SetContentTree(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jcontent_tree);
  void OnReset(JNIEnv* env);

  void SetNeedsAnimate();

  // ui::WindowAndroidObserver implementation.
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

 private:
  // OverscrollGlowClient implementation.
  std::unique_ptr<ui::EdgeEffect> CreateEdgeEffect() override;

  const raw_ptr<ui::WindowAndroid, DanglingUntriaged> window_;
  std::unique_ptr<ui::OverscrollGlow> glow_effect_;
  raw_ptr<ui::ResourceManager> resource_manager_ = nullptr;

  gfx::Vector2dF start_pos_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_
