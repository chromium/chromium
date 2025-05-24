// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_

#include <jni.h>

#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/thumbnail/cc/thumbnail_cache.h"
#include "content/public/browser/render_widget_host_view.h"

using base::android::ScopedJavaLocalRef;

namespace cc::slim {
class Layer;
}

namespace ui {
class UIResourceProvider;
}

namespace android {

class ThumbnailLayer;

// A native component of the Java TabContentManager class.
class TabContentManager : public thumbnail::ThumbnailCacheObserver {
 public:
  static TabContentManager* FromJavaObject(
      const base::android::JavaRef<jobject>& jobj);

  TabContentManager(JNIEnv* env,
                    const jni_zero::JavaRef<jobject>& obj,
                    jint default_cache_size,
                    jint compression_queue_max_size,
                    jint write_queue_max_size,
                    jboolean save_jpeg_thumbnails);

  TabContentManager(const TabContentManager&) = delete;
  TabContentManager& operator=(const TabContentManager&) = delete;

  virtual ~TabContentManager();

  void Destroy(JNIEnv* env);

  void SetUIResourceProvider(
      base::WeakPtr<ui::UIResourceProvider> ui_resource_provider);

  // Get the live layer from the cache.
  scoped_refptr<cc::slim::Layer> GetLiveLayer(int tab_id);

  // Returns the static ThumbnailLayer for a `tab_id`. Note that the lifecycle
  // of the thumbnail is managed by the ThumbnailCache and not the
  // ThumbnailLayer. When displaying a layer it is important that
  // UpdateVisibleIds is called with all the Tab IDs that are required for
  // before calling GetStaticLayer. ThumbnailLayer's should not be retained as
  // their lifecycle is managed by this class.
  ThumbnailLayer* GetStaticLayer(int tab_id);

  // JNI methods.

  // Updates visible tab ids to page into the thumbnail cache.
  void UpdateVisibleIds(const std::vector<int>& priority_ids,
                        int primary_tab_id);

  void CaptureThumbnail(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& tab,
                        jfloat thumbnail_scale,
                        jboolean return_bitmap,
                        const base::android::JavaParamRef<jobject>& j_callback);
  void CacheTabWithBitmap(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& tab,
                          const base::android::JavaParamRef<jobject>& bitmap,
                          jfloat thumbnail_scale);
  void InvalidateIfChanged(JNIEnv* env,
                           jint tab_id,
                           const base::android::JavaParamRef<jobject>& jurl);
  void UpdateVisibleIds(JNIEnv* env,
                        const base::android::JavaParamRef<jintArray>& priority,
                        jint primary_tab_id);
  void NativeRemoveTabThumbnail(int tab_id);
  void RemoveTabThumbnail(JNIEnv* env, jint tab_id);
  void OnUIResourcesWereEvicted();
  void WaitForJpegTabThumbnail(
      JNIEnv* env,
      jint tab_id,
      const base::android::JavaParamRef<jobject>& j_callback);
  void GetEtc1TabThumbnail(
      JNIEnv* env,
      jint tab_id,
      const base::android::JavaParamRef<jobject>& j_callback);
  void SetCaptureMinRequestTimeForTesting(JNIEnv* env, jint timeMs);
  jboolean IsTabCaptureInFlightForTesting(JNIEnv* env, jint tab_id);

  // ThumbnailCacheObserver implementation;
  void OnThumbnailAddedToCache(thumbnail::TabId tab_id) override;
  void OnFinishedThumbnailRead(thumbnail::TabId tab_id) override;

 private:
  class TabReadbackRequest;
  // TODO(crbug.com/41314695) check sizes and consider using base::flat_map if
  // these layer maps are small.
  using ThumbnailLayerMap = std::map<int, scoped_refptr<ThumbnailLayer>>;
  using TabReadbackRequestMap =
      base::flat_map<int, std::unique_ptr<TabReadbackRequest>>;

  content::RenderWidgetHostView* GetRwhvForTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& tab);
  std::unique_ptr<thumbnail::ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
  TrackCapture(thumbnail::TabId tab_id);
  void CleanupTrackers();
  void OnTrackingFinished(int tab_id,
                          thumbnail::ThumbnailCaptureTracker* tracker);
  void OnTabReadback(int tab_id,
                     std::unique_ptr<thumbnail::ThumbnailCaptureTracker,
                                     base::OnTaskRunnerDeleter> tracker,
                     base::android::ScopedJavaGlobalRef<jobject> j_callback,
                     bool return_bitmap,
                     float thumbnail_scale,
                     const SkBitmap& bitmap);

  void SendThumbnailToJava(
      base::android::ScopedJavaGlobalRef<jobject> j_callback,
      bool need_downsampling,
      bool result,
      const SkBitmap& bitmap);

  base::flat_map<thumbnail::TabId,
                 base::WeakPtr<thumbnail::ThumbnailCaptureTracker>>
      in_flight_captures_;
  std::unique_ptr<thumbnail::ThumbnailCache> thumbnail_cache_;
  ThumbnailLayerMap static_layer_cache_;
  TabReadbackRequestMap pending_tab_readbacks_;

  JavaObjectWeakGlobalRef weak_java_tab_content_manager_;
  base::WeakPtrFactory<TabContentManager> weak_factory_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
