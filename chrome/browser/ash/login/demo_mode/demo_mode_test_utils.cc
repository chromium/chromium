// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

using ::testing::_;

MATCHER(ConfigIsAttestation, "") {
  return arg.mode == policy::EnrollmentConfig::MODE_ATTESTATION;
}

}  // namespace

namespace test {

void SetupDemoModeNoEnrollment(MockEnrollmentLauncher* mock) {
  EXPECT_CALL(*mock, Setup(_, _)).Times(0);
}

void SetupDemoModeOnlineEnrollment(MockEnrollmentLauncher* mock,
                                   DemoModeSetupResult result) {
  EXPECT_CALL(*mock, Setup(ConfigIsAttestation(), _));

  EXPECT_CALL(*mock, EnrollUsingAttestation())
      .WillRepeatedly(testing::Invoke([mock, result]() {
        switch (result) {
          case DemoModeSetupResult::SUCCESS:
            mock->status_consumer()->OnDeviceEnrolled();
            break;
          case DemoModeSetupResult::ERROR_POWERWASH_REQUIRED:
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForLockError(
                    InstallAttributes::LOCK_ALREADY_LOCKED));
            break;
          case DemoModeSetupResult::ERROR_DEFAULT:
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForRegistrationError(
                    policy::DeviceManagementStatus::
                        DM_STATUS_TEMPORARY_UNAVAILABLE));
            break;
          default:
            NOTREACHED_IN_MIGRATION();
        }
      }));
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

  if (!base::WriteFile(policy_dir.AppendASCII("device_policy"),
                       std::string_view())) {
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
  if (!base::WriteFile(policy_dir.AppendASCII("local_account_policy"),
                       policy_blob)) {
    LOG(ERROR) << "Failed to create local_account_policy file";
    return false;
  }
  return true;
}

// Helper InstallAttributes::LockResultCallback implementation.
void OnEnterpriseDeviceLock(base::OnceClosure runner_quit_task,
                            InstallAttributes::LockResult in_locked) {
  LOG(INFO) << "Enterprise lock  = " << in_locked;
  std::move(runner_quit_task).Run();
}

void LockDemoDeviceInstallAttributes() {
  base::RunLoop run_loop;
  LOG(INFO) << "Locking demo mode install attributes";
  InstallAttributes::Get()->LockDevice(
      policy::DEVICE_MODE_DEMO, "domain.com",
      std::string(),  // realm
      "device-id",
      base::BindOnce(&OnEnterpriseDeviceLock, run_loop.QuitClosure()));
  run_loop.Run();
}

void AssertDemoDimensionsEqual(
    const enterprise_management::DemoModeDimensions& expected,
    const enterprise_management::DemoModeDimensions& actual) {
  EXPECT_EQ(actual.country(), expected.country());
  EXPECT_EQ(actual.retailer_name(), expected.retailer_name());
  EXPECT_EQ(actual.store_number(), expected.store_number());
  // Compare customization facets ignoring order
  EXPECT_THAT(
      actual.customization_facets(),
      testing::UnorderedElementsAreArray(expected.customization_facets()));
}

}  // namespace test
}  // namespace ash
