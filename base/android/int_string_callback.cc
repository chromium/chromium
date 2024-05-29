// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file encapsulates the JNI headers generated for IntStringCallback, so
// that the methods defined in the generated headers only end up in one object
// file. This is similar to //base/android/callback_android.*.

#include "base/android/int_string_callback.h"

#include <jni.h>

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/IntStringCallback_jni.h"

namespace base {
namespace android {

void RunIntStringCallbackAndroid(const JavaRef<jobject>& callback,
                                 int int_arg,
                                 const std::string& str_arg) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_IntStringCallback_onResult(env, callback, int_arg,
                                  ConvertUTF8ToJavaString(env, str_arg));
}

}  // namespace android
}  // namespace base
