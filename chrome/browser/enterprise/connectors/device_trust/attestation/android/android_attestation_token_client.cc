// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/policy/core/common/policy_logger.h"
#include "crypto/hash.h"

// Note: The generated JNI headers are from the `attestation/android` target.
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/jni_headers/AttestationTokenGenerator_jni.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/jni_headers/AttestationTokenResult_jni.h"

using base::android::ScopedJavaLocalRef;

namespace enterprise_connectors {

namespace {

// Runs on a thread pool worker. The Java token generation may block (e.g. IPC
// to Play Services), so it must not run on the UI thread. All Java references
// are converted to C++ types before returning, since local refs are only valid
// on the thread that created them.
std::optional<std::vector<uint8_t>> GenerateTokenTask(
    std::vector<uint8_t> content_binding_hash) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_content_binding =
      base::android::ToJavaByteArray(env, content_binding_hash);

  ScopedJavaLocalRef<jobject> j_result =
      Java_AttestationTokenGenerator_generateToken(env, j_content_binding);

  if (!j_result) {
    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Attestation token generation failed: no result returned.";
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> token =
      Java_AttestationTokenResult_getToken(env, j_result);

  if (!token || token->empty()) {
    std::optional<std::string> error_message =
        Java_AttestationTokenResult_getErrorMessage(env, j_result);

    if (error_message) {
      LOG_POLICY(ERROR, DEVICE_TRUST)
          << "Attestation token generation failed: " << *error_message;
    } else {
      LOG_POLICY(ERROR, DEVICE_TRUST)
          << "Attestation token generation failed with unknown error.";
    }
    return std::nullopt;
  }

  return token;
}

void PreWarmCacheTask() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AttestationTokenGenerator_preWarmCache(env);
}

}  // namespace

AndroidAttestationTokenClient::AndroidAttestationTokenClient() = default;
AndroidAttestationTokenClient::~AndroidAttestationTokenClient() = default;

void AndroidAttestationTokenClient::GenerateToken(
    base::span<const uint8_t> content_binding_hash,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback) {
  CHECK_EQ(content_binding_hash.size(), crypto::hash::kSha256Size);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      // Copy the bytes, since `content_binding_hash` is not guaranteed to
      // outlive this call.
      base::BindOnce(&GenerateTokenTask, base::ToVector(content_binding_hash)),
      std::move(callback));
}

void AndroidAttestationTokenClient::PreWarmCache() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&PreWarmCacheTask));
}

}  // namespace enterprise_connectors
