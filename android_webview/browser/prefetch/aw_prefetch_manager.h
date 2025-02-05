// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace android_webview {

// Manages prefetch operations for this Profile.
// Lifetime: Profile
class AwPrefetchManager {
 public:
  explicit AwPrefetchManager(content::BrowserContext* browser_context);

  AwPrefetchManager(const AwPrefetchManager&) = delete;
  AwPrefetchManager& operator=(const AwPrefetchManager&) = delete;

  ~AwPrefetchManager();

  void StartBrowserPrefetchRequest(
      const GURL& url,
      bool javascript_enabled,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      const net::HttpRequestHeaders& additional_headers,
      std::unique_ptr<content::PrefetchRequestStatusListener>
          request_status_listener);

  void StartPrefetchRequest(
      JNIEnv* env,
      const std::string& url,
      const base::android::JavaParamRef<jobject>& prefetch_params,
      const base::android::JavaParamRef<jobject>& callback,
      const base::android::JavaParamRef<jobject>& callback_executor);

  // Updates the TTL and maximum number of prefetches.
  void UpdatePrefetchConfiguration(JNIEnv* env,
                                   jint ttl_in_sec,
                                   jint max_prefetches);

  // Returns the Time-to-Live (TTL) for prefetched content in seconds.
  int GetTtlInSec() const { return ttl_in_sec_; }

  // Returns the maximum number of allowed prefetches in cache.
  int GetMaxPrefetches() const { return max_prefetches_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaPrefetchManager();

 private:
  raw_ref<content::BrowserContext> browser_context_;

  int ttl_in_sec_;

  int max_prefetches_;

  // Java object reference.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_MANAGER_H_
