// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_handler_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/WebApkHandlerDelegate_jni.h"
#include "chrome/browser/android/color_helpers.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkColor.h"

using base::android::JavaParamRef;

WebApkHandlerDelegate::WebApkHandlerDelegate(const WebApkInfoCallback& callback)
    : callback_(callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_obj_.Reset(env, Java_WebApkHandlerDelegate_create(
                        env, reinterpret_cast<intptr_t>(this))
                        .obj());
}

WebApkHandlerDelegate::~WebApkHandlerDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkHandlerDelegate_reset(env, j_obj_);
}

void WebApkHandlerDelegate::RetrieveWebApks() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkHandlerDelegate_retrieveWebApks(env, j_obj_);
}

void WebApkHandlerDelegate::OnWebApkInfoRetrieved(
    JNIEnv* env,
    const JavaParamRef<jstring>& jname,
    const JavaParamRef<jstring>& jshort_name,
    const JavaParamRef<jstring>& jpackage_name,
    const JavaParamRef<jstring>& jid,
    const jint jshell_apk_version,
    const jint jversion_code,
    const JavaParamRef<jstring>& juri,
    const JavaParamRef<jstring>& jscope,
    const JavaParamRef<jstring>& jmanifest_url,
    const JavaParamRef<jstring>& jmanifest_start_url,
    const jint jdisplay_mode,
    const jint jorientation,
    const jlong jtheme_color,
    const jlong jbackground_color,
    const jlong jlast_update_check_time_ms,
    const jlong jlast_update_completion_time_ms,
    const jboolean jrelax_updates,
    const JavaParamRef<jstring>& jbacking_browser_package_name,
    const jboolean jis_backing_browser,
    const JavaParamRef<jstring>& jupdate_status) {
  std::string backing_browser_package_name;
  if (jbacking_browser_package_name) {
    backing_browser_package_name = base::android::ConvertJavaStringToUTF8(
        env, jbacking_browser_package_name);
  }

  callback_.Run(WebApkInfo(
      base::android::ConvertJavaStringToUTF8(env, jname),
      base::android::ConvertJavaStringToUTF8(env, jshort_name),
      base::android::ConvertJavaStringToUTF8(env, jpackage_name),
      base::android::ConvertJavaStringToUTF8(env, jid),
      static_cast<int>(jshell_apk_version), static_cast<int>(jversion_code),
      base::android::ConvertJavaStringToUTF8(env, juri),
      base::android::ConvertJavaStringToUTF8(env, jscope),
      base::android::ConvertJavaStringToUTF8(env, jmanifest_url),
      base::android::ConvertJavaStringToUTF8(env, jmanifest_start_url),
      static_cast<blink::mojom::DisplayMode>(jdisplay_mode),
      static_cast<blink::WebScreenOrientationLockType>(jorientation),
      JavaColorToOptionalSkColor(jtheme_color),
      JavaColorToOptionalSkColor(jbackground_color),
      base::Time::FromJavaTime(jlast_update_check_time_ms),
      base::Time::FromJavaTime(jlast_update_completion_time_ms),
      static_cast<bool>(jrelax_updates), backing_browser_package_name,
      static_cast<bool>(jis_backing_browser),
      base::android::ConvertJavaStringToUTF8(env, jupdate_status)));
}
