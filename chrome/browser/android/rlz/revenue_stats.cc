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
  SearchTermsDataAndroid::search_client_.Get() = client;
}

static void JNI_RevenueStats_SetCustomTabSearchClient(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jstring>& j_client) {
  if (j_client.is_null()) {
    SearchTermsDataAndroid::custom_tab_search_client_.Get().reset();
  } else {
    SearchTermsDataAndroid::custom_tab_search_client_.Get().emplace(
        base::android::ConvertJavaStringToUTF8(j_client));
  }
}

static void JNI_RevenueStats_SetRlzParameterValue(JNIEnv* env,
                                                  std::u16string& rlz) {
  SearchTermsDataAndroid::rlz_parameter_value_.Get() = rlz;
}

}  // namespace android
}  // namespace chrome
