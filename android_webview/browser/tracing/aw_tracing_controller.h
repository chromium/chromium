// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_CONTROLLER_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_CONTROLLER_H_

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace android_webview {

class AwTracingController {
 public:
  AwTracingController(JNIEnv* env, jobject obj);

  bool Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             const base::android::JavaParamRef<jstring>& categories,
             jint mode);
  bool StopAndFlush(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  bool IsTracing(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  ~AwTracingController();

  void OnTraceDataReceived(std::unique_ptr<std::string> chunk);
  void OnTraceDataComplete();

  JavaObjectWeakGlobalRef weak_java_object_;
  base::WeakPtrFactory<AwTracingController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AwTracingController);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_CONTROLLER_H_
