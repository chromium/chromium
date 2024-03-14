// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"

#include <string_view>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"

// Size of buffer to allocate on the stack for string conversion.
#define BUFFER_SIZE 1024

namespace {

// Internal version that does not use a scoped local pointer.
jstring ConvertUTF16ToJavaStringImpl(JNIEnv* env, std::u16string_view str) {
  jstring result = env->NewString(reinterpret_cast<const jchar*>(str.data()),
                                  base::checked_cast<jsize>(str.length()));
  base::android::CheckException(env);
  return result;
}

}  // namespace

namespace base {
namespace android {

void ConvertJavaStringToUTF8(JNIEnv* env, jstring str, std::string* result) {
  DCHECK(str);
  if (!str) {
    LOG(WARNING) << "ConvertJavaStringToUTF8 called with null string.";
    result->clear();
    return;
  }
  const jsize length = env->GetStringLength(str);
  if (length <= 0) {
    result->clear();
    CheckException(env);
    return;
  }
  // JNI's GetStringUTFChars() and GetStringUTFRegion returns strings in Java
  // "modified" UTF8, so instead get the String in UTF16 and convert using
  // chromium's conversion function that yields plain (non Java-modified) UTF8.
  if (length <= BUFFER_SIZE) {
    // fast path, allocate temporary buffer on the stack and use GetStringRegion
    // to copy the utf-16 characters into it with no heap allocation.
    // https://developer.android.com/training/articles/perf-jni#utf-8-and-utf-16-strings:~:text=stack%2Dallocated%20buffer
    std::array<jchar, BUFFER_SIZE> chars;
    // GetStringRegion does not copy a null terminated string so the length must
    // be explicitly passed to UTF16ToUTF8.
    env->GetStringRegion(str, 0, length, chars.data());
    UTF16ToUTF8(reinterpret_cast<const char16_t*>(chars.data()),
                static_cast<size_t>(length), result);
  } else {
    // slow path
    // GetStringChars doesn't NULL-terminate the strings it returns, so the
    // length must be explicitly passed to UTF16ToUTF8.
    const jchar* chars = env->GetStringChars(str, NULL);
    DCHECK(chars);
    UTF16ToUTF8(reinterpret_cast<const char16_t*>(chars),
                static_cast<size_t>(length), result);
    env->ReleaseStringChars(str, chars);
  }
  CheckException(env);
}

std::string ConvertJavaStringToUTF8(JNIEnv* env, jstring str) {
  std::string result;
  ConvertJavaStringToUTF8(env, str, &result);
  return result;
}

std::string ConvertJavaStringToUTF8(const JavaRef<jstring>& str) {
  return ConvertJavaStringToUTF8(AttachCurrentThread(), str.obj());
}

std::string ConvertJavaStringToUTF8(JNIEnv* env, const JavaRef<jstring>& str) {
  return ConvertJavaStringToUTF8(env, str.obj());
}

ScopedJavaLocalRef<jstring> ConvertUTF8ToJavaString(JNIEnv* env,
                                                    std::string_view str) {
  // JNI's NewStringUTF expects "modified" UTF8 so instead create the string
  // via our own UTF16 conversion utility.
  // Further, Dalvik requires the string passed into NewStringUTF() to come from
  // a trusted source. We can't guarantee that all UTF8 will be sanitized before
  // it gets here, so constructing via UTF16 side-steps this issue.
  // (Dalvik stores strings internally as UTF16 anyway, so there shouldn't be
  // a significant performance hit by doing it this way).
  return ScopedJavaLocalRef<jstring>(env, ConvertUTF16ToJavaStringImpl(
      env, UTF8ToUTF16(str)));
}

void ConvertJavaStringToUTF16(JNIEnv* env,
                              jstring str,
                              std::u16string* result) {
  DCHECK(str);
  if (!str) {
    LOG(WARNING) << "ConvertJavaStringToUTF16 called with null string.";
    result->clear();
    return;
  }
  const jsize length = env->GetStringLength(str);
  if (length <= 0) {
    result->clear();
    CheckException(env);
    return;
  }
  if (length <= BUFFER_SIZE) {
    // fast path, allocate temporary buffer on the stack and use GetStringRegion
    // to copy the utf-16 characters into it with no heap allocation.
    // https://developer.android.com/training/articles/perf-jni#utf-8-and-utf-16-strings:~:text=stack%2Dallocated%20buffer
    std::array<jchar, BUFFER_SIZE> chars;
    env->GetStringRegion(str, 0, length, chars.data());
    // GetStringRegion does not copy a null terminated string so the length must
    // be explicitly passed to assign.
    result->assign(reinterpret_cast<const char16_t*>(chars.data()),
                   static_cast<size_t>(length));
  } else {
    // slow path
    const jchar* chars = env->GetStringChars(str, NULL);
    DCHECK(chars);
    // GetStringChars doesn't NULL-terminate the strings it returns, so the
    // length must be explicitly passed to assign.
    result->assign(reinterpret_cast<const char16_t*>(chars),
                   static_cast<size_t>(length));
    env->ReleaseStringChars(str, chars);
  }
  CheckException(env);
}

std::u16string ConvertJavaStringToUTF16(JNIEnv* env, jstring str) {
  std::u16string result;
  ConvertJavaStringToUTF16(env, str, &result);
  return result;
}

std::u16string ConvertJavaStringToUTF16(const JavaRef<jstring>& str) {
  return ConvertJavaStringToUTF16(AttachCurrentThread(), str.obj());
}

std::u16string ConvertJavaStringToUTF16(JNIEnv* env,
                                        const JavaRef<jstring>& str) {
  return ConvertJavaStringToUTF16(env, str.obj());
}

ScopedJavaLocalRef<jstring> ConvertUTF16ToJavaString(JNIEnv* env,
                                                     std::u16string_view str) {
  return ScopedJavaLocalRef<jstring>(env,
                                     ConvertUTF16ToJavaStringImpl(env, str));
}

}  // namespace android
}  // namespace base
