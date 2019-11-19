// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_resource.h"

#include "android_webview/common_jni_headers/AwResource_jni.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {
namespace AwResource {

std::vector<std::string> GetConfigKeySystemUuidMapping() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> key_system_uuid_mappings;
  ScopedJavaLocalRef<jobjectArray> mappings =
      Java_AwResource_getConfigKeySystemUuidMapping(env);
  base::android::AppendJavaStringArrayToStringVector(env, mappings,
                                                     &key_system_uuid_mappings);
  return key_system_uuid_mappings;
}

}  // namespace AwResource
}  // namespace android_webview
