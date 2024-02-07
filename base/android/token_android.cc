// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/token_android.h"

#include "base/base_jni/Token_jni.h"

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

static ScopedJavaLocalRef<jobject> JNI_Token_CreateRandom(JNIEnv* env) {
  return TokenAndroid::Create(env, base::Token::CreateRandom());
}

}  // namespace android
}  // namespace base
