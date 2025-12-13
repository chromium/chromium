// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_NAVIGATION_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_NAVIGATION_CLIENT_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/page.h"

namespace android_webview {

// A class that handles the Native->Java communication for the
// AwNavigationClient. AwNavigationClient is created and owned by
// the native AwContents class and it only has a weak reference to the
// its Java peer.
// Lifetime: WebView
class AwNavigationClient {
 public:
  AwNavigationClient(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);
  ~AwNavigationClient() = default;

  void OnFirstContentfulPaint(content::Page& page,
                              const base::TimeDelta& duration);
  void OnLargestContentfulPaint(content::Page& page,
                                const base::TimeDelta& duration);
  void OnPerformanceMark(content::Page&,
                         std::string mark_name,
                         const base::TimeDelta& mark_time);

 private:
  JavaObjectWeakGlobalRef java_ref_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_NAVIGATION_CLIENT_H_
