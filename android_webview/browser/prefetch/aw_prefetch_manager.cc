// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include <jni.h>

#include <optional>

#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPrefetchManager_jni.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

using content::BrowserThread;

namespace android_webview {

class AwPrefetchRequestStatusListener
    : public content::PrefetchRequestStatusListener {
 public:
  AwPrefetchRequestStatusListener(
      const base::android::ScopedJavaGlobalRef<jobject>
          prefetch_manager_java_object,
      const base::android::JavaRef<jobject>& callback,
      const base::android::JavaRef<jobject>& callback_executor)
      : prefetch_manager_java_object_(prefetch_manager_java_object),
        prefetch_java_callback_(callback),
        prefetch_java_callback_executor_(callback_executor) {}
  ~AwPrefetchRequestStatusListener() override = default;

  void OnPrefetchStartFailedGeneric() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchStartFailedGeneric(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchStartFailedDuplicate() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchStartFailedDuplicate(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseCompleted() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseCompleted(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseError() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseError(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseServerError(int response_code) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseServerError(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_, response_code);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> prefetch_manager_java_object_;
  base::android::ScopedJavaGlobalRef<jobject> prefetch_java_callback_;
  base::android::ScopedJavaGlobalRef<jobject> prefetch_java_callback_executor_;
};

AwPrefetchManager::AwPrefetchManager(content::BrowserContext* browser_context)
    : browser_context_(*browser_context) {
  TRACE_EVENT_INSTANT("android_webview",
                      "AwPrefetchManager::AwPrefetchManager");
}

AwPrefetchManager::~AwPrefetchManager() = default;

bool AwPrefetchManager::IsPrefetchRequest(
    const network::ResourceRequest& resource_request) {
  return AwPrefetchManager::IsSecPurposeForPrefetch(
      resource_request.headers.GetHeader(blink::kSecPurposeHeaderName));
}

bool AwPrefetchManager::IsPrerenderRequest(
    const network::ResourceRequest& resource_request) {
  return blink::IsSecPurposeForPrerender(
      resource_request.headers.GetHeader(blink::kSecPurposeHeaderName));
}

bool AwPrefetchManager::IsSecPurposeForPrefetch(
    std::optional<std::string> sec_purpose_header_value) {
  return blink::IsSecPurposeForPrefetch(sec_purpose_header_value);
}

int AwPrefetchManager::StartPrefetchRequest(
    JNIEnv* env,
    const std::string& url,
    const base::android::JavaParamRef<jobject>& prefetch_params,
    const base::android::JavaParamRef<jobject>& callback,
    const base::android::JavaParamRef<jobject>& callback_executor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview", "AwPrefetchManager::StartPrefetchRequest");

  GURL pf_url = GURL(url);
  net::HttpRequestHeaders additional_headers =
      GetAdditionalHeadersFromPrefetchParameters(env, prefetch_params);
  std::optional<net::HttpNoVarySearchData> expected_no_vary_search =
      GetExpectedNoVarySearchFromPrefetchParameters(env, prefetch_params);
  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<AwPrefetchRequestStatusListener>(java_obj_, callback,
                                                            callback_executor);

  // For WebView we will check if there is already a duplicate
  // prefetch request based on the URL and the No-Vary-Search hint. This is for
  // the purpose of deduping prefetch requests on the application's behalf.
  // TODO(crbug.com/393344309): Apply deduping to all prefetch requests (not
  // just WebView).
  if (!browser_context_->IsPrefetchDuplicate(pf_url, expected_no_vary_search)) {
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

    std::unique_ptr<content::PrefetchHandle> prefetch_handle =
        browser_context_->StartBrowserPrefetchRequest(
            pf_url, AW_PREFETCH_METRICS_SUFFIX,
            GetIsJavaScriptEnabledFromPrefetchParameters(env, prefetch_params),
            expected_no_vary_search, additional_headers,
            std::move(request_status_listener), base::Seconds(ttl_in_sec_),
            /*should_append_variations_header=*/false);

    if (prefetch_handle) {
      return AddPrefetchHandle(std::move(prefetch_handle));
    }
  } else {
    request_status_listener->OnPrefetchStartFailedDuplicate();
  }
  return NO_PREFETCH_KEY;
}

void AwPrefetchManager::CancelPrefetch(JNIEnv* env, jint prefetch_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview", "AwPrefetchManager::CancelPrefetch");
  if (prefetch_key == NO_PREFETCH_KEY) {
    // no-op.
    return;
  }

  auto it = all_prefetches_map_.find(prefetch_key);
  if (it != all_prefetches_map_.end()) {
    all_prefetches_map_.erase(it);
  }
}

bool AwPrefetchManager::GetIsPrefetchInCacheForTesting(JNIEnv* env,
                                                       jint prefetch_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return all_prefetches_map_.find(prefetch_key) != all_prefetches_map_.end();
}

jint JNI_AwPrefetchManager_GetNoPrefetchKey(JNIEnv* env) {
  return NO_PREFETCH_KEY;
}

jboolean JNI_AwPrefetchManager_IsSecPurposeForPrefetch(
    JNIEnv* env,
    std::string& sec_purpose_header_value) {
  return AwPrefetchManager::IsSecPurposeForPrefetch(sec_purpose_header_value);
}

base::android::ScopedJavaLocalRef<jobject>
AwPrefetchManager::GetJavaPrefetchManager() {
  if (!java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_ =
        Java_AwPrefetchManager_create(env, reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace android_webview
