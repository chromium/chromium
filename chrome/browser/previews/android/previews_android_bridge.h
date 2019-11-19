// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_ANDROID_PREVIEWS_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_PREVIEWS_ANDROID_PREVIEWS_ANDROID_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

class PreviewsAndroidBridge {
 public:
  PreviewsAndroidBridge(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  virtual ~PreviewsAndroidBridge();

  jboolean ShouldShowPreviewUI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  base::android::ScopedJavaLocalRef<jstring> GetLitePageRedirectOriginalURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_visisble_url);

  base::android::ScopedJavaLocalRef<jstring> GetStalePreviewTimestamp(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void LoadOriginal(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& j_web_contents);

  base::android::ScopedJavaLocalRef<jstring> GetPreviewsType(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

 private:
  base::WeakPtrFactory<PreviewsAndroidBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsAndroidBridge);
};

#endif  // CHROME_BROWSER_PREVIEWS_ANDROID_PREVIEWS_ANDROID_BRIDGE_H_
