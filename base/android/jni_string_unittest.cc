// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(JniString, BasicConversionsUTF8) {
  const std::string kSimpleString = "SimpleTest8";
  JNIEnv* env = AttachCurrentThread();
  std::string result =
      ConvertJavaStringToUTF8(ConvertUTF8ToJavaString(env, kSimpleString));
  EXPECT_EQ(kSimpleString, result);
}

TEST(JniString, BasicConversionsUTF16) {
  const std::u16string kSimpleString = u"SimpleTest16";
  JNIEnv* env = AttachCurrentThread();
  std::u16string result =
      ConvertJavaStringToUTF16(ConvertUTF16ToJavaString(env, kSimpleString));
  EXPECT_EQ(kSimpleString, result);
}

TEST(JniString, EmptyConversionUTF8) {
  const std::string kEmptyString;
  JNIEnv* env = AttachCurrentThread();
  std::string result =
      ConvertJavaStringToUTF8(ConvertUTF8ToJavaString(env, kEmptyString));
  EXPECT_EQ(kEmptyString, result);
}

TEST(JniString, EmptyConversionUTF16) {
  const std::u16string kEmptyString;
  JNIEnv* env = AttachCurrentThread();
  std::u16string result =
      ConvertJavaStringToUTF16(ConvertUTF16ToJavaString(env, kEmptyString));
  EXPECT_EQ(kEmptyString, result);
}

}  // namespace android
}  // namespace base
