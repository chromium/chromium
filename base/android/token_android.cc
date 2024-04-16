// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/token_android.h"

#include "base/numerics/safe_conversions.h"
#include "build/robolectric_buildflags.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/Token_jni.h"  // nogncheck
#else
#include "base/base_jni/Token_jni.h"
#endif

namespace base {
namespace android {

ScopedJavaLocalRef<jobject> TokenAndroid::Create(JNIEnv* env,
                                                 const base::Token& token) {
  return Java_Token_Constructor(env, static_cast<jlong>(token.high()),
                                static_cast<jlong>(token.low()));
}

base::Token TokenAndroid::FromJavaToken(JNIEnv* env,
                                        const JavaRef<jobject>& j_token) {
  const uint64_t high = static_cast<uint64_t>(Java_Token_getHigh(env, j_token));
  const uint64_t low = static_cast<uint64_t>(Java_Token_getLow(env, j_token));
  return base::Token(high, low);
}

ScopedJavaLocalRef<jobjectArray> TokenAndroid::ToJavaArrayOfTokens(
    JNIEnv* env,
    base::span<std::optional<base::Token>> tokens) {
  ScopedJavaLocalRef<jclass> token_clazz =
      jni_zero::GetClass(env, "org/chromium/base/Token");
  jobjectArray joa = env->NewObjectArray(checked_cast<jsize>(tokens.size()),
                                         token_clazz.obj(), nullptr);
  jni_zero::CheckException(env);

  for (size_t i = 0; i < tokens.size(); i++) {
    ScopedJavaLocalRef<jobject> token;
    if (tokens[i]) {
      token = TokenAndroid::Create(env, *tokens[i]);
    }
    env->SetObjectArrayElement(joa, static_cast<jsize>(i), token.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

static ScopedJavaLocalRef<jobject> JNI_Token_CreateRandom(JNIEnv* env) {
  return TokenAndroid::Create(env, base::Token::CreateRandom());
}

}  // namespace android
}  // namespace base
