// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/unguessable_token_android.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(UnguessableTokenAndroid, BasicCreateToken) {
  JNIEnv* env = AttachCurrentThread();
  uint64_t high = 0x1234567812345678;
  uint64_t low = 0x0583503029282304;
  base::UnguessableToken token =
      base::UnguessableToken::CreateForTesting(high, low);
  ScopedJavaLocalRef<jobject> jtoken =
      UnguessableTokenAndroid::Create(env, token);
  std::optional<base::UnguessableToken> result =
      UnguessableTokenAndroid::FromJavaUnguessableToken(env, jtoken);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(token, result.value());
}

TEST(UnguessableTokenAndroid, ParcelAndUnparcel) {
  JNIEnv* env = AttachCurrentThread();
  uint64_t high = 0x1234567812345678;
  uint64_t low = 0x0583503029282304;
  base::UnguessableToken token =
      base::UnguessableToken::CreateForTesting(high, low);
  ScopedJavaLocalRef<jobject> jtoken =
      UnguessableTokenAndroid::Create(env, token);
  ScopedJavaLocalRef<jobject> jtoken_clone =
      UnguessableTokenAndroid::ParcelAndUnparcelForTesting(env, jtoken);
  std::optional<base::UnguessableToken> token_clone =
      UnguessableTokenAndroid::FromJavaUnguessableToken(env, jtoken_clone);

  ASSERT_TRUE(token_clone.has_value());
  EXPECT_EQ(token, token_clone.value());
}

}  // namespace android
}  // namespace base
