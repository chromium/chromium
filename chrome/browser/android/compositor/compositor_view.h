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
class WebContents;
}

namespace ui {
class WindowAndroid;
class ResourceManager;
class UIResourceProvider;
}  // namespace ui

namespace android {

class CompositorViewTest;
class SceneLayer;
class TabContentManager;

class CompositorView : public content::CompositorClient,
                       public content::BrowserChildProcessObserver {
 public:
  CompositorView(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 ui::WindowAndroid* window_android,
                 TabContentManager* tab_content_manager);

  CompositorView(const CompositorView&) = delete;
  CompositorView& operator=(const CompositorView&) = delete;

  ~CompositorView() override;

  ui::ResourceManager* GetResourceManager();
  base::android::ScopedJavaLocalRef<jobject> GetResourceManager(JNIEnv* env);
  void SetNeedsComposite();
  void SetDrawPaused(bool paused);
  void FinalizeLayers();
  void SetLayoutBounds();
  void SurfaceCreated();
  void SurfaceDestroyed();
  std::optional<int> SurfaceChanged(
      int32_t format,
      int32_t width,
      int32_t height,
      bool can_be_used_with_surface_control,
      const base::android::JavaRef<jobject>& surface,
      const base::android::JavaRef<jobject>& browser_input_token);
  void OnPhysicalBackingSizeChanged(content::WebContents* web_contents,
                                    int32_t width,
                                    int32_t height,
                                    bool is_fluid_resize);
  void OnControlsResizeViewChanged(content::WebContents* web_contents,
                                   bool controls_resize_view);
  void NotifyVirtualKeyboardOverlayRect(content::WebContents* web_contents,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);

  void SetOverlayVideoMode(bool enabled);
  void SetOverlayImmersiveArMode(bool enabled);
  void SetOverlayXrFullScreenMode(bool enabled);
  void SetSceneLayer(JNIEnv* env,
                     const base::android::JavaRef<jobject>& jscene_layer);
  void SetCompositorWindow(ui::WindowAndroid* window_android);
  void CacheBackBufferForCurrentSurface();
  void EvictCachedBackBuffer();
  void OnTabChanged();
  void PreserveChildSurfaceControls();
  void SetDidSwapBuffersCallbackEnabled(bool enable);

  // CompositorClient implementation:
  void RecreateSurface() override;
  void UpdateLayerTreeHost() override;
  void DidSwapFrame(int pending_frames) override;
  void DidSwapBuffers(const gfx::Size& swap_size) override;
  base::WeakPtr<ui::UIResourceProvider> GetUIResourceProvider();

 private:
  friend class CompositorViewTest;

  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data,
      const base::Process& process) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void SetBackground(bool visible, SkColor color);

  // Constructor for testing.
  CompositorView(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 ui::WindowAndroid* window_android,
                 TabContentManager* tab_content_manager,
                 std::unique_ptr<content::Compositor> compositor);

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
  bool overlay_xr_full_screen_mode_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_COMPOSITOR_VIEW_H_
