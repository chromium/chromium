// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_setup_test_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/policy/proto/device_management_backend.pb.h"

using testing::_;

namespace {

MATCHER(ConfigIsAttestation, "") {
  return arg.mode == policy::EnrollmentConfig::MODE_ATTESTATION;
}

MATCHER(ConfigIsOfflineDemo, "") {
  return arg.mode == policy::EnrollmentConfig::MODE_OFFLINE_DEMO;
}

}  // namespace

namespace chromeos {

namespace test {

void SetupMockDemoModeNoEnrollmentHelper() {
  std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
      std::make_unique<EnterpriseEnrollmentHelperMock>();
  EXPECT_CALL(*mock, Setup(_, _, _)).Times(0);
  EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
}

void SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult result) {
  std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
      std::make_unique<EnterpriseEnrollmentHelperMock>();
  auto* mock_ptr = mock.get();
  EXPECT_CALL(*mock, Setup(_, ConfigIsAttestation(), _));

  EXPECT_CALL(*mock, EnrollUsingAttestation())
      .WillRepeatedly(testing::Invoke([mock_ptr, result]() {
        switch (result) {
          case DemoModeSetupResult::SUCCESS:
            mock_ptr->status_consumer()->OnDeviceEnrolled();
            break;
          case DemoModeSetupResult::ERROR_POWERWASH_REQUIRED:
            mock_ptr->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForLockError(
                    chromeos::InstallAttributes::LOCK_ALREADY_LOCKED));
            break;
          case DemoModeSetupResult::ERROR_DEFAULT:
            mock_ptr->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForRegistrationError(
                    policy::DeviceManagementStatus::
                        DM_STATUS_TEMPORARY_UNAVAILABLE));
            break;
          default:
            NOTREACHED();
        }
      }));
  EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
}

void SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult result) {
  std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
      std::make_unique<EnterpriseEnrollmentHelperMock>();
  auto* mock_ptr = mock.get();
  EXPECT_CALL(*mock, Setup(_, ConfigIsOfflineDemo(), _));

  EXPECT_CALL(*mock, EnrollForOfflineDemo())
      .WillRepeatedly(testing::Invoke([mock_ptr, result]() {
        switch (result) {
          case DemoModeSetupResult::SUCCESS:
            mock_ptr->status_consumer()->OnDeviceEnrolled();
            break;
          case DemoModeSetupResult::ERROR_POWERWASH_REQUIRED:
            mock_ptr->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForLockError(
                    chromeos::InstallAttributes::LOCK_READBACK_ERROR));
            break;
          case DemoModeSetupResult::ERROR_DEFAULT:
            mock_ptr->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED));
            break;
          default:
            NOTREACHED();
        }
      }));
  EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
}

bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir) {
  base::ScopedAllowBlockingForTesting allow_io;
  if (!temp_dir->CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique tempdir";
    return false;
  }

  const base::FilePath policy_dir = temp_dir->GetPath().AppendASCII("policy");
  if (!base::CreateDirectory(policy_dir)) {
    LOG(ERROR) << "Failed to create policy directory";
    return false;
  }

  if (base::WriteFile(policy_dir.AppendASCII("device_policy"), "", 0) != 0) {
    LOG(ERROR) << "Failed to create device_policy file";
    return false;
  }

  // We use MockCloudPolicyStore for the device local account policy in the
  // tests, thus actual policy content can be empty. account_id is specified
  // since it is used by DemoSetupController to look up the store.
  std::string policy_blob;
  if (!account_id.empty()) {
    enterprise_management::PolicyData policy_data;
    policy_data.set_username(account_id);
    enterprise_management::PolicyFetchResponse policy;
    policy.set_policy_data(policy_data.SerializeAsString());
    policy_blob = policy.SerializeAsString();
  }
  if (base::WriteFile(policy_dir.AppendASCII("local_account_policy"),
                      policy_blob.data(), policy_blob.size()) !=
      static_cast<int>(policy_blob.size())) {
    LOG(ERROR) << "Failed to create local_account_policy file";
    return false;
  }
  return true;
}

}  // namespace test

}  // namespace chromeos
