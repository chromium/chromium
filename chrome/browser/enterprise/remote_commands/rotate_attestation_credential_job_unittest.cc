// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/rotate_attestation_credential_job.h"

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_device_trust_key_manager.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using KeyRotationResult =
    enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult;

using testing::_;
using testing::Invoke;

namespace enterprise_commands {

namespace {

constexpr policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

const char kNonceField[] = "nonce";
const char kNonceValue[] = "some nonce value";

enterprise_management::RemoteCommand CreateCommand() {
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::
          RemoteCommand_Type_BROWSER_ROTATE_ATTESTATION_CREDENTIAL);
  command_proto.set_command_id(kUniqueID);
  return command_proto;
}

std::string GetPayloadWithNonce() {
  base::Value::Dict root;
  root.Set(kNonceField, kNonceValue);

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  return payload;
}

std::string GetEmptyPayload() {
  return "{}";
}

}  // namespace

class RotateAttestationCredentialJobTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  RotateAttestationCredentialJobTest() {
    feature_list_.InitWithFeatureState(
        enterprise_connectors::kDTCKeyRotationEnabled,
        is_key_rotation_enabled());
  }

  void MockKeyRotationWith(KeyRotationResult result) {
    EXPECT_CALL(mock_key_manager_, RotateKey(kNonceValue, _))
        .WillOnce(Invoke(
            [result](const std::string& nonce,
                     base::OnceCallback<void(KeyRotationResult)> callback) {
              std::move(callback).Run(result);
            }));
  }

  std::unique_ptr<RotateAttestationCredentialJob> CreateJob(
      enterprise_management::RemoteCommand command_proto) {
    auto job =
        std::make_unique<RotateAttestationCredentialJob>(&mock_key_manager_);
    EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                          enterprise_management::SignedData{}));
    EXPECT_EQ(kUniqueID, job->unique_id());
    EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());

    return job;
  }

  bool is_key_rotation_enabled() { return GetParam(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  testing::StrictMock<enterprise_connectors::test::MockDeviceTrustKeyManager>
      mock_key_manager_;
};

// Tests that a job conveys a successful key rotation properly.
TEST_P(RotateAttestationCredentialJobTest, SuccessRun) {
  if (is_key_rotation_enabled()) {
    MockKeyRotationWith(KeyRotationResult::SUCCESS);
  }

  auto command_proto = CreateCommand();
  auto json_payload = GetPayloadWithNonce();
  command_proto.set_payload(json_payload);

  base::RunLoop run_loop;
  auto job = CreateJob(std::move(command_proto));

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop.QuitClosure()));
  run_loop.Run();

  if (is_key_rotation_enabled()) {
    EXPECT_EQ(job->status(), policy::RemoteCommandJob::SUCCEEDED);
  } else {
    EXPECT_EQ(job->status(), policy::RemoteCommandJob::FAILED);
  }
}

// Tests that a job conveys a failed key rotation properly.
TEST_P(RotateAttestationCredentialJobTest, FailedRun) {
  if (is_key_rotation_enabled()) {
    MockKeyRotationWith(KeyRotationResult::FAILURE);
  }

  auto command_proto = CreateCommand();
  auto json_payload = GetPayloadWithNonce();
  command_proto.set_payload(json_payload);

  base::RunLoop run_loop;
  auto job = CreateJob(std::move(command_proto));

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(job->status(), policy::RemoteCommandJob::FAILED);
}

// Tests that a job conveys a cancelled key rotation properly.
TEST_P(RotateAttestationCredentialJobTest, CancelledRun) {
  if (is_key_rotation_enabled()) {
    MockKeyRotationWith(KeyRotationResult::CANCELLATION);
  }

  auto command_proto = CreateCommand();
  auto json_payload = GetPayloadWithNonce();
  command_proto.set_payload(json_payload);

  base::RunLoop run_loop;
  auto job = CreateJob(std::move(command_proto));

  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(job->status(), policy::RemoteCommandJob::FAILED);
}

// Tests that a job handles a bad payload properly.
TEST_P(RotateAttestationCredentialJobTest, BadPayload) {
  auto command_proto = CreateCommand();
  auto json_payload = GetEmptyPayload();
  command_proto.set_payload(json_payload);

  auto job =
      std::make_unique<RotateAttestationCredentialJob>(&mock_key_manager_);
  EXPECT_FALSE(job->Init(base::TimeTicks::Now(), command_proto,
                         enterprise_management::SignedData{}));
}

INSTANTIATE_TEST_SUITE_P(, RotateAttestationCredentialJobTest, testing::Bool());

}  // namespace enterprise_commands
