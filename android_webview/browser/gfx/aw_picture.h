// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_PICTURE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_PICTURE_H_

#include "base/android/jni_weak_ref.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkPicture;

namespace android_webview {

// This class can outlive the WebView it was created from. It is self-contained
// and independent of the WebView once constructed.
class AwPicture {
 public:
  AwPicture(sk_sp<SkPicture> picture);

  AwPicture() = delete;
  AwPicture(const AwPicture&) = delete;
  AwPicture& operator=(const AwPicture&) = delete;

  ~AwPicture();

  // Methods called from Java.
  void Destroy(JNIEnv* env);
  jint GetWidth(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint GetHeight(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Draw(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            const base::android::JavaParamRef<jobject>& canvas);

 private:
  sk_sp<SkPicture> picture_;
};

}  // android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_PICTURE_H_
