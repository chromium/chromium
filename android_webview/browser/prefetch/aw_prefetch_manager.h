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
inline constexpr size_t DEFAULT_MAX_PREFETCHES = 10;
// This is the source of truth for the absolute maximum number of prefetches
// that can ever be cached in WebView. It can override the number set by the
// AndroidX API.
inline constexpr int32_t ABSOLUTE_MAX_PREFETCHES = 20;
// Returned from `AwPrefetchManager::StartPrefetchRequest` if the prefetch
// request was unsuccessful (i.e. there is no key for the prefetch).
inline constexpr int NO_PREFETCH_KEY = -1;

// Manages prefetch operations for this Profile.
// Lifetime: Profile
class AwPrefetchManager {
 public:
  explicit AwPrefetchManager(content::BrowserContext* browser_context);

  AwPrefetchManager(const AwPrefetchManager&) = delete;
  AwPrefetchManager& operator=(const AwPrefetchManager&) = delete;

  ~AwPrefetchManager();

  // Returns the key associated with the outgoing prefetch request
  // and thus the prefetch handle inside of `all_prefetches_map_` (if
  // successful), otherwise returns `NO_PREFETCH_KEY`.
  int StartPrefetchRequest(
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaParamRef<jobject>& prefetch_params,
      const base::android::JavaParamRef<jobject>& callback,
      const base::android::JavaParamRef<jobject>& callback_executor);

  void CancelPrefetch(JNIEnv* env, jint prefetch_key);

  bool GetIsPrefetchInCacheForTesting(JNIEnv* env, jint prefetch_key);

  // Updates Time-To-Live (TTL) for the prefetched content in seconds.
  void SetTtlInSec(JNIEnv* env, jint ttl_in_sec) { ttl_in_sec_ = ttl_in_sec; }

  // Updates the maximum number of allowed prefetches in cache
  void SetMaxPrefetches(JNIEnv* env, jint max_prefetches) {
    max_prefetches_ = std::min(max_prefetches, ABSOLUTE_MAX_PREFETCHES);
  }

  // Returns the Time-to-Live (TTL) for prefetched content in seconds.
  jint GetTtlInSec(JNIEnv* env) const { return ttl_in_sec_; }

  // Returns the maximum number of allowed prefetches in cache.
  jint GetMaxPrefetches(JNIEnv* env) const { return max_prefetches_; }

  // Returns the key associated with the prefetch handle inside of
  // `all_prefetches_map_`.
  int AddPrefetchHandle(
      std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
    CHECK(prefetch_handle);
    CHECK(max_prefetches_ > 0u);

    // Make room for the new prefetch request by evicting the older ones.
    if (all_prefetches_map_.size() >= max_prefetches_) {
      int num_prefetches_to_evict =
          all_prefetches_map_.size() - max_prefetches_ + 1;
      auto it = all_prefetches_map_.begin();

      while (num_prefetches_to_evict > 0 && it != all_prefetches_map_.end()) {
        // Because the keys should be sequential based on when the prefetch
        // associated with it was added, a standard iteration should always
        // prioritize removing the oldest entry.
        it = all_prefetches_map_.erase(it);
        num_prefetches_to_evict--;
      }
    }

    const int32_t new_prefetch_key = GetNextPrefetchKey();
    all_prefetches_map_[new_prefetch_key] = std::move(prefetch_handle);
    UpdateLastPrefetchKey(new_prefetch_key);
    return new_prefetch_key;
  }

  std::vector<content::PrefetchHandle*> GetAllPrefetchesForTesting() const {
    std::vector<content::PrefetchHandle*> raw_prefetches;
    raw_prefetches.reserve(all_prefetches_map_.size());
    for (const auto& prefetch_pair : all_prefetches_map_) {
      raw_prefetches.push_back(prefetch_pair.second.get());
    }
    return raw_prefetches;
  }

  int GetLastPrefetchKeyForTesting() const { return last_prefetch_key_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaPrefetchManager();

 private:
  raw_ref<content::BrowserContext> browser_context_;

  int ttl_in_sec_ = DEFAULT_TTL_IN_SEC;

  size_t max_prefetches_ = DEFAULT_MAX_PREFETCHES;

  std::map<int32_t, std::unique_ptr<content::PrefetchHandle>>
      all_prefetches_map_;

  // Java object reference.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Should only be incremented. Acts as an "order added" mechanism
  // inside of `all_prefetches_map_` since `std::map` stores keys
  // in a sorted order.
  int32_t last_prefetch_key_ = -1;

  int32_t GetNextPrefetchKey() const { return last_prefetch_key_ + 1; }

  void UpdateLastPrefetchKey(int new_key) {
    CHECK(new_key > last_prefetch_key_);
    last_prefetch_key_ = new_key;
  }
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
