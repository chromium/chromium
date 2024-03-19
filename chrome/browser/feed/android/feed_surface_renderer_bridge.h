// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_FEED_SURFACE_RENDERER_BRIDGE_H_
#define CHROME_BROWSER_FEED_ANDROID_FEED_SURFACE_RENDERER_BRIDGE_H_

#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/feed/android/feed_reliability_logging_bridge.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/surface_renderer.h"

namespace feedui {
class StreamUpdate;
}

namespace feed::android {

// Native access to |FeedSurfaceRendererBridge| in Java.
// Created for each feed surface (for-you, following feed), and for each
// NTP/start surface instance.
class FeedSurfaceRendererBridge : public ::feed::SurfaceRenderer {
 public:
  explicit FeedSurfaceRendererBridge(
      const base::android::JavaRef<jobject>& j_this,
      jint stream_kind,
      std::string web_feed_id,
      FeedReliabilityLoggingBridge* reliability_logging_bridge,
      jint feed_entry_point);
  FeedSurfaceRendererBridge(const FeedSurfaceRendererBridge&) = delete;
  FeedSurfaceRendererBridge& operator=(const FeedSurfaceRendererBridge&) =
      delete;

  ~FeedSurfaceRendererBridge() override;

  void Destroy(JNIEnv* env);

  // SurfaceRenderer implementation.
  void StreamUpdate(const feedui::StreamUpdate& update) override;
  void ReplaceDataStoreEntry(std::string_view key,
                             std::string_view data) override;
  void RemoveDataStoreEntry(std::string_view key) override;

  ReliabilityLoggingBridge& GetReliabilityLoggingBridge() override;

  void OnStreamUpdated(const feedui::StreamUpdate& stream_update);

  void LoadMore(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& callback_obj);

  void ManualRefresh(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& callback_obj);

  void SurfaceOpened(JNIEnv* env);
  void SurfaceClosed(JNIEnv* env);
  int GetSurfaceId(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<FeedApi> feed_stream_api_;
  SurfaceId surface_id_;
  bool attached_ = false;
  raw_ptr<FeedReliabilityLoggingBridge> reliability_logging_bridge_ = nullptr;
};

}  // namespace feed::android

#endif  // CHROME_BROWSER_FEED_ANDROID_FEED_SURFACE_RENDERER_BRIDGE_H_
