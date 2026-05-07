// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <android/bitmap.h>
#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "cc/slim/layer.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/retry_strategy.h"
#include "chrome/browser/android/compositor/retryable_task.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/thumbnail/cc/thumbnail.h"
#include "components/sync_sessions/features.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_ui/android/jni_headers/TabContentManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::RunBooleanCallbackAndroid;
using base::android::RunObjectCallbackAndroid;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

using TabReadbackCallback = base::OnceCallback<void(float, const SkBitmap&)>;

// After this amount of time we will give up waiting for the readback as it is
// unlikely that it will complete. Having the callbacks continue to wait may
// leak memory or cause callbacks to hang indefinitely.
const base::TimeDelta kTabReadbackTimeout = base::Seconds(5);

constexpr int kMaxReadbackRetries = 5;
constexpr base::TimeDelta kReadbackRetryDelay = base::Milliseconds(100);

content::RenderWidgetHostView* GetRwhv(content::WebContents* web_contents) {
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (!rvh) {
    return nullptr;
  }
  content::RenderWidgetHost* rwh = rvh->GetWidget();
  return rwh ? rwh->GetView() : nullptr;
}

// Compresses `bitmap` to a jpg of a size (in both pixels and bytes) appropriate
// for syncing, and runs `callback` with the result. In case of compression
// failure, does not run the `callback`.
void CompressScreenshotForSync(const SkBitmap& bitmap,
                               base::OnceCallback<void(std::string)> callback) {
  // Resize to roughly 500x500.
  float scale = 500.0f / std::max(bitmap.width(), bitmap.height());
  // Only downscale.
  SkBitmap resized = bitmap;
  if (scale < 1.0f) {
    int new_width = std::round(bitmap.width() * scale);
    int new_height = std::round(bitmap.height() * scale);
    resized = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BETTER, new_width, new_height);
  }

  // Compress to JPG, quality adjusted to be < 8kB.
  // Use binary search with up to 4 steps between 1 and 50 to find a good
  // quality that fits in the size limit.
  int low = 1;
  int high = 50;
  std::optional<std::vector<uint8_t>> best_encoded_data;

  const size_t kMaxSize = 8000;
  const size_t kMinGoodEnoughSize = 7000;

  for (int i = 0; i < 4; ++i) {
    int quality = low + (high - low) / 2;
    std::optional<std::vector<uint8_t>> data =
        gfx::JPEGCodec::Encode(resized, quality);
    if (data && data->size() <= kMaxSize) {
      best_encoded_data = std::move(data);
      low = quality + 1;

      // Optimization: If the current attempt is close enough to the target
      // size, stop searching.
      if (best_encoded_data->size() > kMinGoodEnoughSize) {
        break;
      }
    } else {
      high = quality - 1;
    }
  }

  // If compression failed or all attempts were > 8 kB, give up.
  if (!best_encoded_data) {
    return;
  }

  std::move(callback).Run(
      std::string(best_encoded_data->begin(), best_encoded_data->end()));
}

}  // namespace

namespace android {
class TabContentManager::TabReadbackRequest : public RetryableTask {
 public:
  TabReadbackRequest(content::WebContents* web_contents,
                     float thumbnail_scale,
                     TabReadbackCallback result_callback)
      : weak_web_contents_(web_contents->GetWeakPtr()),
        thumbnail_scale_(thumbnail_scale),
        result_callback_(std::move(result_callback)),
        retry_strategy_(std::make_unique<RetryStrategy>(kMaxReadbackRetries,
                                                        kReadbackRetryDelay)) {
    auto* rwhv = GetRwhv(web_contents);
    if (!rwhv) {
      std::move(result_callback_).Run(0.f, SkBitmap());
      return;
    }
    // Cannot increment capturer count for rwhv that is null.
    decrementor_ =
        web_contents->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                             /*stay_awake=*/false,
                                             /*is_activity=*/false);
    retry_strategy_->Start(
        this, base::BindOnce(&TabReadbackRequest::OnRetryLoopFinished,
                             weak_factory_.GetWeakPtr()));
  }
  TabReadbackRequest(const TabReadbackRequest&) = delete;
  TabReadbackRequest& operator=(const TabReadbackRequest&) = delete;
  ~TabReadbackRequest() override = default;

  void Run(base::OnceCallback<void(bool)> should_retry_callback) override {
    auto* web_contents = weak_web_contents_.get();
    if (!web_contents) {
      SetToDropAfterReadback();
      OnFinishGetTabThumbnailBitmap(std::move(should_retry_callback),
                                    content::CopyFromSurfaceResult());
      return;
    }
    auto* rwhv = GetRwhv(web_contents);
    if (!rwhv) {
      SetToDropAfterReadback();
      OnFinishGetTabThumbnailBitmap(std::move(should_retry_callback),
                                    content::CopyFromSurfaceResult());
      return;
    }
    auto result_callback = base::BindOnce(
        &TabReadbackRequest::OnFinishGetTabThumbnailBitmap,
        weak_factory_.GetWeakPtr(), std::move(should_retry_callback));
    gfx::Size view_size_in_pixels =
        rwhv->GetNativeView()->GetPhysicalBackingSize();
    if (!rwhv->IsSurfaceAvailableForCopy() || view_size_in_pixels.IsEmpty()) {
      std::move(result_callback).Run(content::CopyFromSurfaceResult());
      return;
    }
    gfx::Rect source_rect = gfx::Rect(view_size_in_pixels);
    gfx::Size thumbnail_size(
        gfx::ScaleToCeiledSize(view_size_in_pixels, thumbnail_scale_));
    rwhv->CopyFromSurface(source_rect, thumbnail_size, kTabReadbackTimeout,
                          std::move(result_callback));
  }

  base::WeakPtr<RetryableTask> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void OnFinishGetTabThumbnailBitmap(
      base::OnceCallback<void(bool)> should_retry_callback,
      const content::CopyFromSurfaceResult& result) {
    if (drop_after_readback_) {
      // Release the capturer count as the request is being dropped.
      decrementor_.RunAndReset();
      if (result_callback_) {
        std::move(result_callback_).Run(0.f, SkBitmap());
      }
      std::move(should_retry_callback).Run(false);
      return;
    }

    if (!result.has_value()) {
      std::move(should_retry_callback).Run(true);
      return;
    }

    // Release the capturer count as the readback was successful.
    decrementor_.RunAndReset();
    SkBitmap result_bitmap = result->bitmap;
    result_bitmap.setImmutable();
    float scale = thumbnail_scale_;
    if (result_callback_) {
      std::move(result_callback_).Run(scale, result_bitmap);
    }

    std::move(should_retry_callback).Run(false);
  }

  void OnRetryLoopFinished(bool success) {
    if (success) {
      // result_callback_ should have already been consumed on the success path.
      DCHECK(!result_callback_);
      return;
    }

    // Release the capturer count as no more retry attempts will be made.
    decrementor_.RunAndReset();

    if (result_callback_) {
      std::move(result_callback_).Run(0.f, SkBitmap());
    }
  }

  void SetToDropAfterReadback() { drop_after_readback_ = true; }

 private:
  base::WeakPtr<content::WebContents> weak_web_contents_;
  const float thumbnail_scale_;
  TabReadbackCallback result_callback_;
  bool drop_after_readback_{false};
  base::ScopedClosureRunner decrementor_;
  std::unique_ptr<RetryStrategy> retry_strategy_;

  base::WeakPtrFactory<TabReadbackRequest> weak_factory_{this};
};

// static
TabContentManager* TabContentManager::FromJavaObject(
    const JavaRef<jobject>& jobj) {
  if (jobj.is_null()) {
    return nullptr;
  }
  return reinterpret_cast<TabContentManager*>(
      Java_TabContentManager_getNativePtr(AttachCurrentThread(), jobj));
}

TabContentManager::TabContentManager(JNIEnv* env,
                                     const jni_zero::JavaRef<jobject>& obj,
                                     int32_t default_cache_size,
                                     int32_t compression_queue_max_size,
                                     int32_t write_queue_max_size,
                                     bool save_jpeg_thumbnails)
    : thumbnail_cache_(static_cast<size_t>(default_cache_size),
                       static_cast<size_t>(compression_queue_max_size),
                       static_cast<size_t>(write_queue_max_size),
                       save_jpeg_thumbnails),
      weak_java_tab_content_manager_(env, obj) {
  thumbnail_cache_.AddThumbnailCacheObserver(this);
}

TabContentManager::~TabContentManager() = default;

void TabContentManager::Destroy(JNIEnv* env) {
  thumbnail_cache_.RemoveThumbnailCacheObserver(this);
  delete this;
}

void TabContentManager::SetUIResourceProvider(
    base::WeakPtr<ui::UIResourceProvider> ui_resource_provider) {
  thumbnail_cache_.SetUIResourceProvider(ui_resource_provider);
}

scoped_refptr<cc::slim::Layer> TabContentManager::GetLiveLayer(int tab_id) {
  JNIEnv* env = AttachCurrentThread();
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

ThumbnailLayer* TabContentManager::GetStaticLayer(int tab_id) {
  if (tab_id == -1) {
    return nullptr;
  }
  auto it = static_layer_cache_.find(tab_id);
  // DCHECK is safe as nullptr is returned if layer is not found.
  DCHECK(it != static_layer_cache_.end())
      << "Missing " << tab_id << " in static_layer_cache_. "
      << "Call UpdateVisibleIds before using a static layer.";
  return it == static_layer_cache_.end() ? nullptr : it->second.get();
}

void TabContentManager::UpdateVisibleIds(const std::vector<int>& priority_ids,
                                         int primary_tab_id) {
  thumbnail_cache_.UpdateVisibleIds(priority_ids, primary_tab_id);
  std::erase_if(static_layer_cache_, [&priority_ids](const auto& pair) {
    bool not_priority = !std::ranges::contains(priority_ids, pair.first);
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
    thumbnail::Thumbnail* thumbnail = thumbnail_cache_.Get(tab_id, false);
    if (thumbnail) {
      static_layer->SetThumbnail(thumbnail);
    }
  }
}

TabContentManager::ThumbnailCaptureTrackerPtr TabContentManager::TrackCapture(
    thumbnail::TabId tab_id) {
  CleanupTrackers();
  ThumbnailCaptureTrackerPtr tracker(
      new thumbnail::ThumbnailCaptureTracker(
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

void TabContentManager::CleanupTrackers() {
  absl::erase_if(in_flight_captures_,
                 [](const auto& pair) -> bool { return !pair.second; });
}

void TabContentManager::CaptureThumbnail(
    JNIEnv* env,
    TabAndroid* tab_android,
    float thumbnail_scale,
    bool return_bitmap,
    const base::android::JavaRef<jobject>& j_callback) {
  // Ensure capture only happens on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(tab_android);
  const int tab_id = tab_android->GetAndroidId();
  bool has_pending_readback = pending_tab_readbacks_.contains(tab_id);

  content::WebContents* web_contents = tab_android->GetContents();
  if (has_pending_readback || !web_contents ||
      web_contents->IsBeingDestroyed() ||
      !thumbnail_cache_.CheckAndUpdateThumbnailMetaData(
          tab_id, tab_android->GetURL(), /*force_update=*/false)) {
    if (j_callback) {
      RunObjectCallbackAndroid(j_callback, nullptr);
    }
    return;
  }

  auto tracker = TrackCapture(tab_id);
  TabReadbackCallback readback_done_callback =
      base::BindOnce(&TabContentManager::OnTabReadback,
                     weak_factory_.GetWeakPtr(), tab_id, std::move(tracker),
                     base::BindOnce(&RunObjectCallbackAndroid,
                                    ScopedJavaGlobalRef<jobject>(j_callback)),
                     return_bitmap);
  pending_tab_readbacks_[tab_id] = std::make_unique<TabReadbackRequest>(
      web_contents, thumbnail_scale, std::move(readback_done_callback));
}

void TabContentManager::CacheTabWithBitmap(JNIEnv* env,
                                           TabAndroid* tab_android,
                                           const JavaRef<jobject>& bitmap,
                                           float thumbnail_scale) {
  DCHECK(tab_android);
  int tab_id = tab_android->GetAndroidId();
  GURL url = tab_android->GetURL();

  gfx::JavaBitmap java_bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
  skbitmap.setImmutable();

  // Native pages have their own throttling behavior so force the update if that
  // happens.
  if (thumbnail_cache_.CheckAndUpdateThumbnailMetaData(
          tab_id, url, tab_android->IsNativePage())) {
    // Use default ctor rather than a base::DoNothing callback to skip extra
    // invoking `SendThumbnailToJava`.
    OnTabReadback(tab_id, TrackCapture(tab_id), JavaBitmapCallback(),
                  /*return_bitmap=*/false, thumbnail_scale, skbitmap);
  }
}

void TabContentManager::InvalidateIfChanged(JNIEnv* env,
                                            int32_t tab_id,
                                            const GURL& url) {
  thumbnail_cache_.InvalidateThumbnailIfChanged(tab_id, url);
}

void TabContentManager::UpdateVisibleIds(JNIEnv* env,
                                         const JavaRef<jintArray>& priority,
                                         int32_t primary_tab_id) {
  std::vector<int> priority_ids;
  base::android::JavaIntArrayToIntVector(env, priority, &priority_ids);
  UpdateVisibleIds(priority_ids, primary_tab_id);
}

void TabContentManager::NativeRemoveTabThumbnail(int tab_id) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);
  if (readback_iter != pending_tab_readbacks_.end()) {
    readback_iter->second->SetToDropAfterReadback();
  }
  thumbnail_cache_.Remove(tab_id);
  in_flight_captures_.erase(tab_id);
}

void TabContentManager::RemoveTabThumbnail(JNIEnv* env, int32_t tab_id) {
  NativeRemoveTabThumbnail(tab_id);
}

void TabContentManager::RemoveAllTabThumbnailsExceptForIds(
    JNIEnv* env,
    std::vector<int> tab_ids) {
  thumbnail_cache_.RemoveAllTabThumbnailsExceptForIds(tab_ids);
}

void TabContentManager::WaitForJpegTabThumbnail(
    JNIEnv* env,
    int32_t tab_id,
    const base::android::JavaRef<jobject>& j_callback) {
  auto it = in_flight_captures_.find(tab_id);
  if (it != in_flight_captures_.end() && it->second) {
    // A capture is currently ongoing wait till it finishes.
    it->second->AddOnJpegFinishedCallback(base::BindOnce(
        &RunBooleanCallbackAndroid, ScopedJavaGlobalRef<jobject>(j_callback)));
  } else {
    // Thumbnail is not currently being captured. Run the callback.
    RunBooleanCallbackAndroid(j_callback, true);
  }
}

void TabContentManager::GetEtc1TabThumbnail(
    JNIEnv* env,
    int32_t tab_id,
    const base::android::JavaRef<jobject>& j_callback) {
  thumbnail_cache_.DecompressEtc1ThumbnailFromFile(
      tab_id,
      base::BindOnce(&TabContentManager::SendThumbnailToJava,
                     weak_factory_.GetWeakPtr(),
                     base::BindOnce(&RunObjectCallbackAndroid,
                                    ScopedJavaGlobalRef<jobject>(j_callback)),
                     /*need_downsampling=*/false));
}

void TabContentManager::OnUIResourcesWereEvicted() {
  thumbnail_cache_.OnUIResourcesWereEvicted();
}

void TabContentManager::OnThumbnailAddedToCache(int tab_id) {
  auto it = static_layer_cache_.find(tab_id);
  if (it != static_layer_cache_.end()) {
    thumbnail::Thumbnail* thumbnail = thumbnail_cache_.Get(tab_id, false);
    it->second->SetThumbnail(thumbnail);
  }
}

void TabContentManager::OnFinishedThumbnailRead(int tab_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabContentManager_notifyListenersOfThumbnailChange(
      env, weak_java_tab_content_manager_.get(env), tab_id);
}

void TabContentManager::OnTabReadback(int tab_id,
                                      ThumbnailCaptureTrackerPtr tracker,
                                      JavaBitmapCallback callback,
                                      bool return_bitmap,
                                      float thumbnail_scale,
                                      const SkBitmap& bitmap) {
  pending_tab_readbacks_.erase(tab_id);

  SendThumbnailToJava(std::move(callback), /*need_downsampling=*/true,
                      return_bitmap, bitmap);

  if (thumbnail_scale > 0 && !bitmap.empty()) {
    thumbnail_cache_.Put(tab_id, std::move(tracker), bitmap, thumbnail_scale);
  } else if (tracker) {
    tracker->MarkCaptureFailed();
  }

  if (!bitmap.empty() &&
      base::FeatureList::IsEnabled(sync_sessions::kSyncTabScreenshots)) {
    // Check that tabs sync is active (in which case
    // `service->GetOpenTabsUIDelegate()` returns non-null) before bothering to
    // compress the screenshot.
    sync_sessions::SessionSyncService* service = GetSessionSyncService(tab_id);
    if (service && service->GetOpenTabsUIDelegate()) {
      auto compression_done_callback = base::BindPostTaskToCurrentDefault(
          base::BindOnce(&TabContentManager::AddTabScreenshotToSync,
                         weak_factory_.GetWeakPtr(), tab_id));
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
          base::BindOnce(&CompressScreenshotForSync, bitmap,
                         std::move(compression_done_callback)));
    }
  }
}

void TabContentManager::SendThumbnailToJava(JavaBitmapCallback callback,
                                            bool need_downsampling,
                                            bool result,
                                            const SkBitmap& bitmap) {
  if (!callback) {
    return;
  }
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!bitmap.isNull() && result) {
    int scale = need_downsampling ? 2 : 1;
    int width = bitmap.width() / scale;
    int height = bitmap.height() / scale;

    SkIRect dest_subset = {0, 0, width, height};

    j_bitmap = gfx::ConvertToJavaBitmap(skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BETTER, width, height,
        dest_subset));
  }
  std::move(callback).Run(j_bitmap);
}

void TabContentManager::AddTabScreenshotToSync(int tab_id,
                                               std::string compressed_data) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab = Java_TabContentManager_getTabById(
      env, weak_java_tab_content_manager_.get(env), tab_id);
  if (!jtab) {
    return;
  }
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  if (!tab || !tab->web_contents()) {
    return;
  }
  sync_sessions::SessionSyncService* service = GetSessionSyncService(tab_id);
  if (!service) {
    return;
  }
  SessionID session_id =
      browser_sync::SyncedTabDelegateAndroid::SessionIdFromAndroidId(tab_id);
  service->AddTabScreenshot(session_id, std::move(compressed_data),
                            tab->web_contents()->GetLastCommittedURL());
}

sync_sessions::SessionSyncService* TabContentManager::GetSessionSyncService(
    int tab_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab = Java_TabContentManager_getTabById(
      env, weak_java_tab_content_manager_.get(env), tab_id);
  if (!jtab) {
    return nullptr;
  }
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (!tab_android || !tab_android->web_contents()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(
      tab_android->web_contents()->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return SessionSyncServiceFactory::GetForProfile(profile);
}

void TabContentManager::SetCaptureMinRequestTimeForTesting(JNIEnv* env,
                                                           int32_t time_ms) {
  thumbnail_cache_.SetCaptureMinRequestTimeForTesting(time_ms);
}

bool TabContentManager::IsTabCaptureInFlightForTesting(JNIEnv* env,
                                                       int32_t tab_id) {
  return in_flight_captures_.find(tab_id) != in_flight_captures_.end();
}

// static
void TabContentManager::CompressScreenshotForSyncForTesting(  // IN-TEST
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::string)> callback) {
  CompressScreenshotForSync(bitmap, std::move(callback));
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static int64_t JNI_TabContentManager_Init(JNIEnv* env,
                                          const JavaRef<jobject>& obj,
                                          int32_t default_cache_size,
                                          int32_t compression_queue_max_size,
                                          int32_t write_queue_max_size,
                                          bool save_jpeg_thumbnails) {
  // Ensure this and its thumbnail cache are created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  TabContentManager* manager = new TabContentManager(
      env, obj, default_cache_size, compression_queue_max_size,
      write_queue_max_size, save_jpeg_thumbnails);
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace android

DEFINE_JNI(TabContentManager)
