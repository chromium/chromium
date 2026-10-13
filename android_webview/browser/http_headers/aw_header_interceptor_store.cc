// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/http_headers/aw_header_interceptor_store.h"

#include <memory>
#include <utility>
#include <vector>

#include "android_webview/browser/request_matcher/aw_request_matcher.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwHeaderInterceptorStore_jni.h"

namespace android_webview {

AwHeaderInterceptorStore::InterceptorEntry::InterceptorEntry(
    int32_t id,
    std::unique_ptr<AwRequestMatcher> request_matcher,
    base::android::ScopedJavaGlobalRef<jobject> interceptor)
    : id(id),
      request_matcher(std::move(request_matcher)),
      interceptor(std::move(interceptor)) {}
AwHeaderInterceptorStore::InterceptorEntry::~InterceptorEntry() = default;

AwHeaderInterceptorStore::AwHeaderInterceptorStore() = default;

AwHeaderInterceptorStore::~AwHeaderInterceptorStore() = default;

AwHeaderInterceptorStore::AwHeaderInterceptorStore(AwHeaderInterceptorStore&&) =
    default;

AwHeaderInterceptorStore& AwHeaderInterceptorStore::operator=(
    AwHeaderInterceptorStore&&) = default;

int32_t AwHeaderInterceptorStore::AddInterceptor(
    base::android::ScopedJavaGlobalRef<jobject> interceptor,
    std::unique_ptr<AwRequestMatcher> request_matcher) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int32_t id = next_id_++;

  auto entry = base::MakeRefCounted<InterceptorEntry>(
      id, std::move(request_matcher), std::move(interceptor));
  interceptors_.push_back(entry);

  return id;
}

int32_t AwHeaderInterceptorStore::AddInterceptorFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& interceptor,
    const std::vector<std::string>& url_patterns) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto matcher = AwRequestMatcher::Create(url_patterns);
  if (!matcher.has_value()) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                  matcher.error().c_str());
    return -1;
  }

  return AddInterceptor(
      base::android::ScopedJavaGlobalRef<jobject>(interceptor),
      std::move(matcher.value()));
}

void AwHeaderInterceptorStore::RemoveInterceptor(int32_t id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::erase_if(interceptors_,
                [id](const scoped_refptr<InterceptorEntry>& entry) {
                  return entry->id == id;
                });
}

void AwHeaderInterceptorStore::ClearInterceptors() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  interceptors_.clear();
}

const std::vector<scoped_refptr<AwHeaderInterceptorStore::InterceptorEntry>>&
AwHeaderInterceptorStore::GetInterceptors() const {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return interceptors_;
}

int32_t AwHeaderInterceptorStore::GetInterceptorCountForTesting() const {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_cast<int32_t>(interceptors_.size());
}

}  // namespace android_webview

DEFINE_JNI(AwHeaderInterceptorStore)
