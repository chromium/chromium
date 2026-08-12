// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace {

constexpr jsize kStackBufferSize = 1024;

// Internal version that does not use a scoped local pointer.
jstring ConvertUTF16ToJavaStringImpl(JNIEnv* env, std::u16string_view str) {
  jstring result = env->NewString(reinterpret_cast<const uint16_t*>(str.data()),
                                  base::checked_cast<jsize>(str.length()));
  base::android::CheckException(env);
  return result;
}

}  // namespace

namespace base {
namespace android {

void ConvertJavaStringToUTF8(JNIEnv* env, jstring str, std::string* result) {
  if (!str) {
    result->clear();
    return;
  }
  int32_t length = env->GetStringLength(str);

  // Stack allocation for smallish strings.
  std::array<uint16_t, kStackBufferSize> stack_buf;

  uint16_t* buf = stack_buf.data();
  std::vector<uint16_t> heap_buf;

  // Heap allocation for large ones.
  if (length > kStackBufferSize) {
    heap_buf.resize(static_cast<size_t>(length));
    buf = heap_buf.data();
  }

  // Why use GetStringRegion():
  //  * `GetStringChars()` does a heap allocation & requires a second JNI call
  //    to release the buffer.
  //  * `GetStringCharsCritical()` does the same for strings that internally
  //    stored as "compressed" (no multi-byte chars).
  //  * `GetStringUTFRegion()` returns modified UTF-8, which is not helpful.
  env->GetStringRegion(str, 0, length, buf);
  UTF16ToUTF8(reinterpret_cast<const char16_t*>(buf),
              static_cast<size_t>(length), result);
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
  // ART allocates new empty strings, so use a singleton when applicable.
  if (str.empty()) {
    return jni_zero::g_empty_string.AsLocalRef(env);
  }

  // This is a similar optimization to the one in ConvertJavaStringToUTF8()
  // above. However, this is only safe if the string is ASCII as all ASCII
  // characters are the same in UTF8 and UTF16. This also bypasses any
  // "modified" UTF8 concerns with JNI's NewStringUTF(). The heap vector version
  // of this is already handled in UTF8ToUTF16().
  const size_t length = str.length();
  if (length <= kStackBufferSize && base::IsStringASCII(str)) {
    std::array<uint16_t, kStackBufferSize> chars;
    base::span<uint16_t> chars_span =
        base::span<uint16_t>(chars).first(length);
    for (size_t i = 0; i < length; ++i) {
      chars_span[i] = static_cast<uint16_t>(str[i]);
    }
    jstring result =
        env->NewString(chars_span.data(), base::checked_cast<jsize>(length));
    CheckException(env);
    return jni_zero::AdoptRef(env, result);
  }

  // JNI's NewStringUTF expects "modified" UTF8 so instead create the string
  // via our own UTF16 conversion utility.
  // Further, Dalvik requires the string passed into NewStringUTF() to come from
  // a trusted source. We can't guarantee that all UTF8 will be sanitized before
  // it gets here, so constructing via UTF16 side-steps this issue.
  // (Dalvik stores strings internally as UTF16 anyway, so there shouldn't be
  // a significant performance hit by doing it this way).
  return jni_zero::AdoptRef(
      env, ConvertUTF16ToJavaStringImpl(env, UTF8ToUTF16(str)));
}

void ConvertJavaStringToUTF16(JNIEnv* env,
                              jstring str,
                              std::u16string* result) {
  if (!str) {
    result->clear();
    return;
  }
  int32_t length = env->GetStringLength(str);

  // Stack allocation for smallish strings.
  std::array<uint16_t, kStackBufferSize> stack_buf;

  uint16_t* buf = stack_buf.data();
  std::vector<uint16_t> heap_buf;

  // Heap allocation for large ones.
  if (length > kStackBufferSize) {
    heap_buf.resize(static_cast<size_t>(length));
    buf = heap_buf.data();
  }

  // See comment in ConvertJavaStringToUTF8() about why GetStringRegion() is
  // used.
  env->GetStringRegion(str, 0, length, buf);
  result->assign(reinterpret_cast<const char16_t*>(buf),
                 static_cast<size_t>(length));
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
  // ART allocates new empty strings, so use a singleton when applicable.
  if (str.empty()) {
    return jni_zero::g_empty_string.AsLocalRef(env);
  }
  return jni_zero::AdoptRef(env, ConvertUTF16ToJavaStringImpl(env, str));
}

}  // namespace android
}  // namespace base
