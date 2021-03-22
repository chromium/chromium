// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/attestation/enrollment_policy_observer.h"
#include "chrome/browser/ash/attestation/mock_enrollment_certificate_uploader.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chromeos/dbus/attestation/fake_attestation_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using CertificateStatus =
    ash::attestation::EnrollmentCertificateUploader::Status;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

constexpr int kRetryLimit = 3;

void StatusCallbackSuccess(policy::CloudPolicyClient::StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace

class EnrollmentPolicyObserverTest : public DeviceSettingsTestBase {
 public:
  EnrollmentPolicyObserverTest() = default;
  ~EnrollmentPolicyObserverTest() override = default;

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
    EXPECT_FALSE(observer_->request_in_flight_);
    observer_.reset();
    DeviceSettingsTestBase::TearDown();
    AttestationClient::Shutdown();
  }

 protected:
  static constexpr char kEnrollmentId[] =
      "6fcc0ebddec3db9500cf82476d594f4d60db934c5b47fa6085c707b2a93e205b";

  void SetUpObserver() {
    observer_ = std::make_unique<EnrollmentPolicyObserver>(
        &policy_client_, device_settings_service_.get(),
        &certificate_uploader_);
    observer_->set_retry_limit(kRetryLimit);
    observer_->set_retry_delay(0);
  }

  void ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus status,
                                                   int times) {
    EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
        .Times(times)
        .WillRepeatedly(Invoke(
            [status](
                base::OnceCallback<void(CertificateStatus status)> callback) {
              std::move(callback).Run(status);
            }));
  }

  void ExpectUploadEnterpriseEnrollmentId(int times) {
    // Setting a mock behavior with 0 times causes warnings.
    if (times == 0) {
      return;
    }
    EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentId(enrollment_id_, _))
        .Times(times)
        .WillRepeatedly(WithArgs<1>(Invoke(StatusCallbackSuccess)));
  }

  void SetUpDevicePolicy(bool enrollment_id_needed) {
    device_policy_->policy_data().set_enrollment_id_needed(
        enrollment_id_needed);
  }

  void PropagateDevicePolicy() {
    ReloadDevicePolicy();
    ReloadDeviceSettings();
  }

  void Run() {
    base::RunLoop().RunUntilIdle();
  }

  StrictMock<policy::MockCloudPolicyClient> policy_client_;
  StrictMock<MockEnrollmentCertificateUploader> certificate_uploader_;
  std::unique_ptr<EnrollmentPolicyObserver> observer_;
  std::string enrollment_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentPolicyObserverTest);
};

constexpr char EnrollmentPolicyObserverTest::kEnrollmentId[];

TEST_F(EnrollmentPolicyObserverTest, UploadEnterpriseEnrollmentCertificate) {
  SetUpDevicePolicy(true);
  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kSuccess,
                                              /*times=*/1);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest,
       UploadEnterpriseEnrollmentCertificateFromExistingPolicy) {
  // This test will trigger the observer work twice in a row: when the
  // observer is created, and when it gets notified later on.
  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kSuccess,
                                              /*times=*/2);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, FailedToUploadEnrollmentCertificate) {
  SetUpDevicePolicy(true);
  ExpectUploadEnterpriseEnrollmentCertificate(
      CertificateStatus::kFailedToUpload, /*times=*/1);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, UploadEnterpriseEnrollmentId) {
  SetUpDevicePolicy(true);
  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                              /*times=*/1);
  ExpectUploadEnterpriseEnrollmentId(1);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest,
       UploadEnterpriseEnrollmentIdFromExistingPolicy) {
  // This test will trigger the observer work twice in a row: when the
  // observer is created, and when it gets notified later on.
  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                              /*times=*/2);
  ExpectUploadEnterpriseEnrollmentId(2);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, FeatureDisabled) {
  SetUpDevicePolicy(false);
  SetUpObserver();
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  SetUpDevicePolicy(true);
  SetUpObserver();
  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                              /*times=*/1);
  PropagateDevicePolicy();
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, DBusFailureRetry) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit - 1);

  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                              /*times=*/1);
  ExpectUploadEnterpriseEnrollmentId(1);

  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  SetUpObserver();

  Run();
}

TEST_F(EnrollmentPolicyObserverTest, DBusFailureRetryUntilLimit) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_enrollment_id_dbus_error_count(kRetryLimit);

  ExpectUploadEnterpriseEnrollmentCertificate(CertificateStatus::kFailedToFetch,
                                              /*times=*/1);
  ExpectUploadEnterpriseEnrollmentId(0);

  SetUpDevicePolicy(true);
  PropagateDevicePolicy();
  SetUpObserver();

  Run();
}

}  // namespace attestation
}  // namespace ash
