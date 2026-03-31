// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_

#include <jni.h>

#include <optional>
#include <vector>

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"
#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"
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
  AwPrefetchKey StartPrefetchRequest(
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaRef<jobject>& prefetch_params,
      const base::android::JavaRef<jobject>& callback,
      const base::android::JavaRef<jobject>& callback_executor);
  // From `AwContents::StartPrerendering()`.
  AwPrefetchKey StartPrefetchRequestAheadOfPrerender(
      base::PassKey<AwContents>,
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaRef<jobject>& prefetch_params,
      scoped_refptr<content::PreloadPipelineInfo> preload_pipeline_info);

  void CancelPrefetch(JNIEnv* env, AwPrefetchKey prefetch_key);

  // Updates Time-To-Live (TTL) for the prefetched content in seconds.
  void SetTtlInSec(JNIEnv* env, std::optional<int> ttl_in_sec);

  // Updates the maximum number of allowed prefetches in cache
  void SetMaxPrefetches(JNIEnv* env, std::optional<int> max_prefetches);

  // Returns the Time-to-Live (TTL) for prefetched content in seconds.
  int GetTtlInSecForTesting(JNIEnv* env) const;

  // Returns the maximum number of allowed prefetches in cache.
  size_t GetMaxPrefetchesForTesting(JNIEnv* env) const;

  std::vector<AwPrefetchKey> GetAllPrefetchKeysForTesting() const;

  AwPrefetchKey GetLastPrefetchKeyForTesting() const;

  bool GetIsPrefetchInCacheForTesting(JNIEnv* env,
                                      AwPrefetchKey prefetch_key) const;

  base::android::ScopedJavaLocalRef<jobject> GetJavaPrefetchManager();

 private:
  // Registers an external experiment (synthetic trial) in UMA for the current
  // prefetch request. The experiment ID is derived from the Variations ID
  // provided by the embedder.
  //
  // This is called during `StartPrefetchRequest()` to ensure the metrics state
  // reflects the parameters of the most recent request. If no Variations ID is
  // provided, any previously registered prefetch experiment will be cleared.
  static void SetOrClearExternalPrefetchExperiment(
      std::optional<int> variations_id);

  AwPrefetchKey StartPrefetchRequestInternal(
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaRef<jobject>& prefetch_params,
      const base::android::JavaRef<jobject>& callback,
      const base::android::JavaRef<jobject>& callback_executor,
      scoped_refptr<content::PreloadPipelineInfo> preload_pipeline_info);

  const raw_ref<content::BrowserContext> browser_context_;

  // Manages all the mutable states of `this`.
  AwPrefetchManagerData aw_prefetch_manager_data_;

  // Java object reference.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
