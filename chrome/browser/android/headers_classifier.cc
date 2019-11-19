// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/IntentHeadersRecorder_jni.h"
#include "services/network/public/cpp/cors/cors.h"

using base::android::JavaParamRef;

namespace chrome {
namespace android {

jboolean JNI_IntentHeadersRecorder_IsCorsSafelistedHeader(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_header_name,
    const JavaParamRef<jstring>& j_header_value) {
  std::string header_name(ConvertJavaStringToUTF8(env, j_header_name));
  std::string header_value(ConvertJavaStringToUTF8(env, j_header_value));

  return network::cors::IsCorsSafelistedHeader(header_name, header_value);
}

}  // namespace android
}  // namespace chrome
