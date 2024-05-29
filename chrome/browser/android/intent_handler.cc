// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "services/network/public/cpp/cors/cors.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/IntentHandler_jni.h"

using base::android::JavaParamRef;

namespace chrome {
namespace android {

jboolean JNI_IntentHandler_IsCorsSafelistedHeader(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_header_name,
    const JavaParamRef<jstring>& j_header_value) {
  std::string header_name(
      base::android::ConvertJavaStringToUTF8(env, j_header_name));
  std::string header_value(
      base::android::ConvertJavaStringToUTF8(env, j_header_value));

  return network::cors::IsCorsSafelistedHeader(header_name, header_value);
}

}  // namespace android
}  // namespace chrome
