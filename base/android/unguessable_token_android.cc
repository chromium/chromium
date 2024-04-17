// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/unguessable_token_android.h"

#include "build/robolectric_buildflags.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/TokenBase_jni.h"  // nogncheck
#include "base/base_robolectric_jni/UnguessableToken_jni.h"  // nogncheck
#else
#include "base/base_jni/TokenBase_jni.h"
#include "base/base_jni/UnguessableToken_jni.h"
#endif

namespace jni_zero {
template <>
BASE_EXPORT base::UnguessableToken FromJniType<base::UnguessableToken>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
      env, j_object);
}

template <>
BASE_EXPORT std::optional<base::UnguessableToken>
FromJniType<std::optional<base::UnguessableToken>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  if (!j_object) {
    return std::nullopt;
  }
  return base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
      env, j_object);
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<base::UnguessableToken>(
    JNIEnv* env,
    const base::UnguessableToken& token) {
  return base::android::UnguessableTokenAndroid::Create(env, token);
}
template <>
BASE_EXPORT ScopedJavaLocalRef<jobject>
ToJniType<std::optional<base::UnguessableToken>>(
    JNIEnv* env,
    const std::optional<base::UnguessableToken>& token) {
  if (!token) {
    return nullptr;
  }
  return base::android::UnguessableTokenAndroid::Create(env, token.value());
}
}  // namespace jni_zero

namespace base {
namespace android {

jni_zero::ScopedJavaLocalRef<jobject> UnguessableTokenAndroid::Create(
    JNIEnv* env,
    const base::UnguessableToken& token) {
  const uint64_t high = token.GetHighForSerialization();
  const uint64_t low = token.GetLowForSerialization();
  DCHECK(high);
  DCHECK(low);
  return Java_UnguessableToken_Constructor(env, static_cast<jlong>(high),
                                           static_cast<jlong>(low));
}

base::UnguessableToken UnguessableTokenAndroid::FromJavaUnguessableToken(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& token) {
  const uint64_t high =
      static_cast<uint64_t>(Java_TokenBase_getHighForSerialization(env, token));
  const uint64_t low =
      static_cast<uint64_t>(Java_TokenBase_getLowForSerialization(env, token));
  DCHECK(high);
  DCHECK(low);
  return base::UnguessableToken::Deserialize(high, low).value();
}

jni_zero::ScopedJavaLocalRef<jobject>
UnguessableTokenAndroid::ParcelAndUnparcelForTesting(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& token) {
  return Java_UnguessableToken_parcelAndUnparcelForTesting(env, token);
}

}  // namespace android
}  // namespace base
