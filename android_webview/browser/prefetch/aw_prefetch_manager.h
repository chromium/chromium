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
#include "services/network/public/cpp/resource_request.h"
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

// The suffix used for generating `//content` prefetch internal histogram names
// recorded per trigger.
// TODO(crbug.com/379140429): Merge this with prerender one.
inline constexpr char AW_PREFETCH_METRICS_SUFFIX[] = "WebView";

// Manages prefetch operations for this Profile.
// Lifetime: Profile
class AwPrefetchManager {
 public:
  explicit AwPrefetchManager(content::BrowserContext* browser_context);

  AwPrefetchManager(const AwPrefetchManager&) = delete;
  AwPrefetchManager& operator=(const AwPrefetchManager&) = delete;

  ~AwPrefetchManager();

  // Returns `true` if the `resource_request` is also a prefetch request.
  // NOTE: A prefetch request can also be a prerender request i.e.
  // this method & `IsPrerenderRequest` can both return `true` for it,
  // however this is not always the case.
  static bool IsPrefetchRequest(
      const network::ResourceRequest& resource_request);

  // Returns `true` if the `resource_request` is also a prerender request.
  // NOTE: A prerender request will always be a prefetch request i.e.
  // this method & `IsPrefetchRequest` will always return `true` for it
  // as prefetching a always required for prerendering.
  static bool IsPrerenderRequest(
      const network::ResourceRequest& resource_request);

  // Returns `true` if the `blink::kSecPurposeHeaderName` header is associated
  // with a prefetch request.
  static bool IsSecPurposeForPrefetch(
      std::optional<std::string> sec_purpose_header_value);

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
    CHECK(all_prefetches_map_.size() < max_prefetches_);

    const int32_t new_prefetch_key = GetNextPrefetchKey();
    all_prefetches_map_[new_prefetch_key] = std::move(prefetch_handle);
    UpdateLastPrefetchKey(new_prefetch_key);
    return new_prefetch_key;
  }

  std::vector<int32_t> GetAllPrefetchKeysForTesting() const {
    std::vector<int32_t> prefetch_keys;
    prefetch_keys.reserve(all_prefetches_map_.size());
    for (const auto& prefetch_pair : all_prefetches_map_) {
      prefetch_keys.push_back(prefetch_pair.first);
    }
    return prefetch_keys;
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
