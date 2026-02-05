// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CALLBACK_ANDROID_H_
#define BASE_ANDROID_CALLBACK_ANDROID_H_

#include <string>
#include <type_traits>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

// Provides helper utility methods that run the given callback with the
// specified argument.
namespace base::android {

void BASE_EXPORT RunObjectCallbackAndroid(const JavaRef<jobject>& callback,
                                          const JavaRef<jobject>& arg);

void BASE_EXPORT RunBooleanCallbackAndroid(const JavaRef<jobject>& callback,
                                           bool arg);

void BASE_EXPORT RunIntCallbackAndroid(const JavaRef<jobject>& callback,
                                       int32_t arg);

void BASE_EXPORT RunLongCallbackAndroid(const JavaRef<jobject>& callback,
                                        int64_t arg);

void BASE_EXPORT RunTimeCallbackAndroid(const JavaRef<jobject>& callback,
                                        base::Time time);

void BASE_EXPORT RunStringCallbackAndroid(const JavaRef<jobject>& callback,
                                          const std::string& arg);

void BASE_EXPORT RunOptionalStringCallbackAndroid(
    const JavaRef<jobject>& callback,
    base::optional_ref<const std::string> optional_string_arg);

void BASE_EXPORT RunByteArrayCallbackAndroid(const JavaRef<jobject>& callback,
                                             const std::vector<uint8_t>& arg);
}  // namespace base::android

namespace base::android::internal {
template <typename T>
struct IsOnceCallback : std::false_type {};

template <typename T>
struct IsOnceCallback<base::OnceCallback<void(T)>> : std::true_type {
  using ArgType = T;
};

template <typename T>
struct IsRepeatingCallback : std::false_type {};

template <typename T>
struct IsRepeatingCallback<base::RepeatingCallback<void(T)>> : std::true_type {
  using ArgType = T;
};

template <typename T>
void RunJavaCallback(const jni_zero::ScopedJavaGlobalRef<jobject>& callback,
                     T arg) {
  if constexpr (requires { jni_zero::ToJniType(nullptr, std::move(arg)); }) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(
        callback, jni_zero::ToJniType(env, std::move(arg)));
  } else {
    static_assert(
        sizeof(T) == 0,
        "Could not find ToJniType<> specialization for the callback's "
        "parameter. Make sure the header declaring it is #included before "
        "base/callback_android.h");
  }
}

}  // namespace base::android::internal

namespace jni_zero {

// @JniType("base::OnceClosure") Runnable
template <>
inline base::OnceClosure FromJniType<base::OnceClosure>(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  return base::BindOnce(&jni_zero::RunRunnable,
                        jni_zero::ScopedJavaGlobalRef<>(env, obj));
}

// @JniType("base::RepeatingClosure") Runnable
template <>
inline base::RepeatingClosure FromJniType<base::RepeatingClosure>(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  return base::BindRepeating(&jni_zero::RunRunnable,
                             jni_zero::ScopedJavaGlobalRef<>(env, obj));
}

// @JniType("base::OnceCallback<void(NativeFoo)>") Callback<JavaFoo>
template <typename T>
  requires(base::android::internal::IsOnceCallback<T>::value)
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  namespace internal = base::android::internal;
  using ArgType = typename internal::IsOnceCallback<T>::ArgType;
  return base::BindOnce(&internal::RunJavaCallback<ArgType>,
                        ScopedJavaGlobalRef<jobject>(env, obj));
}

// @JniType("base::RepeatingCallback<void(NativeFoo)>") Callback<JavaFoo>
template <typename T>
  requires(base::android::internal::IsRepeatingCallback<T>::value)
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  namespace internal = base::android::internal;
  using ArgType = typename internal::IsRepeatingCallback<T>::ArgType;
  return base::BindRepeating(&internal::RunJavaCallback<ArgType>,
                             ScopedJavaGlobalRef<jobject>(env, obj));
}

}  // namespace jni_zero

#endif  // BASE_ANDROID_CALLBACK_ANDROID_H_
