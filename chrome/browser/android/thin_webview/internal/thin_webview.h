// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/thin_webview/internal/compositor_view_impl.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace thin_webview {
namespace android {

// Native counterpart of ThinWebViewImpl.java.
class ThinWebView {
 public:
  ThinWebView(JNIEnv* env,
              jobject obj,
              CompositorView* compositor_view,
              ui::WindowAndroid* window_android);
  ~ThinWebView();

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);

  void SetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  void SizeChanged(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& object,
                   jint width,
                   jint height);

 private:
  void SetWebContents(content::WebContents* web_contents);
  void ResizeWebContents(const gfx::Size& size);

  base::android::ScopedJavaGlobalRef<jobject> obj_;
  CompositorView* compositor_view_;
  ui::WindowAndroid* window_android_;
  content::WebContents* web_contents_;
  gfx::Size view_size_;

  DISALLOW_COPY_AND_ASSIGN(ThinWebView);
};

}  // namespace android
}  // namespace thin_webview

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_INTERNAL_THIN_WEBVIEW_H_
