// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/RevenueStats_jni.h"

using base::android::JavaParamRef;

namespace chrome {
namespace android {

static void JNI_RevenueStats_SetSearchClient(JNIEnv* env, std::string& client) {
  SearchTermsDataAndroid::GetSearchClient() = client;
}

static void JNI_RevenueStats_SetCustomTabSearchClient(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& j_client) {
  if (j_client.is_null()) {
    SearchTermsDataAndroid::GetCustomTabSearchClient().reset();
  } else {
    SearchTermsDataAndroid::GetCustomTabSearchClient().emplace(
        base::android::ConvertJavaStringToUTF8(j_client));
  }
}

static void JNI_RevenueStats_SetRlzParameterValue(JNIEnv* env,
                                                  std::u16string& rlz) {
  SearchTermsDataAndroid::GetRlzParameterValue() = rlz;
}

}  // namespace android
}  // namespace chrome

DEFINE_JNI(RevenueStats)
