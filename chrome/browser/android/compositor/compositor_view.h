// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/layer_collections.h"
#include "cc/resources/ui_resource_client.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class Layer;
class SolidColorLayer;
}  // namespace cc

namespace content {
class Compositor;
}

namespace ui {
class WindowAndroid;
class ResourceManager;
class UIResourceProvider;
}  // namespace ui

namespace android {

class SceneLayer;
class TabContentManager;

class CompositorView : public content::CompositorClient,
                       public content::BrowserChildProcessObserver {
 public:
  CompositorView(JNIEnv* env,
                 jobject obj,
                 jboolean low_mem_device,
                 ui::WindowAndroid* window_android,
                 TabContentManager* tab_content_manager);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);

  ui::ResourceManager* GetResourceManager();
  base::android::ScopedJavaLocalRef<jobject> GetResourceManager(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);
  void SetNeedsComposite(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& object);
  void FinalizeLayers(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jobj);
  void SetLayoutBounds(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& object);
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& object);
  void SurfaceChanged(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object,
                      jint format,
                      jint width,
                      jint height,
                      bool can_be_used_with_surface_control,
                      const base::android::JavaParamRef<jobject>& surface);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);

  void SetOverlayVideoMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& object,
                           bool enabled);
  void SetOverlayImmersiveArMode(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      bool enabled);
  void SetSceneLayer(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& object,
                     const base::android::JavaParamRef<jobject>& jscene_layer);
  void SetCompositorWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& window_android);
  void CacheBackBufferForCurrentSurface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object);
  void EvictCachedBackBuffer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object);

  // CompositorClient implementation:
  void RecreateSurface() override;
  void UpdateLayerTreeHost() override;
  void DidSwapFrame(int pending_frames) override;
  void DidSwapBuffers(const gfx::Size& swap_size) override;
  ui::UIResourceProvider* GetUIResourceProvider();

 private:
  ~CompositorView() override;

  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void SetBackground(bool visible, SkColor color);
  void OnSurfaceControlFeatureStatusUpdate(bool available);

  base::android::ScopedJavaGlobalRef<jobject> obj_;
  std::unique_ptr<content::Compositor> compositor_;
  TabContentManager* tab_content_manager_;

  scoped_refptr<cc::SolidColorLayer> root_layer_;
  SceneLayer* scene_layer_;
  scoped_refptr<cc::Layer> scene_layer_layer_;

  int current_surface_format_;
  int content_width_;
  int content_height_;
  bool overlay_video_mode_;
  bool overlay_immersive_ar_mode_;

  base::WeakPtrFactory<CompositorView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CompositorView);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_
