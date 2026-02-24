// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_CALLBACK_H_
#define BASE_ANDROID_JNI_CALLBACK_H_

#include <jni.h>

#include <type_traits>

#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "third_party/jni_zero/jni_zero.h"

namespace base::android {

using JniOnceWrappedCallbackType =
    base::OnceCallback<void(const jni_zero::JavaRef<jobject>&)>;
using JniRepeatingWrappedCallbackType =
    base::RepeatingCallback<void(const jni_zero::JavaRef<jobject>&)>;
using JniOnceWrappedCallback2Type =
    base::OnceCallback<void(const jni_zero::JavaRef<jobject>&,
                            const jni_zero::JavaRef<jobject>&)>;
using JniRepeatingWrappedCallback2Type =
    base::RepeatingCallback<void(const jni_zero::JavaRef<jobject>&,
                                 const jni_zero::JavaRef<jobject>&)>;

BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniOnceWrappedCallbackType&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniRepeatingWrappedCallbackType&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const JniRepeatingWrappedCallbackType& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniOnceWrappedCallback2Type&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    JniRepeatingWrappedCallback2Type&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const JniRepeatingWrappedCallback2Type& callback);
// Overloads that accept no parameter.
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceCallback<void()>&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingCallback<void()>& callback);

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R, typename Arg1, typename Arg2>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceCallback<R(Arg1, Arg2)>&& callback) {
  return ToJniCallback(
      env, base::BindOnce(
               [](base::OnceCallback<R(Arg1, Arg2)> captured_callback,
                  const jni_zero::JavaRef<jobject>& j_result1,
                  const jni_zero::JavaRef<jobject>& j_result2) {
                 std::move(captured_callback)
                     .Run(jni_zero::FromJniType<std::decay_t<Arg1>>(
                              jni_zero::AttachCurrentThread(), j_result1),
                          jni_zero::FromJniType<std::decay_t<Arg2>>(
                              jni_zero::AttachCurrentThread(), j_result2));
               },
               std::move(callback)));
}

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R, typename Arg1, typename Arg2>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingCallback<R(Arg1, Arg2)>& callback) {
  return ToJniCallback(
      env,
      base::BindRepeating(
          [](const base::RepeatingCallback<R(Arg1, Arg2)>& captured_callback,
             const jni_zero::JavaRef<jobject>& j_result1,
             const jni_zero::JavaRef<jobject>& j_result2) {
            captured_callback.Run(
                jni_zero::FromJniType<std::decay_t<Arg1>>(
                    jni_zero::AttachCurrentThread(), j_result1),
                jni_zero::FromJniType<std::decay_t<Arg2>>(
                    jni_zero::AttachCurrentThread(), j_result2));
          },
          callback));
}

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R, typename Arg>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceCallback<R(Arg)>&& callback) {
  return ToJniCallback(
      env, base::BindOnce(
               [](base::OnceCallback<R(Arg)> captured_callback,
                  const jni_zero::JavaRef<jobject>& j_result) {
                 auto result = jni_zero::FromJniType<std::decay_t<Arg>>(
                     jni_zero::AttachCurrentThread(), j_result);
                 std::move(captured_callback).Run(std::move(result));
               },
               std::move(callback)));
}

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceCallback<R()>&& callback) {
  return ToJniCallback(env, base::BindOnce(
                                [](base::OnceCallback<R()> captured_callback,
                                   const jni_zero::JavaRef<jobject>& j_result) {
                                  std::move(captured_callback).Run();
                                },
                                std::move(callback)));
}

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R, typename Arg>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingCallback<R(Arg)>& callback) {
  return ToJniCallback(
      env, base::BindRepeating(
               [](const base::RepeatingCallback<R(Arg)>& captured_callback,
                  const jni_zero::JavaRef<jobject>& j_result) {
                 Arg result = jni_zero::FromJniType<Arg>(
                     jni_zero::AttachCurrentThread(), j_result);
                 captured_callback.Run(std::move(result));
               },
               callback));
}

// Java Callbacks don't return a value so any return value by the passed in
// callback will be ignored.
template <typename R>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingCallback<R()>& callback) {
  return ToJniCallback(
      env, base::BindRepeating(
               [](const base::RepeatingCallback<R()>& captured_callback,
                  const jni_zero::JavaRef<jobject>& j_result) {
                 captured_callback.Run();
               },
               callback));
}
}  // namespace base::android

namespace jni_zero {

template <typename T>
concept IsJniCallback =
    base::android::internal::IsOnceCallback<std::remove_cvref_t<T>>::value ||
    base::android::internal::IsRepeatingCallback<std::remove_cvref_t<T>>::value;

template <IsJniCallback T>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env, T&& callback) {
  if constexpr (base::android::internal::IsOnceCallback<
                    std::remove_cvref_t<T>>::value) {
    return base::android::ToJniCallback(env, std::move(callback));
  } else {
    return base::android::ToJniCallback(env, std::forward<T>(callback));
  }
}

}  // namespace jni_zero

#endif  // BASE_ANDROID_JNI_CALLBACK_H_
