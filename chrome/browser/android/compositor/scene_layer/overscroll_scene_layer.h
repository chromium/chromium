// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_

#include "base/android/jni_weak_ref.h"
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
                       const base::android::JavaParamRef<jobject>& jobj,
                       const base::android::JavaParamRef<jobject>& jwindow);
  ~OverscrollSceneLayer() override;

  void Prepare(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,

               jfloat start_x,
               jfloat start_y,
               jint width,
               jint height);
  jboolean Update(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& object,
                  const base::android::JavaParamRef<jobject>& jresource_manager,
                  jfloat accumulated_overscroll_x,
                  jfloat delta_x);
  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);
  void OnReset(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  void SetNeedsAnimate();

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override {}
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

 private:
  // OverscrollGlowClient implementation.
  std::unique_ptr<ui::EdgeEffect> CreateEdgeEffect() override;

  ui::WindowAndroid* const window_;
  std::unique_ptr<ui::OverscrollGlow> glow_effect_;
  ui::ResourceManager* resource_manager_ = nullptr;

  gfx::Vector2dF start_pos_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollSceneLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_OVERSCROLL_SCENE_LAYER_H_
