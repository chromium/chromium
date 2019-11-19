// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/chromeos/attestation/fake_certificate.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void CertCallbackSuccess(const AttestationFlow::CertificateCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, ATTESTATION_SUCCESS, "fake_cert"));
}

void CertCallbackUnspecifiedFailure(
    const AttestationFlow::CertificateCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, ATTESTATION_UNSPECIFIED_FAILURE, ""));
}

void CertCallbackBadRequestFailure(
    const AttestationFlow::CertificateCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(callback, ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(callback, true));
}

}  // namespace

class EnrollmentCertificateUploaderTest : public ::testing::Test {
 public:
  EnrollmentCertificateUploaderTest() {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    policy_client_.SetDMToken("fake_dm_token");
  }

 protected:
  void SetupMocks() {
    // Setup expected cert uploads. Use WillOnce() so StrictMock will trigger an
    // error if our expectations are not met exactly. We want to verify that
    // during a single run through the uploader only one upload operation occurs
    // (because it is costly).
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate("fake_cert", _))
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

    // Setup expected cert generations. Again use WillOnce(). Cert generation
    // is another costly operation and if it gets triggered more than once
    // during a single pass this indicates a logical problem in the uploader.
    EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _))
        .WillOnce(WithArgs<5>(Invoke(CertCallbackSuccess)));
  }

  void Run(bool expected_status) {
    EnrollmentCertificateUploaderImpl uploader(
        &policy_client_, &cryptohome_client_, &attestation_flow_);
    uploader.set_retry_limit(3);
    uploader.set_retry_delay(base::TimeDelta());

    uploader.ObtainAndUploadCertificate(
        base::BindLambdaForTesting([expected_status](bool status) {
          EXPECT_EQ(status, expected_status);
        }));

    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  FakeCryptohomeClient cryptohome_client_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
};

TEST_F(EnrollmentCertificateUploaderTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  Run(false /* expected_status */);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateUnspecifiedFailure) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _))
      .WillRepeatedly(WithArgs<5>(Invoke(CertCallbackUnspecifiedFailure)));
  Run(false /* expected_status */);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateBadRequestFailure) {
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _))
      .WillOnce(WithArgs<5>(Invoke(CertCallbackBadRequestFailure)));
  Run(false /* expected_status */);
}

TEST_F(EnrollmentCertificateUploaderTest, NewCertificate) {
  SetupMocks();
  Run(true /* expected_status */);
}

TEST_F(EnrollmentCertificateUploaderTest, DBusFailureRetry) {
  SetupMocks();
  // Simulate a DBus failure.
  cryptohome_client_.SetServiceIsAvailable(false);

  // Emulate delayed service initialization.
  // Run() instantiates an Observer, which synchronously calls
  // TpmAttestationDoesKeyExist() and fails. During this call, we make the
  // service available in the next run, so on retry, it will successfully
  // return the result.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](FakeCryptohomeClient* cryptohome_client) {
                       cryptohome_client->SetServiceIsAvailable(true);
                     },
                     base::Unretained(&cryptohome_client_)));
  Run(true /* expected_status */);
}

}  // namespace attestation
}  // namespace chromeos
