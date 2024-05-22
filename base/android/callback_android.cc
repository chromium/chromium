// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "build/robolectric_buildflags.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/Callback_jni.h"  // nogncheck
#else
#include "base/callback_jni/Callback_jni.h"
#endif

namespace base {
namespace android {

void RunObjectCallbackAndroid(const JavaRef<jobject>& callback,
                              const JavaRef<jobject>& arg) {
  Java_Helper_onObjectResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunBooleanCallbackAndroid(const JavaRef<jobject>& callback, bool arg) {
  Java_Helper_onBooleanResultFromNative(AttachCurrentThread(), callback,
                                        static_cast<jboolean>(arg));
}

void RunIntCallbackAndroid(const JavaRef<jobject>& callback, int32_t arg) {
  Java_Helper_onIntResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunLongCallbackAndroid(const JavaRef<jobject>& callback, int64_t arg) {
  Java_Helper_onLongResultFromNative(AttachCurrentThread(), callback, arg);
}

void RunTimeCallbackAndroid(const JavaRef<jobject>& callback, base::Time time) {
  Java_Helper_onTimeResultFromNative(AttachCurrentThread(), callback,
                                     time.InMillisecondsSinceUnixEpoch());
}

void RunStringCallbackAndroid(const JavaRef<jobject>& callback,
                              const std::string& arg) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_string = ConvertUTF8ToJavaString(env, arg);
  Java_Helper_onObjectResultFromNative(env, callback, java_string);
}

void RunOptionalStringCallbackAndroid(
    const JavaRef<jobject>& callback,
    base::optional_ref<const std::string> optional_string_arg) {
  JNIEnv* env = AttachCurrentThread();
  if (optional_string_arg.has_value()) {
    Java_Helper_onOptionalStringResultFromNative(
        env, callback, true,
        ConvertUTF8ToJavaString(env, optional_string_arg.value()));
  } else {
    Java_Helper_onOptionalStringResultFromNative(
        env, callback, false, ConvertUTF8ToJavaString(env, std::string()));
  }
}

void RunByteArrayCallbackAndroid(const JavaRef<jobject>& callback,
                                 const std::vector<uint8_t>& arg) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes = ToJavaByteArray(env, arg);
  Java_Helper_onObjectResultFromNative(env, callback, j_bytes);
}

void RunRunnableAndroid(const JavaRef<jobject>& runnable) {
  Java_Helper_runRunnable(AttachCurrentThread(), runnable);
}

}  // namespace android
}  // namespace base
