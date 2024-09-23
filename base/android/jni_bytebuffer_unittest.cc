// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_bytebuffer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android {

TEST(JniByteBuffer, ConversionDoesNotCopy) {
  uint8_t bytes[] = {0, 1, 2, 3};
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> jbuffer(
      env, env->NewDirectByteBuffer(bytes, sizeof(bytes)));
  ASSERT_TRUE(jbuffer);

  base::span<const uint8_t> span = JavaByteBufferToSpan(env, jbuffer.obj());
  EXPECT_EQ(span.data(), bytes);
  EXPECT_EQ(span.size(), sizeof(bytes));
}

// Disabled pending diagnosis: https://crbug.com/1521406
// Specifically, under test, env->GetDirectBufferAddress() is returning non-null
// and env->GetDirectBufferCapacity() is returning >= 0, both of which they are
// not supposed to do in this situation.
TEST(JniByteBuffer, DISABLED_ConversionFromNonBuffer) {
  JNIEnv* env = AttachCurrentThread();
  jclass cls = env->FindClass("java/util/ArrayList");
  ASSERT_TRUE(cls);

  jmethodID init =
      base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
          env, cls, "<init>", "()V");

  ScopedJavaLocalRef<jobject> jnonbuffer(env, env->NewObject(cls, init));

  std::optional<base::span<const uint8_t>> maybe_span =
      MaybeJavaByteBufferToSpan(env, jnonbuffer.obj());
  EXPECT_FALSE(maybe_span.has_value());
}

TEST(JniByteBuffer, ZeroByteConversionSucceeds) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbuffer(env,
                                      env->NewDirectByteBuffer(nullptr, 0));
  ASSERT_TRUE(jbuffer);

  base::span<const uint8_t> span = JavaByteBufferToSpan(env, jbuffer.obj());
  EXPECT_EQ(span.data(), nullptr);
  EXPECT_EQ(span.size(), 0u);
}

}  // namespace base::android
