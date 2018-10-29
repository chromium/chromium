// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_variations_seed_bridge.h"

#include <jni.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/variations/seed_response.h"
#include "jni/AwVariationsSeedBridge_jni.h"

namespace android_webview {

std::unique_ptr<variations::SeedResponse> GetAndClearJavaSeed() {
  using namespace base::android;

  JNIEnv* env = AttachCurrentThread();
  if (!Java_AwVariationsSeedBridge_haveSeed(env))
    return nullptr;

  ScopedJavaLocalRef<jstring> j_signature =
      Java_AwVariationsSeedBridge_getSignature(env);
  ScopedJavaLocalRef<jstring> j_country =
      Java_AwVariationsSeedBridge_getCountry(env);
  ScopedJavaLocalRef<jstring> j_date = Java_AwVariationsSeedBridge_getDate(env);
  jboolean j_is_gzip_compressed =
      Java_AwVariationsSeedBridge_getIsGzipCompressed(env);
  ScopedJavaLocalRef<jbyteArray> j_data =
      Java_AwVariationsSeedBridge_getData(env);
  Java_AwVariationsSeedBridge_clearSeed(env);

  auto java_seed = std::make_unique<variations::SeedResponse>();
  base::android::JavaByteArrayToString(env, j_data, &java_seed->data);
  java_seed->signature = ConvertJavaStringToUTF8(j_signature);
  java_seed->country = ConvertJavaStringToUTF8(j_country);
  java_seed->date = ConvertJavaStringToUTF8(j_date);
  java_seed->is_gzip_compressed = static_cast<bool>(j_is_gzip_compressed);
  return java_seed;
}

}  // namespace android_webview
