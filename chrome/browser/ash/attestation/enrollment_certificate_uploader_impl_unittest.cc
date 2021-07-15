// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/attestation/fake_certificate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using CertStatus = ash::attestation::EnrollmentCertificateUploader::Status;
using testing::_;
using ::testing::InSequence;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

constexpr int kRetryLimit = 3;

void CertCallbackSuccess(AttestationFlow::CertificateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_SUCCESS, "fake_cert"));
}

void CertCallbackUnspecifiedFailure(
    AttestationFlow::CertificateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_UNSPECIFIED_FAILURE, ""));
}

void CertCallbackBadRequestFailure(
    AttestationFlow::CertificateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

void StatusCallbackFailure(policy::CloudPolicyClient::StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

void StatusCallbackSuccess(policy::CloudPolicyClient::StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace

class EnrollmentCertificateUploaderTest : public ::testing::Test {
 public:
  EnrollmentCertificateUploaderTest()
      : uploader_(&policy_client_, &attestation_flow_) {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    policy_client_.SetDMToken("fake_dm_token");

    uploader_.set_retry_limit(kRetryLimit);
    uploader_.set_retry_delay(base::TimeDelta());
  }

 protected:
  void Run(CertStatus expected_status) {
    uploader_.ObtainAndUploadCertificate(
        base::BindLambdaForTesting([expected_status](CertStatus status) {
          EXPECT_EQ(status, expected_status);
        }));

    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;

  EnrollmentCertificateUploaderImpl uploader_;
};

TEST_F(EnrollmentCertificateUploaderTest, UnregisteredPolicyClient) {
  InSequence s;

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentCertificate(_, _))
      .Times(0);

  policy_client_.SetDMToken("");
  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateUnspecifiedFailure) {
  InSequence s;

  // Shall try to fetch existing certificate through all attempts.
  constexpr int total_attempts = kRetryLimit + 1;
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(total_attempts)
      .WillRepeatedly(WithArgs<5>(Invoke(CertCallbackUnspecifiedFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateBadRequestFailure) {
  InSequence s;

  // Shall fail without retries.
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackBadRequestFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadCertificateFailure) {
  InSequence s;

  // First, uploader shall try to fetch and upload existing certificate. After
  // first failure, it shall retry with a new one until retry limit.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate("fake_cert", _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));

  for (int i = 0; i < kRetryLimit; ++i) {
    // Cannot use Times(kRetryLimit) because of expected sequence.
    EXPECT_CALL(attestation_flow_,
                GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                               /*force_new_key=*/true, _, _))
        .Times(1)
        .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate("fake_cert", _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));
  }

  Run(/*expected_status=*/CertStatus::kFailedToUpload);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadCertificateSuccessWithNewKey) {
  InSequence s;

  // First, uploader shall try to fetch and upload existing certificate. After
  // first failure, it shall retry with a new one.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate("fake_cert", _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));

  // Make an intermediate certificate fetch failure to check new key is still
  // requested.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackUnspecifiedFailure)));

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate("fake_cert", _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest, NewCertificate) {
  InSequence s;

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));

  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate("fake_cert", _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadsOnlyOnce) {
  InSequence s;

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate("fake_cert", _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
  // Mocks expect single upload request and will fail if requested more than
  // once.
  Run(/*expected_status=*/CertStatus::kSuccess);
}

}  // namespace attestation
}  // namespace ash
