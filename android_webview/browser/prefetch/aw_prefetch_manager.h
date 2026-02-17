// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_

#include <jni.h>

#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
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
inline constexpr int kDefaultTtlInSec = 60;
// The MaxPrefetches number is not present in the `//content` layer, so it is
// specific to WebView.
inline constexpr size_t kDefaultMaxPrefetches = 10;
// This is the source of truth for the absolute maximum number of prefetches
// that can ever be cached in WebView. It can override the number set by the
// AndroidX API.
inline constexpr int32_t kAbsoluteMaxPrefetches = 20;
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
      const base::android::JavaRef<jobject>& prefetch_params,
      const base::android::JavaRef<jobject>& callback,
      const base::android::JavaRef<jobject>& callback_executor);

  void CancelPrefetch(JNIEnv* env, int32_t prefetch_key);

  // Registers an external experiment (synthetic trial) in UMA for the current
  // prefetch request. The experiment ID is derived from the Variations ID
  // provided by the embedder.
  //
  // This is called during `StartPrefetchRequest()` to ensure the metrics state
  // reflects the parameters of the most recent request. If no Variations ID is
  // provided, any previously registered prefetch experiment will be cleared.
  void SetOrClearExternalPrefetchExperiment(std::optional<int> variations_id);

  bool GetIsPrefetchInCacheForTesting(JNIEnv* env, int32_t prefetch_key);

  // Updates Time-To-Live (TTL) for the prefetched content in seconds.
  void SetTtlInSec(JNIEnv* env, std::optional<int> ttl_in_sec) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    int sanitized_ttl_in_sec = ttl_in_sec.value_or(kDefaultTtlInSec);

    CHECK_GT(sanitized_ttl_in_sec, 0);
    ttl_in_sec_ = sanitized_ttl_in_sec;
  }

  // Updates the maximum number of allowed prefetches in cache
  void SetMaxPrefetches(JNIEnv* env, std::optional<int> max_prefetches) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    int sanitized_max_prefetches =
      max_prefetches.value_or(kDefaultMaxPrefetches);

    CHECK_GT(sanitized_max_prefetches, 0);
    max_prefetches_ = std::min(sanitized_max_prefetches, kAbsoluteMaxPrefetches);
  }

  // Returns the Time-to-Live (TTL) for prefetched content in seconds.
  int32_t GetTtlInSec(JNIEnv* env) const { return ttl_in_sec_; }

  // Returns the maximum number of allowed prefetches in cache.
  int32_t GetMaxPrefetches(JNIEnv* env) const { return max_prefetches_; }

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

  int ttl_in_sec_ = kDefaultTtlInSec;

  size_t max_prefetches_ = kDefaultMaxPrefetches;

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
