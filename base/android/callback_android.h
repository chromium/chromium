// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CALLBACK_ANDROID_H_
#define BASE_ANDROID_CALLBACK_ANDROID_H_

#include <string>
#include <type_traits>
#include <utility>
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

BASE_EXPORT void RunObjectCallbackAndroid(const JavaRef<jobject>& callback,
                                          const JavaRef<jobject>& arg);

BASE_EXPORT void RunObjectCallbackAndroid2(const JavaRef<jobject>& callback,
                                           const JavaRef<jobject>& arg1,
                                           const JavaRef<jobject>& arg2);

BASE_EXPORT void RunBooleanCallbackAndroid(const JavaRef<jobject>& callback,
                                           bool arg);

BASE_EXPORT void RunIntCallbackAndroid(const JavaRef<jobject>& callback,
                                       int32_t arg);

BASE_EXPORT void RunLongCallbackAndroid(const JavaRef<jobject>& callback,
                                        int64_t arg);

BASE_EXPORT void RunTimeCallbackAndroid(const JavaRef<jobject>& callback,
                                        base::Time time);

BASE_EXPORT void RunStringCallbackAndroid(const JavaRef<jobject>& callback,
                                          const std::string& arg);

BASE_EXPORT void RunOptionalStringCallbackAndroid(
    const JavaRef<jobject>& callback,
    base::optional_ref<const std::string> optional_string_arg);

BASE_EXPORT void RunByteArrayCallbackAndroid(const JavaRef<jobject>& callback,
                                             const std::vector<uint8_t>& arg);

BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::OnceClosure&& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    const base::RepeatingClosure& callback);
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniCallback(
    JNIEnv* env,
    base::RepeatingClosure&& callback);
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
                 JNIEnv* env = jni_zero::AttachCurrentThread();
                 std::move(captured_callback)
                     .Run(jni_zero::FromJniType<Arg1>(env, j_result1),
                          jni_zero::FromJniType<Arg2>(env, j_result2));
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
            JNIEnv* env = jni_zero::AttachCurrentThread();
            captured_callback.Run(jni_zero::FromJniType<Arg1>(env, j_result1),
                                  jni_zero::FromJniType<Arg2>(env, j_result2));
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
                 JNIEnv* env = jni_zero::AttachCurrentThread();
                 std::move(captured_callback)
                     .Run(jni_zero::FromJniType<Arg>(env, j_result));
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
      env,
      base::BindRepeating(
          [](const base::RepeatingCallback<R(Arg)>& captured_callback,
             const jni_zero::JavaRef<jobject>& j_result) {
            JNIEnv* env = jni_zero::AttachCurrentThread();
            captured_callback.Run(jni_zero::FromJniType<Arg>(env, j_result));
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

namespace base::android::internal {

template <typename T>
struct IsOnceCallback : std::false_type {};

template <>
struct IsOnceCallback<base::OnceCallback<void()>> : std::true_type {};

template <typename T>
struct IsOnceCallback<base::OnceCallback<void(T)>> : std::true_type {
  using ArgType = T;
};

template <typename T1, typename T2>
struct IsOnceCallback<base::OnceCallback<void(T1, T2)>> : std::true_type {
  using Arg1Type = T1;
  using Arg2Type = T2;
};

template <typename T>
struct IsRepeatingCallback : std::false_type {};

template <>
struct IsRepeatingCallback<base::RepeatingCallback<void()>> : std::true_type {};

template <typename T>
struct IsRepeatingCallback<base::RepeatingCallback<void(T)>> : std::true_type {
  using ArgType = T;
};

template <typename T1, typename T2>
struct IsRepeatingCallback<base::RepeatingCallback<void(T1, T2)>>
    : std::true_type {
  using Arg1Type = T1;
  using Arg2Type = T2;
};

template <typename T>
void RunJavaCallback(const jni_zero::ScopedJavaGlobalRef<jobject>& callback,
                     T arg) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      callback, jni_zero::ToJniType(env, std::move(arg)));
}

template <typename T1, typename T2>
void RunJavaCallback2(const jni_zero::ScopedJavaGlobalRef<jobject>& callback,
                      T1 arg1,
                      T2 arg2) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid2(
      callback, jni_zero::ToJniType(env, std::move(arg1)),
      jni_zero::ToJniType(env, std::move(arg2)));
}

}  // namespace base::android::internal

namespace jni_zero {

// @JniType("base::OnceCallback<void(NativeFoo)>") Callback<JavaFoo>
template <typename T>
  requires(base::android::internal::IsOnceCallback<T>::value)
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  namespace internal = base::android::internal;
  if constexpr (std::same_as<std::remove_cvref_t<T>, base::OnceClosure>) {
    return base::BindOnce(&jni_zero::RunRunnable,
                          jni_zero::ScopedJavaGlobalRef<jobject>(env, obj));
  } else if constexpr (requires {
                         typename internal::IsOnceCallback<T>::ArgType;
                       }) {
    using ArgType = typename internal::IsOnceCallback<T>::ArgType;
    return base::BindOnce(&internal::RunJavaCallback<ArgType>,
                          ScopedJavaGlobalRef<jobject>(env, obj));
  } else {
    using Arg1Type = typename internal::IsOnceCallback<T>::Arg1Type;
    using Arg2Type = typename internal::IsOnceCallback<T>::Arg2Type;
    return base::BindOnce(&internal::RunJavaCallback2<Arg1Type, Arg2Type>,
                          ScopedJavaGlobalRef<jobject>(env, obj));
  }
}

// @JniType("base::RepeatingCallback<void(NativeFoo)>") Callback<JavaFoo>
template <typename T>
  requires(base::android::internal::IsRepeatingCallback<T>::value)
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  namespace internal = base::android::internal;
  if constexpr (std::same_as<std::remove_cvref_t<T>, base::RepeatingClosure>) {
    return base::BindRepeating(
        &jni_zero::RunRunnable,
        jni_zero::ScopedJavaGlobalRef<jobject>(env, obj));
  } else if constexpr (requires {
                         typename internal::IsRepeatingCallback<T>::ArgType;
                       }) {
    using ArgType = typename internal::IsRepeatingCallback<T>::ArgType;
    return base::BindRepeating(&internal::RunJavaCallback<ArgType>,
                               ScopedJavaGlobalRef<jobject>(env, obj));
  } else {
    using Arg1Type = typename internal::IsRepeatingCallback<T>::Arg1Type;
    using Arg2Type = typename internal::IsRepeatingCallback<T>::Arg2Type;
    return base::BindRepeating(&internal::RunJavaCallback2<Arg1Type, Arg2Type>,
                               ScopedJavaGlobalRef<jobject>(env, obj));
  }
}

template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::OnceCallback<R(Args...)>&& callback) {
  return base::android::ToJniCallback(env, std::move(callback));
}

template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    base::RepeatingCallback<R(Args...)>&& callback) {
  return base::android::ToJniCallback(env, std::move(callback));
}

template <typename R, typename... Args>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const base::RepeatingCallback<R(Args...)>& callback) {
  return base::android::ToJniCallback(env, callback);
}

}  // namespace jni_zero

#endif  // BASE_ANDROID_CALLBACK_ANDROID_H_
