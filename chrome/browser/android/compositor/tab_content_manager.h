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
                    jobject obj,
                    jint default_cache_size,
                    jint approximation_cache_size,
                    jint compression_queue_max_size,
                    jint write_queue_max_size,
                    jboolean use_approximation_thumbnail,
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

  // Deprecated: This will be replace by just GetStaticLayer soon.
  // Get the static thumbnail from the cache, or the NTP.
  ThumbnailLayer* GetOrCreateStaticLayer(int tab_id, bool force_disk_read);
  // JNI methods.

  // Should be called when a tab gets a new live layer that should be served
  // by the cache to the CompositorView.
  void AttachTab(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jtab,
                 jint tab_id);

  // Should be called when a tab removes a live layer because it should no
  // longer be served by the CompositorView.  If `layer` is nullptr, will
  // make sure all live layers are detached.
  void DetachTab(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jtab,
                 jint tab_id);
  void CaptureThumbnail(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& tab,
                        jfloat thumbnail_scale,
                        jboolean write_to_cache,
                        jdouble aspect_ratio,
                        const base::android::JavaParamRef<jobject>& j_callback);
  void CacheTabWithBitmap(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& tab,
                          const base::android::JavaParamRef<jobject>& bitmap,
                          jfloat thumbnail_scale,
                          jdouble aspect_ratio);
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
      jdouble aspect_ratio,
      const base::android::JavaParamRef<jobject>& j_callback);
  void SetCaptureMinRequestTimeForTesting(JNIEnv* env, jint timeMs);
  jint GetPendingReadbacksForTesting(JNIEnv* env);

  // ThumbnailCacheObserver implementation;
  void OnThumbnailAddedToCache(thumbnail::TabId tab_id) override;
  void OnFinishedThumbnailRead(thumbnail::TabId tab_id) override;

 private:
  class TabReadbackRequest;
  // TODO(crbug/714384) check sizes and consider using base::flat_map if these
  // layer maps are small.
  using LayerMap = std::map<int, scoped_refptr<cc::slim::Layer>>;
  using ThumbnailLayerMap = std::map<int, scoped_refptr<ThumbnailLayer>>;
  using TabReadbackRequestMap =
      base::flat_map<int, std::unique_ptr<TabReadbackRequest>>;

  content::RenderWidgetHostView* GetRwhvForTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& tab);
  std::unique_ptr<thumbnail::ThumbnailCaptureTracker, base::OnTaskRunnerDeleter>
  TrackCapture(thumbnail::TabId tab_id);
  void OnTrackingFinished(int tab_id,
                          thumbnail::ThumbnailCaptureTracker* tracker);
  void OnTabReadback(int tab_id,
                     std::unique_ptr<thumbnail::ThumbnailCaptureTracker,
                                     base::OnTaskRunnerDeleter> tracker,
                     base::android::ScopedJavaGlobalRef<jobject> j_callback,
                     bool write_to_cache,
                     double aspect_ratio,
                     float thumbnail_scale,
                     const SkBitmap& bitmap);

  void SendThumbnailToJava(
      base::android::ScopedJavaGlobalRef<jobject> j_callback,
      bool need_downsampling,
      double aspect_ratio,
      bool result,
      const SkBitmap& bitmap);

  base::flat_map<thumbnail::TabId,
                 base::WeakPtr<thumbnail::ThumbnailCaptureTracker>>
      in_flight_captures_;
  std::unique_ptr<thumbnail::ThumbnailCache> thumbnail_cache_;
  ThumbnailLayerMap static_layer_cache_;
  LayerMap live_layer_list_;
  TabReadbackRequestMap pending_tab_readbacks_;

  JavaObjectWeakGlobalRef weak_java_tab_content_manager_;
  base::WeakPtrFactory<TabContentManager> weak_factory_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
