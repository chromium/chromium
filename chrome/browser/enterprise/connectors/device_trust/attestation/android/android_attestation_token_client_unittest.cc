// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/android_attestation_token_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/policy/core/common/policy_logger.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/enterprise/connectors/device_trust/attestation/android/jni_headers/AttestationTokenGenerator_jni.h"

using base::android::ScopedJavaLocalRef;

namespace enterprise_connectors {

namespace {

using TokenFuture = base::test::TestFuture<std::optional<std::vector<uint8_t>>>;

constexpr char kFailurePrefix[] = "Attestation token generation failed";

// Arbitrary 32-byte content binding hash.
std::vector<uint8_t> ContentBindingHash() {
  return std::vector<uint8_t>(32, 0xab);
}

// Returns true if any chrome://policy/logs entry contains `substring`.
bool HasPolicyLogMessage(std::string_view substring) {
  base::test::TestFuture<base::ListValue> future;
  policy::PolicyLogger::GetInstance()->GetAsList(future.GetCallback());
  base::ListValue logs = future.Take();
  for (const auto& log : logs) {
    if (!log.is_dict()) {
      continue;
    }
    const std::string* message = log.GetDict().FindString("message");
    if (message && message->find(substring) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Installs a fake Java AttestationTokenGeneratorDelegate that returns the given
// result, or a null result object if `return_null_result` is true. The
// previously installed delegate is restored on destruction, similar to
// base::AutoReset.
class ScopedFakeAttestationTokenGenerator {
 public:
  ScopedFakeAttestationTokenGenerator(std::optional<std::vector<uint8_t>> token,
                                      std::optional<std::string> error_message,
                                      bool return_null_result = false) {
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jbyteArray> j_token;
    if (token) {
      j_token = base::android::ToJavaByteArray(env, *token);
    }
    ScopedJavaLocalRef<jstring> j_error_message;
    if (error_message) {
      j_error_message =
          base::android::ConvertUTF8ToJavaString(env, *error_message);
    }
    ScopedJavaLocalRef<jobject> fake_delegate =
        Java_AttestationTokenGenerator_createFakeDelegateForTesting(
            env, j_token, j_error_message, return_null_result);
    previous_delegate_.Reset(
        Java_AttestationTokenGenerator_swapDelegateForTesting(env,
                                                              fake_delegate));
  }

  ScopedFakeAttestationTokenGenerator(
      const ScopedFakeAttestationTokenGenerator&) = delete;
  ScopedFakeAttestationTokenGenerator& operator=(
      const ScopedFakeAttestationTokenGenerator&) = delete;

  ~ScopedFakeAttestationTokenGenerator() {
    Java_AttestationTokenGenerator_swapDelegateForTesting(
        base::android::AttachCurrentThread(), previous_delegate_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> previous_delegate_;
};

}  // namespace

class AndroidAttestationTokenClientTest : public testing::Test {
 protected:
  void SetUp() override {
    policy::PolicyLogger::GetInstance()->ResetLoggerForTesting();
  }

  void TearDown() override {
    policy::PolicyLogger::GetInstance()->ResetLoggerForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  AndroidAttestationTokenClient client_;
};

// Tests that a valid token returned by Java is passed to the callback, that
// the callback runs asynchronously, and that no failure is logged.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenSuccess) {
  const std::vector<uint8_t> kToken = {0x01, 0x02, 0x03};
  ScopedFakeAttestationTokenGenerator fake_generator(
      kToken, /*error_message=*/std::nullopt);

  TokenFuture future;
  client_.GenerateToken(ContentBindingHash(), future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  EXPECT_EQ(future.Get(), kToken);
  EXPECT_FALSE(HasPolicyLogMessage(kFailurePrefix));
}

// Tests that std::nullopt is returned and logged when Java returns a null
// result object, which violates the delegate contract.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenNullResult) {
  ScopedFakeAttestationTokenGenerator fake_generator(
      /*token=*/std::nullopt, /*error_message=*/std::nullopt,
      /*return_null_result=*/true);

  TokenFuture future;
  client_.GenerateToken(ContentBindingHash(), future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_TRUE(HasPolicyLogMessage(
      "Attestation token generation failed: no result returned."));
}

// Tests that std::nullopt is returned when Java returns no token along with an
// error message, and that the Java error message is logged.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenErrorWithMessage) {
  ScopedFakeAttestationTokenGenerator fake_generator(/*token=*/std::nullopt,
                                                     "Token generation error");

  TokenFuture future;
  client_.GenerateToken(ContentBindingHash(), future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_TRUE(HasPolicyLogMessage(
      "Attestation token generation failed: Token generation error"));
}

// Tests that std::nullopt is returned when Java returns neither a token nor an
// error message, and that an unknown error is logged.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenErrorWithoutMessage) {
  ScopedFakeAttestationTokenGenerator fake_generator(
      /*token=*/std::nullopt, /*error_message=*/std::nullopt);

  TokenFuture future;
  client_.GenerateToken(ContentBindingHash(), future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_TRUE(HasPolicyLogMessage(
      "Attestation token generation failed with unknown error."));
}

// Tests that an empty token is treated as a failure and logged.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenEmptyToken) {
  ScopedFakeAttestationTokenGenerator fake_generator(
      std::vector<uint8_t>(), /*error_message=*/std::nullopt);

  TokenFuture future;
  client_.GenerateToken(ContentBindingHash(), future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_TRUE(HasPolicyLogMessage(
      "Attestation token generation failed with unknown error."));
}

// Tests that a content binding hash that is not 32 bytes long is rejected.
TEST_F(AndroidAttestationTokenClientTest, GenerateTokenInvalidHashSize) {
  EXPECT_CHECK_DEATH(
      client_.GenerateToken(std::vector<uint8_t>(31, 0xab), base::DoNothing()));
}

}  // namespace enterprise_connectors
