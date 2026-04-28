// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/startup/url_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/IntentHandler_jni.h"

using base::android::JavaRef;

namespace chrome {
namespace android {

static bool JNI_IntentHandler_IsCorsSafelistedHeader(
    JNIEnv* env,
    const std::string& header_name,
    const std::string& header_value) {
  return network::cors::IsCorsSafelistedHeader(header_name, header_value);
}

static bool JNI_IntentHandler_ValidateUrl(JNIEnv* env,
                                          const JavaRef<jobject>& url) {
  return startup::ValidateUrl(url::GURLAndroid::ToNativeGURL(env, url));
}

}  // namespace android
}  // namespace chrome

DEFINE_JNI(IntentHandler)
