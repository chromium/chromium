// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LARGE_ICON_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_LARGE_ICON_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"

// The C++ counterpart to Java's LargeIconBridge. Together these classes expose
// LargeIconService to Java.
class LargeIconBridge {
 public:
  LargeIconBridge();
  void Destroy(JNIEnv* env);
  jboolean GetLargeIconForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jstring>& j_page_url,
      jint min_source_size_px,
      const base::android::JavaParamRef<jobject>& j_callback);

 private:
  virtual ~LargeIconBridge();

  base::CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconBridge);
};

#endif  // CHROME_BROWSER_ANDROID_LARGE_ICON_BRIDGE_H_
