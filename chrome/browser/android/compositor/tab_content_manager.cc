// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <android/bitmap.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "cc/slim/layer.h"
#include "chrome/android/chrome_jni_headers/TabContentManager_jni.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/thumbnail/cc/features.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

using TabReadbackCallback = base::OnceCallback<void(float, const SkBitmap&)>;

}  // namespace

namespace android {

class TabContentManager::TabReadbackRequest {
 public:
  TabReadbackRequest(content::RenderWidgetHostView* rwhv,
                     float thumbnail_scale,
                     double aspect_ratio,
                     bool crop_to_match_aspect_ratio,
                     TabReadbackCallback end_callback)
      : thumbnail_scale_(thumbnail_scale),
        end_callback_(std::move(end_callback)),
        drop_after_readback_(false) {
    DCHECK(rwhv);
    auto result_callback =
        base::BindOnce(&TabReadbackRequest::OnFinishGetTabThumbnailBitmap,
                       weak_factory_.GetWeakPtr());

    gfx::Size view_size_in_pixels =
        rwhv->GetNativeView()->GetPhysicalBackingSize();
    if (view_size_in_pixels.IsEmpty()) {
      std::move(result_callback).Run(SkBitmap());
      return;
    }
    if (!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
      if (crop_to_match_aspect_ratio) {
        int height =
            std::min(view_size_in_pixels.height(),
                     (int)(view_size_in_pixels.width() / aspect_ratio));
        view_size_in_pixels.set_height(height);
      }
    }
    gfx::Rect source_rect = gfx::Rect(view_size_in_pixels);
    gfx::Size thumbnail_size(
        gfx::ScaleToCeiledSize(view_size_in_pixels, thumbnail_scale_));
    rwhv->CopyFromSurface(source_rect, thumbnail_size,
                          std::move(result_callback));
  }

  TabReadbackRequest(const TabReadbackRequest&) = delete;
  TabReadbackRequest& operator=(const TabReadbackRequest&) = delete;

  virtual ~TabReadbackRequest() = default;

  void OnFinishGetTabThumbnailBitmap(const SkBitmap& bitmap) {
    if (bitmap.drawsNothing() || drop_after_readback_) {
      std::move(end_callback_).Run(0.f, SkBitmap());
      return;
    }

    SkBitmap result_bitmap = bitmap;
    result_bitmap.setImmutable();
    std::move(end_callback_).Run(thumbnail_scale_, bitmap);
  }

  void SetToDropAfterReadback() { drop_after_readback_ = true; }

 private:
  const float thumbnail_scale_;
  TabReadbackCallback end_callback_;
  bool drop_after_readback_;

  base::WeakPtrFactory<TabReadbackRequest> weak_factory_{this};
};

// static
TabContentManager* TabContentManager::FromJavaObject(
    const JavaRef<jobject>& jobj) {
  if (jobj.is_null()) {
    return nullptr;
  }
  return reinterpret_cast<TabContentManager*>(
      Java_TabContentManager_getNativePtr(base::android::AttachCurrentThread(),
                                          jobj));
}

TabContentManager::TabContentManager(JNIEnv* env,
                                     jobject obj,
                                     jint default_cache_size,
                                     jint approximation_cache_size,
                                     jint compression_queue_max_size,
                                     jint write_queue_max_size,
                                     jboolean use_approximation_thumbnail,
                                     jboolean save_jpeg_thumbnails)
    : weak_java_tab_content_manager_(env, obj) {
  thumbnail_cache_ = std::make_unique<thumbnail::ThumbnailCache>(
      static_cast<size_t>(default_cache_size),
      static_cast<size_t>(approximation_cache_size),
      static_cast<size_t>(compression_queue_max_size),
      static_cast<size_t>(write_queue_max_size), use_approximation_thumbnail,
      save_jpeg_thumbnails);
  thumbnail_cache_->AddThumbnailCacheObserver(this);
}

TabContentManager::~TabContentManager() = default;

void TabContentManager::Destroy(JNIEnv* env) {
  thumbnail_cache_->RemoveThumbnailCacheObserver(this);
  delete this;
}

void TabContentManager::SetUIResourceProvider(
    base::WeakPtr<ui::UIResourceProvider> ui_resource_provider) {
  thumbnail_cache_->SetUIResourceProvider(ui_resource_provider);
}

scoped_refptr<cc::slim::Layer> TabContentManager::GetLiveLayer(int tab_id) {
  if (base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> jtab = Java_TabContentManager_getTabById(
        env, weak_java_tab_content_manager_.get(env), tab_id);
    if (!jtab) {
      return nullptr;
    }
    TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
    if (!tab) {
      return nullptr;
    }
    return tab->GetContentLayer();
  }
  return live_layer_list_[tab_id];
}

ThumbnailLayer* TabContentManager::GetStaticLayer(int tab_id) {
  if (tab_id == -1) {
    return nullptr;
  }
  auto it = static_layer_cache_.find(tab_id);
  if (base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
    // Use a DCHECK to try to prevent this from happening during development,
    // but it is not guranteed that every possible failure case was eliminated
    // when adding this CHECK so leave as a DCHECK of now.
    DCHECK(it != static_layer_cache_.end())
        << "Static layer should be created with UpdateVisibleIds before being"
           "requested";
  }
  return it == static_layer_cache_.end() ? nullptr : it->second.get();
}

ThumbnailLayer* TabContentManager::GetOrCreateStaticLayer(
    int tab_id,
    bool force_disk_read) {
  if (base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
    return GetStaticLayer(tab_id);
  }
  thumbnail::Thumbnail* thumbnail =
      thumbnail_cache_->Get(tab_id, force_disk_read, true);
  scoped_refptr<ThumbnailLayer> static_layer = static_layer_cache_[tab_id];

  if (!thumbnail || !thumbnail->ui_resource_id()) {
    if (static_layer.get()) {
      static_layer->layer()->RemoveFromParent();
      static_layer_cache_.erase(tab_id);
    }
    return nullptr;
  }

  if (!static_layer.get()) {
    static_layer = ThumbnailLayer::Create();
    static_layer_cache_[tab_id] = static_layer;
  }

  static_layer->SetThumbnail(thumbnail);
  return static_layer.get();
}

void TabContentManager::AttachTab(JNIEnv* env,
                                  const JavaParamRef<jobject>& jtab,
                                  jint tab_id) {
  DCHECK(!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor));
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  scoped_refptr<cc::slim::Layer> layer = tab->GetContentLayer();
  if (!layer.get()) {
    return;
  }

  scoped_refptr<cc::slim::Layer> cached_layer = live_layer_list_[tab_id];
  if (cached_layer != layer) {
    live_layer_list_[tab_id] = layer;
  }
}

void TabContentManager::DetachTab(JNIEnv* env,
                                  const JavaParamRef<jobject>& jtab,
                                  jint tab_id) {
  DCHECK(!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor));
  scoped_refptr<cc::slim::Layer> current_layer = live_layer_list_[tab_id];
  if (!current_layer.get()) {
    // Empty cached layer should not exist but it is ok if it happens.
    return;
  }

  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  scoped_refptr<cc::slim::Layer> layer = tab->GetContentLayer();
  // We need to remove if we're getting a detach for our current layer or we're
  // getting a detach with NULL and we have a current layer, which means remove
  //  all layers.
  if (current_layer.get() &&
      (layer.get() == current_layer.get() || !layer.get())) {
    live_layer_list_.erase(tab_id);
  }
}

content::RenderWidgetHostView* TabContentManager::GetRwhvForTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& tab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  const int tab_id = tab_android->GetAndroidId();
  if (pending_tab_readbacks_.find(tab_id) != pending_tab_readbacks_.end()) {
    return nullptr;
  }

  content::WebContents* web_contents = tab_android->web_contents();
  DCHECK(web_contents);

  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (!rvh) {
    return nullptr;
  }

  content::RenderWidgetHost* rwh = rvh->GetWidget();
  content::RenderWidgetHostView* rwhv = rwh ? rwh->GetView() : nullptr;
  if (!rwhv || !rwhv->IsSurfaceAvailableForCopy()) {
    return nullptr;
  }

  return rwhv;
}

std::unique_ptr<thumbnail::ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
TabContentManager::TrackCapture(thumbnail::TabId tab_id) {
  std::unique_ptr<thumbnail::ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
      tracker(new thumbnail::ThumbnailCaptureTracker(
                  base::BindOnce(&TabContentManager::OnTrackingFinished,
                                 weak_factory_.GetWeakPtr(), tab_id)),
              base::OnTaskRunnerDeleter(
                  base::SequencedTaskRunner::GetCurrentDefault()));
  in_flight_captures_[tab_id] = tracker->GetWeakPtr();
  return tracker;
}

void TabContentManager::OnTrackingFinished(
    int tab_id,
    thumbnail::ThumbnailCaptureTracker* tracker) {
  auto it = in_flight_captures_.find(tab_id);
  if (it == in_flight_captures_.end()) {
    return;
  }
  // Remove only the latest tracker.
  if (it->second.get() == tracker) {
    in_flight_captures_.erase(it);
  }
}

void TabContentManager::CaptureThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& tab,
    jfloat thumbnail_scale,
    jboolean write_to_cache,
    jdouble aspect_ratio,
    const base::android::JavaParamRef<jobject>& j_callback) {
  // Ensure capture only happens on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  const int tab_id = tab_android->GetAndroidId();

  content::RenderWidgetHostView* rwhv = GetRwhvForTab(env, tab);
  // If the tab's ID is in the list of VisibleIds then it has a LayoutTab
  // active and can be captured. Otherwise the surface will be missing and
  // the capture will stall forever.
  if (!rwhv || !thumbnail_cache_->IsInVisibleIds(tab_id)) {
    if (j_callback) {
      RunObjectCallbackAndroid(j_callback, nullptr);
    }
    return;
  }
  if (write_to_cache && !thumbnail_cache_->CheckAndUpdateThumbnailMetaData(
                            tab_id, tab_android->GetURL())) {
    return;
  }
  TabReadbackCallback readback_done_callback =
      base::BindOnce(&TabContentManager::OnTabReadback,
                     weak_factory_.GetWeakPtr(), tab_id, TrackCapture(tab_id),
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback),
                     write_to_cache, aspect_ratio);
  pending_tab_readbacks_[tab_id] = std::make_unique<TabReadbackRequest>(
      rwhv, thumbnail_scale, aspect_ratio, !write_to_cache,
      std::move(readback_done_callback));
}

void TabContentManager::CacheTabWithBitmap(JNIEnv* env,
                                           const JavaParamRef<jobject>& tab,
                                           const JavaParamRef<jobject>& bitmap,
                                           jfloat thumbnail_scale,
                                           jdouble aspect_ratio) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  int tab_id = tab_android->GetAndroidId();
  GURL url = tab_android->GetURL();

  gfx::JavaBitmap java_bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
  skbitmap.setImmutable();

  if (thumbnail_cache_->CheckAndUpdateThumbnailMetaData(tab_id, url)) {
    OnTabReadback(tab_id, TrackCapture(tab_id), nullptr, true, aspect_ratio,
                  thumbnail_scale, skbitmap);
  }
}

void TabContentManager::InvalidateIfChanged(JNIEnv* env,
                                            jint tab_id,
                                            const JavaParamRef<jobject>& jurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, jurl);
  thumbnail_cache_->InvalidateThumbnailIfChanged(tab_id, *url);
}

void TabContentManager::UpdateVisibleIds(
    JNIEnv* env,
    const JavaParamRef<jintArray>& priority,
    jint primary_tab_id) {
  std::vector<int> priority_ids;
  base::android::JavaIntArrayToIntVector(env, priority, &priority_ids);
  thumbnail_cache_->UpdateVisibleIds(priority_ids, primary_tab_id);
  if (!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
    return;
  }
  std::erase_if(static_layer_cache_, [&priority_ids](const auto& pair) {
    bool not_priority = !base::Contains(priority_ids, pair.first);
    if (not_priority && pair.second) {
      pair.second->layer()->RemoveFromParent();
    }
    return not_priority;
  });
  for (int tab_id : priority_ids) {
    auto static_layer = static_layer_cache_[tab_id];
    if (!static_layer) {
      static_layer = ThumbnailLayer::Create();
      static_layer_cache_[tab_id] = static_layer;
    }
    thumbnail::Thumbnail* thumbnail =
        thumbnail_cache_->Get(tab_id, false, false);
    if (thumbnail) {
      static_layer->SetThumbnail(thumbnail);
    }
  }
}

void TabContentManager::NativeRemoveTabThumbnail(int tab_id) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);
  if (readback_iter != pending_tab_readbacks_.end()) {
    readback_iter->second->SetToDropAfterReadback();
  }
  thumbnail_cache_->Remove(tab_id);
}

void TabContentManager::RemoveTabThumbnail(JNIEnv* env, jint tab_id) {
  NativeRemoveTabThumbnail(tab_id);
}

void TabContentManager::WaitForJpegTabThumbnail(
    JNIEnv* env,
    jint tab_id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  DCHECK(base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor));

  auto it = in_flight_captures_.find(tab_id);
  if (it != in_flight_captures_.end() && it->second) {
    // A capture is currently ongoing wait till it finishes.
    it->second->AddOnJpegFinishedCallback(base::BindOnce(
        &base::android::RunBooleanCallbackAndroid,
        base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
  } else {
    // Thumbnail is not currently being captured. Run the callback.
    RunBooleanCallbackAndroid(j_callback, true);
  }
}

void TabContentManager::GetEtc1TabThumbnail(
    JNIEnv* env,
    jint tab_id,
    jdouble aspect_ratio,
    const base::android::JavaParamRef<jobject>& j_callback) {
  thumbnail_cache_->DecompressEtc1ThumbnailFromFile(
      tab_id, aspect_ratio,
      base::BindOnce(&TabContentManager::SendThumbnailToJava,
                     weak_factory_.GetWeakPtr(),
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback),
                     /*need_downsampling=*/true, aspect_ratio));
}

void TabContentManager::OnUIResourcesWereEvicted() {
  thumbnail_cache_->OnUIResourcesWereEvicted();
}

void TabContentManager::OnThumbnailAddedToCache(int tab_id) {
  if (!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
    return;
  }
  auto it = static_layer_cache_.find(tab_id);
  if (it != static_layer_cache_.end()) {
    thumbnail::Thumbnail* thumbnail =
        thumbnail_cache_->Get(tab_id, false, false);
    it->second->SetThumbnail(thumbnail);
  }
}

void TabContentManager::OnFinishedThumbnailRead(int tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabContentManager_notifyListenersOfThumbnailChange(
      env, weak_java_tab_content_manager_.get(env), tab_id);
}

void TabContentManager::OnTabReadback(
    int tab_id,
    std::unique_ptr<thumbnail::ThumbnailCaptureTracker,
                    base::OnTaskRunnerDeleter> tracker,
    base::android::ScopedJavaGlobalRef<jobject> j_callback,
    bool write_to_cache,
    double aspect_ratio,
    float thumbnail_scale,
    const SkBitmap& bitmap) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);

  if (readback_iter != pending_tab_readbacks_.end()) {
    pending_tab_readbacks_.erase(tab_id);
  }

  if (j_callback) {
    SendThumbnailToJava(j_callback, write_to_cache, aspect_ratio, true, bitmap);
  }

  if (write_to_cache && thumbnail_scale > 0 && !bitmap.empty()) {
    thumbnail_cache_->Put(tab_id, std::move(tracker), bitmap, thumbnail_scale,
                          aspect_ratio);
  }
}

void TabContentManager::SendThumbnailToJava(
    base::android::ScopedJavaGlobalRef<jobject> j_callback,
    bool need_downsampling,
    double aspect_ratio,
    bool result,
    const SkBitmap& bitmap) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!bitmap.isNull() && result) {
    // We want to show thumbnails in a specific aspect ratio. Therefore, the
    // thumbnail saved needs to be cropped to the target aspect ratio,
    // otherwise it would be vertically center-aligned and the top would be
    // hidden in portrait mode, or it would be shown in the wrong aspect ratio
    // in landscape mode.
    int scale = need_downsampling ? 2 : 1;

    int width = 0;
    int height = 0;
    if (!base::FeatureList::IsEnabled(thumbnail::kThumbnailCacheRefactor)) {
      width = std::min(bitmap.width() / scale,
                       (int)(bitmap.height() * aspect_ratio / scale));
      height = std::min(bitmap.height() / scale,
                        (int)(bitmap.width() / aspect_ratio / scale));
    } else {
      width = bitmap.width() / scale;
      height = bitmap.height() / scale;
    }
    // When cropping the thumbnails, we want to keep the top center portion.
    int begin_x = (bitmap.width() / scale - width) / 2;
    int end_x = begin_x + width;
    SkIRect dest_subset = {begin_x, 0, end_x, height};

    j_bitmap = gfx::ConvertToJavaBitmap(skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BETTER, bitmap.width() / scale,
        bitmap.height() / scale, dest_subset));
  }
  RunObjectCallbackAndroid(j_callback, j_bitmap);
}

void TabContentManager::SetCaptureMinRequestTimeForTesting(JNIEnv* env,
                                                           jint timeMs) {
  thumbnail_cache_->SetCaptureMinRequestTimeForTesting(timeMs);
}

jint TabContentManager::GetPendingReadbacksForTesting(JNIEnv* env) {
  return pending_tab_readbacks_.size();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_TabContentManager_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jint default_cache_size,
                                 jint approximation_cache_size,
                                 jint compression_queue_max_size,
                                 jint write_queue_max_size,
                                 jboolean use_approximation_thumbnail,
                                 jboolean save_jpeg_thumbnails) {
  // Ensure this and its thumbnail cache are created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TabContentManager* manager = new TabContentManager(
      env, obj, default_cache_size, approximation_cache_size,
      compression_queue_max_size, write_queue_max_size,
      use_approximation_thumbnail, save_jpeg_thumbnails);
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace android
