// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_TOKEN_ANDROID_H_
#define BASE_ANDROID_TOKEN_ANDROID_H_

#include <jni.h>

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/token.h"

namespace base {
namespace android {

class BASE_EXPORT TokenAndroid {
 public:
  // Create a Java Token with the same value as `token`.
  static ScopedJavaLocalRef<jobject> Create(JNIEnv* env,
                                            const base::Token& token);

  // Creates a Token from `j_token`.
  static base::Token FromJavaToken(JNIEnv* env,
                                   const JavaRef<jobject>& j_token);

  // Converts the collection of `tokens` to an array of Token objects in Java.
  static ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfTokens(
      JNIEnv* env,
      base::span<std::optional<base::Token>> tokens);

  TokenAndroid() = delete;
  TokenAndroid(const TokenAndroid&) = delete;
  TokenAndroid& operator=(const TokenAndroid&) = delete;
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_TOKEN_ANDROID_H_
