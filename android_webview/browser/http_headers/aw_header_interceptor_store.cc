// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/http_headers/aw_header_interceptor_store.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"

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

}  // namespace android_webview
