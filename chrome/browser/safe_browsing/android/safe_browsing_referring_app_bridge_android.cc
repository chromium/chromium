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
using ReferringAppSource = safe_browsing::LoginReputationClientRequest::
    ReferringAppInfo::ReferringAppSource;

namespace {
ReferringAppSource IntToReferringAppSource(int source) {
  return static_cast<ReferringAppSource>(source);
}
}  // namespace

namespace safe_browsing {

ReferringAppInfo GetReferringAppInfo(content::WebContents* web_contents) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();

  if (!window_android) {
    return ReferringAppInfo{LoginReputationClientRequest::ReferringAppInfo::
                                REFERRING_APP_SOURCE_UNSPECIFIED,
                            "", GURL()};
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_info =
      Java_SafeBrowsingReferringAppBridge_getReferringAppInfo(
          env, window_android->GetJavaObject());
  ReferringAppSource source =
      IntToReferringAppSource(Java_ReferringAppInfo_getSource(env, j_info));
  std::string name =
      ConvertJavaStringToUTF8(Java_ReferringAppInfo_getName(env, j_info));
  GURL url = GURL(
      ConvertJavaStringToUTF8(Java_ReferringAppInfo_getTargetUrl(env, j_info)));
  base::UmaHistogramTimes("SafeBrowsing.GetReferringAppInfo.Duration",
                          base::TimeTicks::Now() - start_time);
  return ReferringAppInfo{source, name, url};
}

}  // namespace safe_browsing
