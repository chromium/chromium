// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/token_android.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(TokenAndroid, CreateToken) {
  JNIEnv* env = AttachCurrentThread();
  uint64_t high = 0xDEADBEEF12345678;
  uint64_t low = 0xABCDEF0123456789;

  base::Token token(high, low);
  ScopedJavaLocalRef<jobject> j_token = TokenAndroid::Create(env, token);
  base::Token result = TokenAndroid::FromJavaToken(env, j_token);

  EXPECT_EQ(token, result);
}

}  // namespace android
}  // namespace base
