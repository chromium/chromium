// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/compositor_view.h"

#include <android/bitmap.h>

#include <memory>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/containers/id_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/peak_gpu_memory_tracker_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CompositorView_jni.h"

using base::android::JavaParamRef;

namespace android {

static jboolean JNI_CompositorView_IsSurfaceControlEnabled(JNIEnv* env) {
  return features::IsAndroidSurfaceControlEnabled();
}

static jlong JNI_CompositorView_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jwindow_android,
    const JavaParamRef<jobject>& jtab_content_manager) {
  CompositorView* view;
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  TabContentManager* tab_content_manager =
      TabContentManager::FromJavaObject(jtab_content_manager);

  DCHECK(tab_content_manager);

  // TODO(clholgat): Remove the compositor tabstrip flag.
  view = new CompositorView(env, obj, window_android, tab_content_manager);

  if (tab_content_manager) {
    tab_content_manager->SetUIResourceProvider(view->GetUIResourceProvider());
  }

  return reinterpret_cast<intptr_t>(view);
}

static jboolean JNI_CompositorView_PreferRgb565ForDisplay(JNIEnv* env) {
  return features::PreferRGB565ResourcesForDisplay();
}

CompositorView::CompositorView(JNIEnv* env,
                               const base::android::JavaRef<jobject>& obj,
                               ui::WindowAndroid* window_android,
                               TabContentManager* tab_content_manager)
    : tab_content_manager_(tab_content_manager),
      root_layer_(cc::slim::SolidColorLayer::Create()),
      scene_layer_(nullptr),
      current_surface_format_(0),
      content_width_(0),
      content_height_(0),
      overlay_video_mode_(false),
      overlay_immersive_ar_mode_(false),
      overlay_xr_full_screen_mode_(false) {
  content::BrowserChildProcessObserver::Add(this);
  obj_.Reset(env, obj);
  compositor_.reset(content::Compositor::Create(this, window_android));

  root_layer_->SetIsDrawable(true);
  root_layer_->SetBackgroundColor(SkColors::kWhite);
}

// Constructor for testing.
CompositorView::CompositorView(JNIEnv* env,
                               const base::android::JavaRef<jobject>& obj,
                               ui::WindowAndroid* window_android,
                               TabContentManager* tab_content_manager,
                               std::unique_ptr<content::Compositor> compositor)
    : tab_content_manager_(tab_content_manager),
      root_layer_(cc::slim::SolidColorLayer::Create()),
      scene_layer_(nullptr),
      current_surface_format_(0),
      content_width_(0),
      content_height_(0),
      overlay_video_mode_(false),
      overlay_immersive_ar_mode_(false),
      overlay_xr_full_screen_mode_(false) {
  content::BrowserChildProcessObserver::Add(this);
  obj_.Reset(env, obj);
  compositor_ = std::move(compositor);

  root_layer_->SetIsDrawable(true);
  root_layer_->SetBackgroundColor(SkColors::kWhite);
}

CompositorView::~CompositorView() {
  content::BrowserChildProcessObserver::Remove(this);
  tab_content_manager_->OnUIResourcesWereEvicted();

  // Explicitly reset these scoped_ptrs here because otherwise we callbacks will
  // try to access member variables during destruction.
  compositor_.reset();
}

void CompositorView::Destroy(JNIEnv* env) {
  delete this;
}

ui::ResourceManager* CompositorView::GetResourceManager() {
  return compositor_ ? &compositor_->GetResourceManager() : nullptr;
}

base::android::ScopedJavaLocalRef<jobject> CompositorView::GetResourceManager(
    JNIEnv* env) {
  return compositor_->GetResourceManager().GetJavaObject();
}

void CompositorView::RecreateSurface() {
  JNIEnv* env = base::android::AttachCurrentThread();
  compositor_->SetSurface(nullptr, false, nullptr);
  Java_CompositorView_recreateSurface(env, obj_);
}

void CompositorView::UpdateLayerTreeHost() {
  std::optional<base::ElapsedTimer> timer;
  if (base::ShouldRecordSubsampledMetric(0.01)) {
    timer.emplace();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(wkorman): Rename JNI interface to onCompositorUpdateLayerTreeHost.
  Java_CompositorView_onCompositorLayout(env, obj_);

  if (timer) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Android.Compositor.UpdateLayerTree.Duration.Subsampled",
        timer->Elapsed(), base::Microseconds(1), base::Milliseconds(30), 50);
  }
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

base::WeakPtr<ui::UIResourceProvider> CompositorView::GetUIResourceProvider() {
  return compositor_ ? compositor_->GetUIResourceProvider() : nullptr;
}

void CompositorView::SurfaceCreated(JNIEnv* env) {
  compositor_->SetRootLayer(root_layer_);
  current_surface_format_ = 0;
}

void CompositorView::SurfaceDestroyed(JNIEnv* env) {
  compositor_->SetSurface(nullptr, false, nullptr);
  current_surface_format_ = 0;
  tab_content_manager_->OnUIResourcesWereEvicted();
}

std::optional<int> CompositorView::SurfaceChanged(
    JNIEnv* env,
    jint format,
    jint width,
    jint height,
    bool can_be_used_with_surface_control,
    const JavaParamRef<jobject>& surface,
    const JavaParamRef<jobject>& browser_input_token) {
  // Java View layout sometimes unexpectedly cause CompositorView to be sized so
  // large that it exceeds the max texture size and memory on the device. This
  // then subsequently causes the GPU process to crash loop. See
  // crbug.com/369374760. Ignore these which is probably less bad than crashing
  // the GPU process.
  constexpr int kExcessiveSurfaceSize = 1000000;
  if (width >= kExcessiveSurfaceSize || height >= kExcessiveSurfaceSize ||
      width <= 0 || height <= 0) {
    LOG(WARNING) << "Ignoring invalid surface size " << width << "x" << height;
    return std::nullopt;
  }

  std::optional<int> surface_handle = std::nullopt;
  DCHECK(surface);
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    surface_handle = compositor_->SetSurface(
        surface, can_be_used_with_surface_control, browser_input_token);
  }
  gfx::Size size = gfx::Size(width, height);
  compositor_->SetWindowBounds(size);
  content_width_ = size.width();
  content_height_ = size.height();
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
  return surface_handle;
}

void CompositorView::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
}

void CompositorView::OnControlsResizeViewChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jboolean controls_resize_view) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  web_contents->GetNativeView()->OnControlsResizeViewChanged(
      controls_resize_view);
}

void CompositorView::NotifyVirtualKeyboardOverlayRect(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint x,
    jint y,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Rect keyboard_rect(x, y, width, height);
  web_contents->GetNativeView()->NotifyVirtualKeyboardOverlayRect(
      keyboard_rect);
}

void CompositorView::SetLayoutBounds(JNIEnv* env) {
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
}

void CompositorView::SetBackground(bool visible, SkColor color) {
  // TODO(crbug.com/41347744): Set the background color on the compositor.
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  root_layer_->SetBackgroundColor(SkColor4f::FromColor(color));
  root_layer_->SetIsDrawable(visible);
}

void CompositorView::SetOverlayVideoMode(JNIEnv* env,
                                         bool enabled) {
  if (overlay_video_mode_ == enabled) {
    return;
  }
  overlay_video_mode_ = enabled;
  compositor_->SetRequiresAlphaChannel(enabled);
  SetNeedsComposite(env);
}

void CompositorView::SetOverlayImmersiveArMode(
    JNIEnv* env,
    bool enabled) {
  DVLOG(1) << __func__ << ": enabled=" << enabled;

  overlay_immersive_ar_mode_ = enabled;

  // This method may be called after SetOverlayVideoMode (when switching between
  // opaque and translucent surfaces), or just by itself (in SurfaceControl
  // mode). All settings from SetOverlayVideoMode that the AR overlay depends on
  // must be duplicated here. Currently, that's just SetRequiresAlphaChannel.
  compositor_->SetRequiresAlphaChannel(enabled);

  // This mode needs a transparent background color.
  // ContentViewRenderView::SetOverlayVideoMode applies this, but the
  // CompositorView::SetOverlayVideoMode version in this file doesn't.
  compositor_->SetBackgroundColor(enabled ? SK_ColorTRANSPARENT
                                          : SK_ColorWHITE);
  compositor_->SetNeedsComposite();
}

void CompositorView::SetOverlayXrFullScreenMode(
    JNIEnv* env,
    bool enabled) {
  if (overlay_xr_full_screen_mode_ == enabled) {
    return;
  }

  overlay_xr_full_screen_mode_ = enabled;

  // XR full screen mode requires a transparent background.
  compositor_->SetBackgroundColor(enabled ? SK_ColorTRANSPARENT
                                          : SK_ColorWHITE);
  compositor_->SetNeedsComposite();
}

void CompositorView::SetSceneLayer(JNIEnv* env,
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
      return;
    }

    root_layer_->InsertChild(scene_layer->layer(), 0);
  }

  if (overlay_xr_full_screen_mode_ || overlay_immersive_ar_mode_) {
    // Suppress the scene background's default background which breaks
    // transparency. TODO(crbug.com/40098084): Remove this workaround
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

void CompositorView::FinalizeLayers(JNIEnv* env) {
  if (GetResourceManager()) {
    GetResourceManager()->OnFrameUpdatesFinished();
  }
#if !defined(OFFICIAL_BUILD)
  TRACE_EVENT0("compositor", "CompositorView::FinalizeLayers");
#endif
}

void CompositorView::SetNeedsComposite(JNIEnv* env) {
  compositor_->SetNeedsComposite();
}

void CompositorView::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  LOG(WARNING) << "Child process died (type=" << data.process_type
               << ") pid=" << data.GetProcess().Pid() << ")";

  // On Android R surface control layers leak if GPU process crashes, so we need
  // to re-create surface to get rid of them.
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    JNIEnv* env = base::android::AttachCurrentThread();
    compositor_->SetSurface(nullptr, false, nullptr);
    Java_CompositorView_recreateSurface(env, obj_);
  }
}

void CompositorView::SetCompositorWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& window_android) {
  ui::WindowAndroid* wa =
      ui::WindowAndroid::FromJavaWindowAndroid(window_android);
  compositor_->SetRootWindow(wa);
}

void CompositorView::CacheBackBufferForCurrentSurface(JNIEnv* env) {
  compositor_->CacheBackBufferForCurrentSurface();
}

void CompositorView::EvictCachedBackBuffer(JNIEnv* env) {
  compositor_->EvictCachedBackBuffer();
}

void CompositorView::OnTabChanged(JNIEnv* env) {
  if (!compositor_) {
    return;
  }
  std::unique_ptr<viz::PeakGpuMemoryTracker> tracker =
      content::PeakGpuMemoryTrackerFactory::Create(
          viz::PeakGpuMemoryTracker::Usage::CHANGE_TAB);
  compositor_->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
      [](std::unique_ptr<viz::PeakGpuMemoryTracker> tracker,
         const viz::FrameTimingDetails& frame_timing_details) {
        // This callback will be ran once the content::Compositor presents the
        // next frame. The destruction of |tracker| will get the peak GPU memory
        // and record a histogram.
      },
      std::move(tracker)));
}

void CompositorView::PreserveChildSurfaceControls(JNIEnv* env) {
  compositor_->PreserveChildSurfaceControls();
}

void CompositorView::SetDidSwapBuffersCallbackEnabled(JNIEnv* env,
                                                      jboolean enable) {
  compositor_->SetDidSwapBuffersCallbackEnabled(enable);
}

}  // namespace android

DEFINE_JNI(CompositorView)
