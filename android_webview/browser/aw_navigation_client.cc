// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_navigation_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwNavigationClient_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwNavigationClient::AwNavigationClient(JNIEnv* env,
                                       const jni_zero::JavaRef<jobject>& obj)
    : java_ref_(env, obj) {}

void AwNavigationClient::OnFirstContentfulPaint(
    content::Page& page,
    const base::TimeDelta& duration) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    return;
  }

  Java_AwNavigationClient_onFirstContentfulPaint(env, obj, page.GetJavaPage(),
                                                 duration.InMilliseconds());
}

void AwNavigationClient::OnLargestContentfulPaint(
    content::Page& page,
    const base::TimeDelta& duration) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    return;
  }

  Java_AwNavigationClient_onLargestContentfulPaint(env, obj, page.GetJavaPage(),
                                                   duration.InMilliseconds());
}

void AwNavigationClient::OnPerformanceMark(content::Page& page,
                                           std::string mark_name,
                                           const base::TimeDelta& mark_time) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    return;
  }

  Java_AwNavigationClient_onPerformanceMark(
      env, obj, page.GetJavaPage(), ConvertUTF8ToJavaString(env, mark_name),
      mark_time.InMilliseconds());
}
}  // namespace android_webview
