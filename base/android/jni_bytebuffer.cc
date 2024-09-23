// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_bytebuffer.h"

#include "base/numerics/safe_conversions.h"

namespace base::android {

base::span<const uint8_t> JavaByteBufferToSpan(JNIEnv* env, jobject buffer) {
  auto span = MaybeJavaByteBufferToSpan(env, buffer);
  CHECK(span.has_value());
  return *span;
}

base::span<uint8_t> JavaByteBufferToMutableSpan(JNIEnv* env, jobject buffer) {
  auto span = MaybeJavaByteBufferToMutableSpan(env, buffer);
  CHECK(span.has_value());
  return *span;
}

std::optional<base::span<const uint8_t>> MaybeJavaByteBufferToSpan(
    JNIEnv* env,
    jobject buffer) {
  auto span = MaybeJavaByteBufferToMutableSpan(env, buffer);
  return span ? std::make_optional(base::span<const uint8_t>(*span))
              : std::nullopt;
}

std::optional<base::span<uint8_t>> MaybeJavaByteBufferToMutableSpan(
    JNIEnv* env,
    jobject buffer) {
  void* data = env->GetDirectBufferAddress(buffer);
  jlong size = env->GetDirectBufferCapacity(buffer);

  // !data && size == 0 is allowed - this is how a 0-length Buffer is
  // represented.
  if (size < 0 || (!data && size > 0)) {
    return std::nullopt;
  }

  // SAFETY: This relies on the ByteBuffer to be internally valid.
  return UNSAFE_BUFFERS(base::span<uint8_t>(static_cast<uint8_t*>(data),
                                            base::checked_cast<size_t>(size)));
}

}  // namespace base::android
