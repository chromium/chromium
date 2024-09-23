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
#include "base/android_runtime_jni_headers/Buffer_jni.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base::android {

TEST(JniByteBuffer, ConversionDoesNotCopy) {
  uint8_t bytes[] = {0, 1, 2, 3};
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> jbuffer(
      env, env->NewDirectByteBuffer(bytes, sizeof(bytes)));
  ASSERT_TRUE(jbuffer);

  base::span<const uint8_t> span = JavaByteBufferToSpan(env, jbuffer);
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
      MaybeJavaByteBufferToSpan(env, jnonbuffer);
  EXPECT_FALSE(maybe_span.has_value());
}

TEST(JniByteBuffer, ZeroByteConversionSucceeds) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbuffer(env,
                                      env->NewDirectByteBuffer(nullptr, 0));
  ASSERT_TRUE(jbuffer);

  base::span<const uint8_t> span = JavaByteBufferToSpan(env, jbuffer);
  EXPECT_EQ(span.data(), nullptr);
  EXPECT_EQ(span.size(), 0u);
}

TEST(JniByteBuffer, PositionDefaultsToZero) {
  JNIEnv* env = AttachCurrentThread();

  std::array<uint8_t, 8> tmp_array;
  ScopedJavaLocalRef<jobject> byte_buffer(
      env, env->NewDirectByteBuffer(tmp_array.data(), tmp_array.size()));
  ASSERT_TRUE(byte_buffer);
  ASSERT_EQ(JNI_Buffer::Java_Buffer_position(env, byte_buffer), 0);
}

TEST(JniByteBuffer, LimitDefaultsToSize) {
  JNIEnv* env = AttachCurrentThread();

  std::array<uint8_t, 8> tmp_array;
  ScopedJavaLocalRef<jobject> byte_buffer(
      env, env->NewDirectByteBuffer(tmp_array.data(), tmp_array.size()));
  ASSERT_TRUE(byte_buffer);
  ASSERT_EQ(
      static_cast<size_t>(JNI_Buffer::Java_Buffer_limit(env, byte_buffer)),
      tmp_array.size());
}

TEST(JniByteBuffer, ChangesToPositionAreRespected) {
  JNIEnv* env = AttachCurrentThread();

  std::array<uint8_t, 8> tmp_array = {0, 1, 2, 3, 4, 5, 6, 7};

  ScopedJavaLocalRef<jobject> byte_buffer(
      env, env->NewDirectByteBuffer(tmp_array.data(), tmp_array.size()));
  ASSERT_TRUE(byte_buffer);

  JNI_Buffer::Java_Buffer_position(env, byte_buffer, 4);
  EXPECT_THAT(JavaByteBufferToSpan(env, byte_buffer), ElementsAre(4, 5, 6, 7));
}

TEST(JniByteBuffer, ChangesToLimitAreRespected) {
  JNIEnv* env = AttachCurrentThread();

  std::array<uint8_t, 8> tmp_array = {0, 1, 2, 3, 4, 5, 6, 7};

  ScopedJavaLocalRef<jobject> byte_buffer(
      env, env->NewDirectByteBuffer(tmp_array.data(), tmp_array.size()));
  ASSERT_TRUE(byte_buffer);

  JNI_Buffer::Java_Buffer_limit(env, byte_buffer, 2);
  base::span<const uint8_t> span = JavaByteBufferToSpan(env, byte_buffer);
  EXPECT_THAT(span, ElementsAre(0, 1));
}

TEST(JniByteBuffer, ChangingBothPositionAndLimitWorks) {
  JNIEnv* env = AttachCurrentThread();

  std::array<uint8_t, 8> tmp_array = {0, 1, 2, 3, 4, 5, 6, 7};

  ScopedJavaLocalRef<jobject> byte_buffer(
      env, env->NewDirectByteBuffer(tmp_array.data(), tmp_array.size()));
  ASSERT_TRUE(byte_buffer);

  JNI_Buffer::Java_Buffer_position(env, byte_buffer, 1);
  JNI_Buffer::Java_Buffer_limit(env, byte_buffer, 4);
  EXPECT_THAT(JavaByteBufferToSpan(env, byte_buffer), ElementsAre(1, 2, 3));
}

}  // namespace base::android
