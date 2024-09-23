// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/token_android.h"

#include "build/robolectric_buildflags.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/TokenBase_jni.h"  // nogncheck
#include "base/base_robolectric_jni/Token_jni.h"      // nogncheck
#else
#include "base/base_jni/TokenBase_jni.h"
#include "base/base_jni/Token_jni.h"
#endif

namespace base::android {

ScopedJavaLocalRef<jobject> TokenAndroid::Create(JNIEnv* env,
                                                 const base::Token& token) {
  return Java_Token_Constructor(env, static_cast<jlong>(token.high()),
                                static_cast<jlong>(token.low()));
}

base::Token TokenAndroid::FromJavaToken(JNIEnv* env,
                                        const JavaRef<jobject>& j_token) {
  const uint64_t high = static_cast<uint64_t>(
      Java_TokenBase_getHighForSerialization(env, j_token));
  const uint64_t low = static_cast<uint64_t>(
      Java_TokenBase_getLowForSerialization(env, j_token));
  return base::Token(high, low);
}

static base::Token JNI_Token_CreateRandom(JNIEnv* env) {
  return base::Token::CreateRandom();
}

}  // namespace base::android
