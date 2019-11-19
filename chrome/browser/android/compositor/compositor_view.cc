// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/compositor_view.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include <memory>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/id_map.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "chrome/android/chrome_jni_headers/CompositorView_jni.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;

namespace android {

jlong JNI_CompositorView_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean low_mem_device,
    const JavaParamRef<jobject>& jwindow_android,
    const JavaParamRef<jobject>& jlayer_title_cache,
    const JavaParamRef<jobject>& jtab_content_manager) {
  CompositorView* view;
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);
  TabContentManager* tab_content_manager =
      TabContentManager::FromJavaObject(jtab_content_manager);

  DCHECK(tab_content_manager);

  // TODO(clholgat): Remove the compositor tabstrip flag.
  view = new CompositorView(env, obj, low_mem_device, window_android,
                            tab_content_manager);

  ui::UIResourceProvider* ui_resource_provider = view->GetUIResourceProvider();
  // TODO(dtrainor): Pass the ResourceManager on the Java side to the tree
  // builders instead.
  if (layer_title_cache)
    layer_title_cache->SetResourceManager(view->GetResourceManager());
  if (tab_content_manager)
    tab_content_manager->SetUIResourceProvider(ui_resource_provider);

  return reinterpret_cast<intptr_t>(view);
}

CompositorView::CompositorView(JNIEnv* env,
                               jobject obj,
                               jboolean low_mem_device,
                               ui::WindowAndroid* window_android,
                               TabContentManager* tab_content_manager)
    : tab_content_manager_(tab_content_manager),
      root_layer_(cc::SolidColorLayer::Create()),
      scene_layer_(nullptr),
      current_surface_format_(0),
      content_width_(0),
      content_height_(0),
      overlay_video_mode_(false),
      overlay_immersive_ar_mode_(false) {
  content::BrowserChildProcessObserver::Add(this);
  obj_.Reset(env, obj);
  compositor_.reset(content::Compositor::Create(this, window_android));

  root_layer_->SetIsDrawable(true);
  root_layer_->SetBackgroundColor(SK_ColorWHITE);

  // It is safe to not keep a ref on the feature checker because it adds one
  // internally in CheckGpuFeatureAvailability and unrefs after the callback is
  // dispatched.
  auto surface_control_feature_checker = content::GpuFeatureChecker::Create(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL,
      base::Bind(&CompositorView::OnSurfaceControlFeatureStatusUpdate,
                 weak_factory_.GetWeakPtr()));
  surface_control_feature_checker->CheckGpuFeatureAvailability();
}

CompositorView::~CompositorView() {
  content::BrowserChildProcessObserver::Remove(this);
  tab_content_manager_->OnUIResourcesWereEvicted();

  // Explicitly reset these scoped_ptrs here because otherwise we callbacks will
  // try to access member variables during destruction.
  compositor_.reset();
}

void CompositorView::Destroy(JNIEnv* env, const JavaParamRef<jobject>& object) {
  delete this;
}

ui::ResourceManager* CompositorView::GetResourceManager() {
  return compositor_ ? &compositor_->GetResourceManager() : nullptr;
}

base::android::ScopedJavaLocalRef<jobject> CompositorView::GetResourceManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  return compositor_->GetResourceManager().GetJavaObject();
}

void CompositorView::RecreateSurface() {
  JNIEnv* env = base::android::AttachCurrentThread();
  compositor_->SetSurface(nullptr, false);
  Java_CompositorView_recreateSurface(env, obj_);
}

void CompositorView::UpdateLayerTreeHost() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(wkorman): Rename JNI interface to onCompositorUpdateLayerTreeHost.
  Java_CompositorView_onCompositorLayout(env, obj_);
}

void CompositorView::DidSwapFrame(int pending_frames) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CompositorView_didSwapFrame(env, obj_, pending_frames);
}

void CompositorView::DidSwapBuffers(const gfx::Size& swap_size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool swapped_current_size =
      swap_size == gfx::Size(content_width_, content_height_);
  Java_CompositorView_didSwapBuffers(env, obj_, swapped_current_size);
}

ui::UIResourceProvider* CompositorView::GetUIResourceProvider() {
  return compositor_ ? &compositor_->GetUIResourceProvider() : nullptr;
}

void CompositorView::OnSurfaceControlFeatureStatusUpdate(bool available) {
  if (available) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_CompositorView_notifyWillUseSurfaceControl(env, obj_);
  }
}

void CompositorView::SurfaceCreated(JNIEnv* env,
                                    const JavaParamRef<jobject>& object) {
  compositor_->SetRootLayer(root_layer_);
  current_surface_format_ = 0;
}

void CompositorView::SurfaceDestroyed(JNIEnv* env,
                                      const JavaParamRef<jobject>& object) {
  compositor_->SetSurface(nullptr, false);
  current_surface_format_ = 0;
  tab_content_manager_->OnUIResourcesWereEvicted();
}

void CompositorView::SurfaceChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& object,
                                    jint format,
                                    jint width,
                                    jint height,
                                    bool can_be_used_with_surface_control,
                                    const JavaParamRef<jobject>& surface) {
  DCHECK(surface);
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    compositor_->SetSurface(surface, can_be_used_with_surface_control);
  }
  gfx::Size size = gfx::Size(width, height);
  compositor_->SetWindowBounds(size);
  content_width_ = size.width();
  content_height_ = size.height();
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
}

void CompositorView::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
}

void CompositorView::SetLayoutBounds(JNIEnv* env,
                                     const JavaParamRef<jobject>& object) {
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
}

void CompositorView::SetBackground(bool visible, SkColor color) {
  // TODO(crbug.com/770911): Set the background color on the compositor.
  root_layer_->SetBackgroundColor(color);
  root_layer_->SetIsDrawable(visible);
}

void CompositorView::SetOverlayVideoMode(JNIEnv* env,
                                         const JavaParamRef<jobject>& object,
                                         bool enabled) {
  if (overlay_video_mode_ == enabled)
    return;
  overlay_video_mode_ = enabled;
  compositor_->SetRequiresAlphaChannel(enabled);
  SetNeedsComposite(env, object);
}

void CompositorView::SetOverlayImmersiveArMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    bool enabled) {
  // This mode is a variant of overlay video mode, the Java code is responsible
  // for calling SetOverlayVideoMode(enabled) first to ensure consistent state.
  // Check to make sure this didn't get bypassed.
  DCHECK_EQ(enabled, overlay_video_mode_) << "missing SetOverlayVideoMode call";

  overlay_immersive_ar_mode_ = enabled;
  // This mode needs a transparent background color.
  // ContentViewRenderView::SetOverlayVideoMode applies this, but the
  // CompositorView::SetOverlayVideoMode version in this file doesn't.
  compositor_->SetBackgroundColor(enabled ? SK_ColorTRANSPARENT
                                          : SK_ColorWHITE);
  compositor_->SetNeedsComposite();
}

void CompositorView::SetSceneLayer(JNIEnv* env,
                                   const JavaParamRef<jobject>& object,
                                   const JavaParamRef<jobject>& jscene_layer) {
  SceneLayer* scene_layer = SceneLayer::FromJavaObject(env, jscene_layer);

  if (scene_layer_ != scene_layer) {
    // The old tree should be detached only if it is not the cached layer or
    // the cached layer is not somewhere in the new root.
    if (scene_layer_ &&
        !scene_layer_->layer()->HasAncestor(scene_layer->layer().get())) {
      scene_layer_->OnDetach();
    }

    scene_layer_ = scene_layer;

    if (!scene_layer) {
      scene_layer_layer_ = nullptr;
      return;
    }

    scene_layer_layer_ = scene_layer->layer();
    root_layer_->InsertChild(scene_layer->layer(), 0);
  }

  if (overlay_immersive_ar_mode_) {
    // Suppress the scene background's default background which breaks
    // transparency. TODO(https://crbug.com/1002270): Remove this workaround
    // once the issue with StaticTabSceneLayer's unexpected background is
    // resolved.
    bool should_show_background = scene_layer->ShouldShowBackground();
    SkColor color = scene_layer->GetBackgroundColor();
    if (should_show_background && color != SK_ColorTRANSPARENT) {
      DVLOG(1) << "override non-transparent background 0x" << std::hex << color;
      SetBackground(false, SK_ColorTRANSPARENT);
    } else {
      // No override needed, scene doesn't provide an opaque background.
      SetBackground(should_show_background, color);
    }
  } else if (scene_layer) {
    SetBackground(scene_layer->ShouldShowBackground(),
                  scene_layer->GetBackgroundColor());
  } else {
#ifndef NDEBUG
    // This should not happen. Setting red background just for debugging.
    SetBackground(true, SK_ColorRED);
#else
    SetBackground(true, SK_ColorBLACK);
#endif
  }
}

void CompositorView::FinalizeLayers(JNIEnv* env,
                                    const JavaParamRef<jobject>& jobj) {
#if !defined(OFFICIAL_BUILD)
  TRACE_EVENT0("compositor", "CompositorView::FinalizeLayers");
#endif
}

void CompositorView::SetNeedsComposite(JNIEnv* env,
                                       const JavaParamRef<jobject>& object) {
  compositor_->SetNeedsComposite();
}

void CompositorView::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  LOG(WARNING) << "Child process died (type=" << data.process_type
               << ") pid=" << data.GetProcess().Pid() << ")";
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
          base::android::SDK_VERSION_JELLY_BEAN_MR2 &&
      data.process_type == content::PROCESS_TYPE_GPU) {
    JNIEnv* env = base::android::AttachCurrentThread();
    compositor_->SetSurface(nullptr, false);
    Java_CompositorView_recreateSurface(env, obj_);
  }
}

void CompositorView::SetCompositorWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& window_android) {
  ui::WindowAndroid* wa =
      ui::WindowAndroid::FromJavaWindowAndroid(window_android);
  compositor_->SetRootWindow(wa);
}

void CompositorView::CacheBackBufferForCurrentSurface(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& object) {
  compositor_->CacheBackBufferForCurrentSurface();
}

void CompositorView::EvictCachedBackBuffer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& object) {
  compositor_->EvictCachedBackBuffer();
}

}  // namespace android
