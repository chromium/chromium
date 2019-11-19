// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_debugging_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/FeedDebuggingBridge_jni.h"
#include "url/gurl.h"

namespace feed {

GURL GetFeedFetchUrlForDebugging() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_string =
      Java_FeedDebuggingBridge_getFeedFetchUrl(env);
  return GURL(ConvertJavaStringToUTF8(env, j_string));
}

std::string GetFeedProcessScopeDumpForDebugging() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_string =
      Java_FeedDebuggingBridge_getProcessScopeDump(env);
  return ConvertJavaStringToUTF8(env, j_string);
}

void TriggerRefreshForDebugging() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedDebuggingBridge_triggerRefresh(env);
}

}  // namespace feed
