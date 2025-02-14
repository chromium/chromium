// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include <jni.h>

#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPrefetchManager_jni.h"

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

  void OnPrefetchStartFailed() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchStartFailed(
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
    : browser_context_(*browser_context) {}

AwPrefetchManager::~AwPrefetchManager() = default;

void AwPrefetchManager::StartPrefetchRequest(
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
  // Check if we are trying to exceed the limit.
  if (all_prefetches_.size() >= static_cast<uint>(max_prefetches_)) {
    // Now remove the oldest prefetch, making it out of scope should trigger
    // the destructor which handles the reset needed.
    all_prefetches_.pop_front();
  }
  std::unique_ptr<content::PrefetchHandle> prefetch_handle =
      browser_context_->StartBrowserPrefetchRequest(
          pf_url,
          GetIsJavaScriptEnabledFromPrefetchParameters(env, prefetch_params),
          expected_no_vary_search, additional_headers,
          std::move(request_status_listener), base::Seconds(ttl_in_sec_));

  if (prefetch_handle) {
    all_prefetches_.push_back(std::move(prefetch_handle));
  }
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
