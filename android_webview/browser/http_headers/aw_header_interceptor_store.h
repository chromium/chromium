// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_HTTP_HEADERS_AW_HEADER_INTERCEPTOR_STORE_H_
#define ANDROID_WEBVIEW_BROWSER_HTTP_HEADERS_AW_HEADER_INTERCEPTOR_STORE_H_

#include <memory>
#include <vector>

#include "android_webview/browser/request_matcher/aw_request_matcher.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

// Manages AwHeaderInterceptor registrations, each paired with an
// AwRequestMatcher that determines which requests the interceptor applies to.
// This class is only safe to be used on the UI thread, though it is safe to
// make a copy of the interceptors (by calling GetInterceptors()) and pass that
// copy to a different thread.
class AwHeaderInterceptorStore {
 public:
  // Interceptor entry in the store, which associates an interceptor
  // with a request matcher and a monotonic id.
  //
  // This class is reference-counted for multiple reasons:
  // - It allows us to specify that the ScopedJavaGlobalRef should be destroyed
  //   on the UI thread (through content::BrowserThread::DeleteOnUIThread),
  //   instead of the IO thread. This can potentially avoid attaching the JNI
  //   environment to the IO thread.
  // - It keeps the matcher and the interceptor alive if it's still being used
  //   by a background thread. This is important because we cache a copy of the
  //   interceptor list in the IO thread (and other background threads), that
  //   can outlive the AwHeaderInterceptorStore.
  // - It avoids having to copy each entry's content when we copy the list of
  //   interceptors across multiple threads.
  //
  // This class is RefCountedThreadSafe so it can be safely shared across
  // threads.
  //
  // This class is thread-safe. Its fields can be read and used from any
  // thread.
  class InterceptorEntry : public base::RefCountedThreadSafe<
                               InterceptorEntry,
                               content::BrowserThread::DeleteOnUIThread> {
   public:
    InterceptorEntry(int32_t id,
                     std::unique_ptr<AwRequestMatcher> request_matcher,
                     base::android::ScopedJavaGlobalRef<jobject> interceptor);

    // An id used to identify the interceptor. It is unique within this store.
    const int32_t id;
    // The request matcher that determines which requests the interceptor
    // applies to.
    const std::unique_ptr<const AwRequestMatcher> request_matcher;
    // A Java reference to the interceptor (an instance of AwHeaderInterceptor).
    const base::android::ScopedJavaGlobalRef<jobject> interceptor;

   private:
    friend struct content::BrowserThread::DeleteOnThread<
        content::BrowserThread::UI>;
    friend class base::DeleteHelper<InterceptorEntry>;
    ~InterceptorEntry();
  };

  AwHeaderInterceptorStore();
  AwHeaderInterceptorStore(const AwHeaderInterceptorStore&) = delete;
  AwHeaderInterceptorStore& operator=(const AwHeaderInterceptorStore&) = delete;
  AwHeaderInterceptorStore(AwHeaderInterceptorStore&&);
  AwHeaderInterceptorStore& operator=(AwHeaderInterceptorStore&&);
  ~AwHeaderInterceptorStore();

  // Adds an interceptor with its associated request matcher. Returns a
  // monotonically increasing ID for the registered interceptor.
  // This method can only be called from the UI thread.
  int AddInterceptor(base::android::ScopedJavaGlobalRef<jobject> interceptor,
                     std::unique_ptr<AwRequestMatcher> request_matcher);

  // Removes the interceptor and request matcher associated with the given ID.
  // This method can only be called from the UI thread.
  void RemoveInterceptor(int32_t id);

  // Removes all registered interceptors.
  // This method can only be called from the UI thread.
  void ClearInterceptors();

  // Returns the interceptors currently registered.
  // This method can only be called from the UI thread, though the result can
  // be copied and passed to other threads (see the documentation of
  // InterceptorEntry).
  const std::vector<scoped_refptr<InterceptorEntry>>& GetInterceptors() const;

 private:
  int32_t next_id_ = 0;
  std::vector<scoped_refptr<InterceptorEntry>> interceptors_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_HTTP_HEADERS_AW_HEADER_INTERCEPTOR_STORE_H_
