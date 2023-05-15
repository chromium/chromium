// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/attestation/stub_attestation_features.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

namespace {

using CertStatus = EnrollmentCertificateUploader::Status;
using CertCallback = AttestationFlow::CertificateCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

constexpr int kRetryLimit = 3;

void CertCallbackSuccess(CertCallback callback,
                         const std::string& certificate) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_SUCCESS, certificate));
}

void CertCallbackUnspecifiedFailure(CertCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_UNSPECIFIED_FAILURE, ""));
}

void CertCallbackBadRequestFailure(CertCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

void ResultCallbackFailure(policy::CloudPolicyClient::ResultCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                policy::CloudPolicyClient::Result(
                                    policy::DM_STATUS_TEMPORARY_UNAVAILABLE)));
}

void ResultCallbackSuccess(policy::CloudPolicyClient::ResultCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), policy::CloudPolicyClient::Result(
                                              policy::DM_STATUS_SUCCESS)));
}

}  // namespace

class EnrollmentCertificateUploaderTest : public ::testing::Test {
 public:
  EnrollmentCertificateUploaderTest() : uploader_(&policy_client_) {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    policy_client_.SetDMToken("fake_dm_token");

    uploader_.set_retry_limit_for_testing(kRetryLimit);
    uploader_.set_retry_delay_for_testing(base::TimeDelta());
    uploader_.set_attestation_flow_for_testing(&attestation_flow_);
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
  ScopedStubAttestationFeatures attestation_features_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;

  EnrollmentCertificateUploaderImpl uploader_;
};

TEST_F(EnrollmentCertificateUploaderTest, UnregisteredPolicyClient) {
  InSequence s;

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentCertificate(_, _))
      .Times(0);

  policy_client_.SetDMToken("");
  Run(/*expected_status=*/CertStatus::kInvalidClient);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateUnspecifiedFailure) {
  InSequence s;

  // Shall try to fetch existing certificate through all attempts.
  constexpr int total_attempts = kRetryLimit + 1;
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(total_attempts)
      .WillRepeatedly(WithArgs<7>(Invoke(CertCallbackUnspecifiedFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateBadRequestFailure) {
  InSequence s;

  // Shall fail without retries.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke(CertCallbackBadRequestFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest,
       GetCertificateFailureWithUnregisteredClientOnRetry) {
  InSequence s;

  // Shall fail on |CloudPolicyClient::is_registered()| check and not retry.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([this](CertCallback callback) {
        policy_client_.SetDMToken("");
        CertCallbackUnspecifiedFailure(std::move(callback));
      })));

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .Times(0);

  Run(/*expected_status=*/CertStatus::kInvalidClient);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadCertificateFailure) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  constexpr int total_attempts = kRetryLimit + 1;
  for (int i = 0; i < total_attempts; ++i) {
    // Cannot use Times(kRetryLimit) because of expected sequence.
    EXPECT_CALL(attestation_flow_,
                GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                               /*force_new_key=*/true, _, _, _, _))
        .Times(1)
        .WillOnce(
            WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
              CertCallbackSuccess(std::move(callback), valid_certificate);
            })));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke(ResultCallbackFailure)));
  }

  Run(/*expected_status=*/CertStatus::kFailedToUpload);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UploadCertificateFailureWithUnregisteredClientOnRetry) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  // Shall fail on |CloudPolicyClient::is_registered()| check and not retry.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(
          Invoke([this](policy::CloudPolicyClient::ResultCallback callback) {
            policy_client_.SetDMToken("");
            ResultCallbackFailure(std::move(callback));
          })));

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(0);

  Run(/*expected_status=*/CertStatus::kInvalidClient);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UnregisteredClientAfterValidCertificateRequested) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  // Shall fail on |CloudPolicyClient::is_registered()| check and not retry.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .WillOnce(
          WithArgs<7>(Invoke([this, valid_certificate](CertCallback callback) {
            policy_client_.SetDMToken("");
            CertCallbackSuccess(std::move(callback), valid_certificate);
          })));

  EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentCertificate(_, _))
      .Times(0);

  Run(/*expected_status=*/CertStatus::kInvalidClient);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadValidRsaCertificate) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;
  // When only RSA is supported, we should use RSA.
  attestation_features_.Get()->Clear();
  attestation_features_.Get()->set_is_available(true);
  attestation_features_.Get()->set_is_rsa_supported(true);
  attestation_features_.Get()->set_is_ecc_supported(false);

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true,
                             ::attestation::KEY_TYPE_RSA, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(ResultCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadValidEccCertificate) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;
  // When both ECC/RSA are supported, we should prefer ECC.
  attestation_features_.Get()->Clear();
  attestation_features_.Get()->set_is_available(true);
  attestation_features_.Get()->set_is_rsa_supported(true);
  attestation_features_.Get()->set_is_ecc_supported(true);

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true,
                             ::attestation::KEY_TYPE_ECC, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(ResultCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest, GetFeaturesNoAttestationAvailable) {
  InSequence s;
  // When both ECC/RSA are supported, we should prefer ECC.
  attestation_features_.Get()->Clear();
  attestation_features_.Get()->set_is_available(false);

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .Times(0);
  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetFeaturesNoAvailableCryptoKeyType) {
  InSequence s;
  // When both ECC/RSA are supported, we should prefer ECC.
  attestation_features_.Get()->Clear();
  attestation_features_.Get()->set_is_available(true);
  attestation_features_.Get()->set_is_rsa_supported(false);
  attestation_features_.Get()->set_is_ecc_supported(false);

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _))
      .Times(0);
  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadValidCertificateOnlyOnce) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(ResultCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);

  // Mocks expect single upload request and will fail if requested more than
  // once.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _, _, _))
      .Times(1)
      .WillOnce(WithArgs<7>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  Run(/*expected_status=*/CertStatus::kSuccess);
}

}  // namespace attestation
}  // namespace ash
