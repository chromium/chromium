// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SafeBrowsingReferringAppBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using ReferringAppSource = safe_browsing::ReferringAppInfo::ReferringAppSource;

namespace {
ReferringAppSource IntToReferringAppSource(int source) {
  return static_cast<ReferringAppSource>(source);
}
}  // namespace

namespace safe_browsing {

internal::ReferringAppInfo GetReferringAppInfo(
    content::WebContents* web_contents,
    bool get_webapk_info) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();

  internal::ReferringAppInfo info;

  if (!window_android) {
    return info;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_info =
      Java_SafeBrowsingReferringAppBridge_getReferringAppInfo(
          env, window_android->GetJavaObject(),
          static_cast<jboolean>(get_webapk_info));
  info.referring_app_source =
      IntToReferringAppSource(Java_ReferringAppInfo_getSource(env, j_info));
  info.referring_app_name = Java_ReferringAppInfo_getName(env, j_info);
  info.target_url = GURL(Java_ReferringAppInfo_getTargetUrl(env, j_info));
  if (get_webapk_info) {
    info.referring_webapk_start_url =
        GURL(Java_ReferringAppInfo_getReferringWebApkStartUrl(env, j_info));
    info.referring_webapk_manifest_id =
        GURL(Java_ReferringAppInfo_getReferringWebApkManifestId(env, j_info));
  }

  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("SafeBrowsing.GetReferringAppInfo.Duration",
                          duration);
  if (get_webapk_info) {
    base::UmaHistogramTimes(
        "SafeBrowsing.GetReferringAppInfo.WithWebApk.Duration", duration);
    base::UmaHistogramBoolean(
        "SafeBrowsing.GetReferringAppInfo.WithWebApk.FoundWebApk",
        info.referring_webapk_start_url.is_valid());
  }
  return info;
}

}  // namespace safe_browsing

DEFINE_JNI(SafeBrowsingReferringAppBridge)
