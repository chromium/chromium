// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_bytebuffer.h"

#include "base/android/scoped_java_ref.h"
#include "base/android_runtime_jni_headers/Buffer_jni.h"
#include "base/numerics/safe_conversions.h"

namespace base::android {

base::span<const uint8_t> JavaByteBufferToSpan(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer) {
  auto span = MaybeJavaByteBufferToSpan(env, buffer);
  CHECK(span.has_value());
  return *span;
}

base::span<uint8_t> JavaByteBufferToMutableSpan(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer) {
  auto span = MaybeJavaByteBufferToMutableSpan(env, buffer);
  CHECK(span.has_value());
  return *span;
}

std::optional<base::span<const uint8_t>> MaybeJavaByteBufferToSpan(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer) {
  auto span = MaybeJavaByteBufferToMutableSpan(env, buffer);
  return span ? std::make_optional(base::span<const uint8_t>(*span))
              : std::nullopt;
}

std::optional<base::span<uint8_t>> MaybeJavaByteBufferToMutableSpan(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& buffer) {
  void* data = env->GetDirectBufferAddress(buffer.obj());

  size_t position =
      static_cast<size_t>(JNI_Buffer::Java_Buffer_position(env, buffer));
  size_t limit =
      static_cast<size_t>(JNI_Buffer::Java_Buffer_limit(env, buffer));
  size_t size = limit - position;

  // !data && size == 0 is allowed - this is how a 0-length Buffer is
  // represented.
  if (!data && size > 0) {
    return std::nullopt;
  }

  // SAFETY: This relies on the ByteBuffer to be internally valid.
  return UNSAFE_BUFFERS(base::span<uint8_t>(static_cast<uint8_t*>(data), limit))
      .subspan(position);
}

}  // namespace base::android
