// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/prefetch_handle.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace android_webview {

// The default TTL value in `//content` is 10 minutes which is too long for most
// of WebView cases. This value here can change in the future and that shouldn't
// affect the `//content` TTL default value.
inline constexpr int DEFAULT_TTL_IN_SEC = 60;
// The MaxPrefetches number is not present in the `//content` layer, so it is
// specific to WebView.
inline constexpr int DEFAULT_MAX_PREFETCHES = 10;
// This is the source of truth for the absolute maximum number of prefetches
// that can ever be cached in WebView. It can override the number set by the
// AndroidX API.
inline constexpr int ABSOLUTE_MAX_PREFETCHES = 20;

// Manages prefetch operations for this Profile.
// Lifetime: Profile
class AwPrefetchManager {
 public:
  explicit AwPrefetchManager(content::BrowserContext* browser_context);

  AwPrefetchManager(const AwPrefetchManager&) = delete;
  AwPrefetchManager& operator=(const AwPrefetchManager&) = delete;

  ~AwPrefetchManager();

  void StartPrefetchRequest(
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaParamRef<jobject>& prefetch_params,
      const base::android::JavaParamRef<jobject>& callback,
      const base::android::JavaParamRef<jobject>& callback_executor);

  // Updates Time-To-Live (TTL) for the prefetched content in seconds.
  void SetTtlInSec(JNIEnv* env, jint ttl_in_sec) { ttl_in_sec_ = ttl_in_sec; }

  // Updates the maximum number of allowed prefetches in cache
  void SetMaxPrefetches(JNIEnv* env, jint max_prefetches) {
    max_prefetches_ = std::min(max_prefetches, ABSOLUTE_MAX_PREFETCHES);
  }

  // Returns the Time-to-Live (TTL) for prefetched content in seconds.
  int GetTtlInSec(JNIEnv* env) const { return ttl_in_sec_; }

  // Returns the maximum number of allowed prefetches in cache.
  int GetMaxPrefetches(JNIEnv* env) const { return max_prefetches_; }

  std::vector<content::PrefetchHandle*> GetAllPrefetchesForTesting() const {
    std::vector<content::PrefetchHandle*> raw_prefetches;
    raw_prefetches.reserve(all_prefetches_.size());
    for (const auto& prefetch : all_prefetches_) {
      raw_prefetches.push_back(prefetch.get());
    }
    return raw_prefetches;
  }

  base::android::ScopedJavaLocalRef<jobject> GetJavaPrefetchManager();

 private:
  raw_ref<content::BrowserContext> browser_context_;

  int ttl_in_sec_ = DEFAULT_TTL_IN_SEC;

  int max_prefetches_ = DEFAULT_MAX_PREFETCHES;

  base::circular_deque<std::unique_ptr<content::PrefetchHandle>>
      all_prefetches_;

  // Java object reference.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
