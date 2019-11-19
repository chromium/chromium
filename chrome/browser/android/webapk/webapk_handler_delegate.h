// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/android/webapk/webapk_info.h"

// Delegate for retrieving installed WebAPKs for display in WebUI.
class WebApkHandlerDelegate {
 public:
  using WebApkInfoCallback = base::RepeatingCallback<void(const WebApkInfo&)>;

  explicit WebApkHandlerDelegate(const WebApkInfoCallback& callback);
  ~WebApkHandlerDelegate();

  // Fetches information about each WebAPK.
  void RetrieveWebApks();

  // Called once for each installed WebAPK when RetrieveWebApks() is called.
  void OnWebApkInfoRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jname,
      const base::android::JavaParamRef<jstring>& jshort_name,
      const base::android::JavaParamRef<jstring>& jpackage_name,
      const base::android::JavaParamRef<jstring>& jid,
      const jint jshell_apk_version,
      const jint jversion_code,
      const base::android::JavaParamRef<jstring>& juri,
      const base::android::JavaParamRef<jstring>& jscope,
      const base::android::JavaParamRef<jstring>& jmanifest_url,
      const base::android::JavaParamRef<jstring>& jmanifest_start_url,
      const jint jdisplay_mode,
      const jint jorientation,
      const jlong jtheme_color,
      const jlong jbackground_color,
      const jlong jlast_update_check_time_ms,
      const jlong jlast_update_completion_time_ms,
      const jboolean jrelax_updates,
      const base::android::JavaParamRef<jstring>& jbacking_browser_package_name,
      const jboolean jis_backing_browser,
      const base::android::JavaParamRef<jstring>& jupdate_status);

 private:
  WebApkInfoCallback callback_;

  base::android::ScopedJavaGlobalRef<jobject> j_obj_;

  DISALLOW_COPY_AND_ASSIGN(WebApkHandlerDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_
