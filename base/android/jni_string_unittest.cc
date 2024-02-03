// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

TEST(JniString, FastConversionsUTF8) {
  const std::string kSimpleString = "SimpleTest8";
  JNIEnv* env = AttachCurrentThread();
  std::string result =
      ConvertJavaStringToUTF8(ConvertUTF8ToJavaString(env, kSimpleString));
  EXPECT_EQ(kSimpleString, result);
}

TEST(JniString, FastConversionsUTF16) {
  const std::u16string kSimpleString = u"SimpleTest16";
  JNIEnv* env = AttachCurrentThread();
  std::u16string result =
      ConvertJavaStringToUTF16(ConvertUTF16ToJavaString(env, kSimpleString));
  EXPECT_EQ(kSimpleString, result);
}

TEST(JniString, SlowConversionsUTF8) {
  constexpr auto length = 2048;
  std::array<char, length> kLongArray;
  for (int i = 0; i < length; i++) {
    kLongArray[i] = 'a';
  }
  std::string kLongString;
  kLongString.assign(reinterpret_cast<const char*>(kLongArray.data()),
                     static_cast<size_t>(length));
  JNIEnv* env = AttachCurrentThread();
  std::string result =
      ConvertJavaStringToUTF8(ConvertUTF8ToJavaString(env, kLongString));
  EXPECT_EQ(kLongString, result);
}

TEST(JniString, SlowConversionsUTF16) {
  constexpr auto length = 2048;
  std::array<char16_t, length> kLongArray;
  for (int i = 0; i < length; i++) {
    kLongArray[i] = u'a';
  }
  std::u16string kLongString;
  kLongString.assign(reinterpret_cast<const char16_t*>(kLongArray.data()),
                     static_cast<size_t>(length));
  JNIEnv* env = AttachCurrentThread();
  std::u16string result =
      ConvertJavaStringToUTF16(ConvertUTF16ToJavaString(env, kLongString));
  EXPECT_EQ(kLongString, result);
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
