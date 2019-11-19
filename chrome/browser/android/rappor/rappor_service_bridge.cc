// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/RapporServiceBridge_jni.h"
#include "chrome/browser/browser_process.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace rappor {

void JNI_RapporServiceBridge_SampleDomainAndRegistryFromURL(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_metric,
    const JavaParamRef<jstring>& j_url) {
  // TODO(knn): UMA metrics hash the string to prevent frequent re-encoding,
  // perhaps we should do that as well.
  std::string metric(base::android::ConvertJavaStringToUTF8(env, j_metric));
  GURL gurl(base::android::ConvertJavaStringToUTF8(env, j_url));
  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          metric, gurl);
}

void JNI_RapporServiceBridge_SampleString(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_metric,
    const JavaParamRef<jstring>& j_value) {
  std::string metric(base::android::ConvertJavaStringToUTF8(env, j_metric));
  std::string value(base::android::ConvertJavaStringToUTF8(env, j_value));
  rappor::SampleString(g_browser_process->rappor_service(),
                       metric, rappor::UMA_RAPPOR_TYPE, value);
}

}  // namespace rappor
