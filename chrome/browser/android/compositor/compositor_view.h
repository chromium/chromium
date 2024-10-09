// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc::slim {
class SolidColorLayer;
}  // namespace cc::slim

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

  CompositorView(const CompositorView&) = delete;
  CompositorView& operator=(const CompositorView&) = delete;

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
  void SurfaceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint format,
      jint width,
      jint height,
      bool can_be_used_with_surface_control,
      const base::android::JavaParamRef<jobject>& surface,
      const base::android::JavaParamRef<jobject>& browser_input_token);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  void OnControlsResizeViewChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jboolean controls_resize_view);
  void NotifyVirtualKeyboardOverlayRect(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint x,
      jint y,
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
  void OnTabChanged(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& object);
  void PreserveChildSurfaceControls(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object);
  void SetDidSwapBuffersCallbackEnabled(JNIEnv* env, jboolean enable);

  // CompositorClient implementation:
  void RecreateSurface() override;
  void UpdateLayerTreeHost() override;
  void DidSwapFrame(int pending_frames) override;
  void DidSwapBuffers(const gfx::Size& swap_size) override;
  base::WeakPtr<ui::UIResourceProvider> GetUIResourceProvider();

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

  // TODO(crbug.com/324196360): One of these is triggering Dangling Pointer
  // Detection. Figure out why and remove the DanglingUntriaged annotation.
  raw_ptr<TabContentManager, DanglingUntriaged> tab_content_manager_;

  scoped_refptr<cc::slim::SolidColorLayer> root_layer_;
  // TODO(crbug.com/324196360): One of these is triggering Dangling Pointer
  // Detection. Figure out why and remove the DanglingUntriaged annotation.
  raw_ptr<SceneLayer, DanglingUntriaged> scene_layer_;

  int current_surface_format_;
  int content_width_;
  int content_height_;
  bool overlay_video_mode_;
  bool overlay_immersive_ar_mode_;

  base::WeakPtrFactory<CompositorView> weak_factory_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_
