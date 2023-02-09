// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/attestation/enrollment_id_upload_manager.h"
#include "chrome/browser/ash/attestation/mock_enrollment_certificate_uploader.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

namespace {

using CertificateStatus = EnrollmentCertificateUploader::Status;
using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArgs;

constexpr int kRetryLimit = 3;

void ResultCallbackSuccess(policy::CloudPolicyClient::ResultCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), policy::CloudPolicyClient::Result(
                                              policy::DM_STATUS_SUCCESS)));
}

}  // namespace

class EnrollmentIdUploadManagerTest : public DeviceSettingsTestBase {
 public:
  EnrollmentIdUploadManagerTest() = default;

  // Disallow copy and assign.
  EnrollmentIdUploadManagerTest(const EnrollmentIdUploadManagerTest&) = delete;
  EnrollmentIdUploadManagerTest& operator=(
      const EnrollmentIdUploadManagerTest&) = delete;

  ~EnrollmentIdUploadManagerTest() override = default;

  void SetUp() override {
    AttestationClient::InitializeFake();
    DeviceSettingsTestBase::SetUp();

    policy_client_.SetDMToken("fake_dm_token");

    EXPECT_TRUE(base::HexStringToString(kEnrollmentId, &enrollment_id_));

    AttestationClient::Get()
        ->GetTestInterface()
        ->set_enrollment_id_ignore_cache(enrollment_id_);
    AttestationClient::Get()->GetTestInterface()->set_cached_enrollment_id(
        "unexpected query to cached enrollment ID");
  }

  void TearDown() override {
    EXPECT_TRUE(
        enrollment_id_upload_manager_->upload_manager_callbacks_.empty());
    enrollment_id_upload_manager_.reset();
    DeviceSettingsTestBase::TearDown();
    AttestationClient::Shutdown();
  }

 protected:
  static constexpr char kEnrollmentId[] =
      "6fcc0ebddec3db9500cf82476d594f4d60db934c5b47fa6085c707b2a93e205b";

  void SetUpEnrollmentIdUploadManager() {
    enrollment_id_upload_manager_ = std::make_unique<EnrollmentIdUploadManager>(
        &policy_client_, device_settings_service_.get(),
        &certificate_uploader_);
    enrollment_id_upload_manager_->set_retry_limit_for_testing(kRetryLimit);
    enrollment_id_upload_manager_->set_retry_delay_for_testing(0);
  }

  void ExpectUploadEnrollmentCertificate(CertificateStatus status, int times) {
    EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
        .Times(times)
        .WillRepeatedly(Invoke(
            [status](
                base::OnceCallback<void(CertificateStatus status)> callback) {
              std::move(callback).Run(status);
            }));
  }

  void ExpectUploadEnrollmentId(int times) {
    // Setting a mock behavior with 0 times causes warnings.
    if (times == 0) {
      return;
    }
    EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentId(enrollment_id_, _))
        .Times(times)
        .WillRepeatedly(WithArgs<1>(Invoke(ResultCallbackSuccess)));
  }

  void SetUpDevicePolicy(bool enrollment_id_needed) {
    device_policy_->policy_data().set_enrollment_id_needed(
        enrollment_id_needed);
  }

  void PropagateDevicePolicy() {
    ReloadDevicePolicy();
    ReloadDeviceSettings();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void CheckStatusAndQuitLoop(bool expect_success, bool success) {
    EXPECT_EQ(expect_success, success);
    run_loop_.Quit();
  }

  // Calls ObtainAndUploadEnrollmentId(), waits for the callback, and verifies
  // the expected status on the callback.
  void TestObtainAndUploadEnrollmentId(bool expect_success) {
    enrollment_id_upload_manager_->ObtainAndUploadEnrollmentId(
        base::BindOnce(&EnrollmentIdUploadManagerTest::CheckStatusAndQuitLoop,
                       base::Unretained(this), expect_success));
    run_loop_.Run();
  }

  base::RunLoop run_loop_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
  StrictMock<MockEnrollmentCertificateUploader> certificate_uploader_;
  std::unique_ptr<EnrollmentIdUploadManager> enrollment_id_upload_manager_;
  std::string enrollment_id_;
};

constexpr char EnrollmentIdUploadManagerTest::kEnrollmentId[];

TEST_F(EnrollmentIdUploadManagerTest, UploadEnrollmentCertificate) {
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  ExpectUploadEnrollmentCertificate(CertificateStatus::kSuccess, /*times=*/1);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest,
       UploadEnrollmentCertificateFromExistingPolicy) {
  // This test will trigger the manager work twice in a row: when the
  // manager is created, and when it gets notified later on.
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  PropagateDevicePolicy();
  ExpectUploadEnrollmentCertificate(CertificateStatus::kSuccess, /*times=*/2);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, FailedToUploadEnrollmentCertificate) {
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToUpload,
                                    /*times=*/1);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, UploadEnrollmentId) {
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/1);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, UploadEnrollmentIdFromExistingPolicy) {
  // This test will trigger the manager work twice in a row: when the
  // manager is created, and when it gets notified later on.
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  PropagateDevicePolicy();
  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/2);
  ExpectUploadEnrollmentId(/*times=*/2);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, EnrollmentIdNotNeeded) {
  SetUpDevicePolicy(/*enrollment_id_needed=*/false);
  SetUpEnrollmentIdUploadManager();
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  SetUpEnrollmentIdUploadManager();
  ExpectUploadEnrollmentCertificate(CertificateStatus::kInvalidClient,
                                    /*times=*/1);
  PropagateDevicePolicy();
  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, DBusFailureRetry) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit - 1);

  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/1);

  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  PropagateDevicePolicy();
  SetUpEnrollmentIdUploadManager();

  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, DBusFailureRetryUntilLimit) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit);

  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/0);

  SetUpDevicePolicy(/*enrollment_id_needed=*/true);
  PropagateDevicePolicy();
  SetUpEnrollmentIdUploadManager();

  RunUntilIdle();
}

TEST_F(EnrollmentIdUploadManagerTest, ObtainAndUploadSendsCertificate) {
  ExpectUploadEnrollmentCertificate(CertificateStatus::kSuccess, /*times=*/1);
  SetUpEnrollmentIdUploadManager();
  TestObtainAndUploadEnrollmentId(/*expect_success=*/true);
}

TEST_F(EnrollmentIdUploadManagerTest, ObtainAndUploadFailsToUploadCertificate) {
  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToUpload,
                                    /*times=*/1);
  SetUpEnrollmentIdUploadManager();
  TestObtainAndUploadEnrollmentId(/*expect_success=*/false);
}

TEST_F(EnrollmentIdUploadManagerTest, ObtainAndUploadSendsEnrollmentId) {
  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/1);
  SetUpEnrollmentIdUploadManager();
  TestObtainAndUploadEnrollmentId(/*expect_success=*/true);
}

TEST_F(EnrollmentIdUploadManagerTest, ObtainAndUploadUnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  SetUpEnrollmentIdUploadManager();
  ExpectUploadEnrollmentCertificate(CertificateStatus::kInvalidClient,
                                    /*times=*/1);
  TestObtainAndUploadEnrollmentId(/*expect_success=*/false);
}

TEST_F(EnrollmentIdUploadManagerTest, ObtainAndUploadDBusFailureRetry) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit - 1);

  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/1);

  SetUpEnrollmentIdUploadManager();
  TestObtainAndUploadEnrollmentId(/*expect_success=*/true);
}

TEST_F(EnrollmentIdUploadManagerTest,
       ObtainAndUploadDBusFailureRetryUntilLimit) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit);

  ExpectUploadEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                    /*times=*/1);
  ExpectUploadEnrollmentId(/*times=*/0);

  SetUpEnrollmentIdUploadManager();
  TestObtainAndUploadEnrollmentId(/*expect_success=*/false);
}

}  // namespace attestation
}  // namespace ash
