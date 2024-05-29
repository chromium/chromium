// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_handler_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/color_utils_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkHandlerDelegate_jni.h"

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
    const std::string& jname,
    const std::string& jshort_name,
    const std::string& jpackage_name,
    const std::string& jid,
    const jint jshell_apk_version,
    const jint jversion_code,
    const std::string& juri,
    const std::string& jscope,
    const std::string& jmanifest_url,
    const std::string& jmanifest_start_url,
    const base::android::JavaParamRef<jstring>& jmanifest_id,
    const jint jdisplay_mode,
    const jint jorientation,
    const jlong jtheme_color,
    const jlong jbackground_color,
    const jlong jdark_theme_color,
    const jlong jdark_background_color,
    const jlong jlast_update_check_time_ms,
    const jlong jlast_update_completion_time_ms,
    const jboolean jrelax_updates,
    const base::android::JavaParamRef<jstring>& jbacking_browser_package_name,
    const jboolean jis_backing_browser,
    const std::string& jupdate_status) {
  std::string backing_browser_package_name;
  if (jbacking_browser_package_name) {
    backing_browser_package_name = base::android::ConvertJavaStringToUTF8(
        env, jbacking_browser_package_name);
  }

  std::string manifest_id;
  if (jmanifest_id) {
    manifest_id = base::android::ConvertJavaStringToUTF8(env, jmanifest_id);
  }

  callback_.Run(WebApkInfo(
      jname, jshort_name, jpackage_name, jid,
      static_cast<int>(jshell_apk_version), static_cast<int>(jversion_code),
      juri, jscope, jmanifest_url, jmanifest_start_url, manifest_id,
      static_cast<blink::mojom::DisplayMode>(jdisplay_mode),
      static_cast<device::mojom::ScreenOrientationLockType>(jorientation),
      ui::JavaColorToOptionalSkColor(jtheme_color),
      ui::JavaColorToOptionalSkColor(jbackground_color),
      ui::JavaColorToOptionalSkColor(jdark_theme_color),
      ui::JavaColorToOptionalSkColor(jdark_background_color),
      base::Time::FromMillisecondsSinceUnixEpoch(jlast_update_check_time_ms),
      base::Time::FromMillisecondsSinceUnixEpoch(
          jlast_update_completion_time_ms),
      static_cast<bool>(jrelax_updates), backing_browser_package_name,
      static_cast<bool>(jis_backing_browser), jupdate_status));
}
