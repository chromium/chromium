// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/android/webapk/webapk_info.h"

// Delegate for retrieving installed WebAPKs for display in WebUI.
class WebApkHandlerDelegate {
 public:
  using WebApkInfoCallback = base::RepeatingCallback<void(const WebApkInfo&)>;

  explicit WebApkHandlerDelegate(const WebApkInfoCallback& callback);

  WebApkHandlerDelegate(const WebApkHandlerDelegate&) = delete;
  WebApkHandlerDelegate& operator=(const WebApkHandlerDelegate&) = delete;

  ~WebApkHandlerDelegate();

  // Fetches information about each WebAPK.
  void RetrieveWebApks();

  // Called once for each installed WebAPK when RetrieveWebApks() is called.
  void OnWebApkInfoRetrieved(
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
      const std::string& jupdate_status);

 private:
  WebApkInfoCallback callback_;

  base::android::ScopedJavaGlobalRef<jobject> j_obj_;
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_HANDLER_DELEGATE_H_
