// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/unguessable_token_android.h"

#include "base/base_jni/UnguessableToken_jni.h"

namespace base {
namespace android {

ScopedJavaLocalRef<jobject> UnguessableTokenAndroid::Create(
    JNIEnv* env,
    const base::UnguessableToken& token) {
  const uint64_t high = token.GetHighForSerialization();
  const uint64_t low = token.GetLowForSerialization();
  DCHECK(high);
  DCHECK(low);
  return Java_UnguessableToken_create(env, static_cast<jlong>(high),
                                      static_cast<jlong>(low));
}

absl::optional<base::UnguessableToken>
UnguessableTokenAndroid::FromJavaUnguessableToken(
    JNIEnv* env,
    const JavaRef<jobject>& token) {
  const uint64_t high = static_cast<uint64_t>(
      Java_UnguessableToken_getHighForSerialization(env, token));
  const uint64_t low = static_cast<uint64_t>(
      Java_UnguessableToken_getLowForSerialization(env, token));
  DCHECK(high);
  DCHECK(low);
  return base::UnguessableToken::Deserialize(high, low);
}

ScopedJavaLocalRef<jobject>
UnguessableTokenAndroid::ParcelAndUnparcelForTesting(
    JNIEnv* env,
    const JavaRef<jobject>& token) {
  return Java_UnguessableToken_parcelAndUnparcelForTesting(env, token);
}

}  // namespace android
}  // namespace base
