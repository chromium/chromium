// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"

namespace policy {

FakeDeviceCloudPolicyInitializer::FakeDeviceCloudPolicyInitializer()
    : DeviceCloudPolicyInitializer(
          nullptr,  // local_state
          nullptr,  // enterprise_service
          // background_task_runner
          scoped_refptr<base::SequencedTaskRunner>(nullptr),
          nullptr,  // install_attributes
          nullptr,  // state_keys_broker
          nullptr,  // device_store
          nullptr,  // policy_manager
          std::make_unique<chromeos::attestation::MockAttestationFlow>(),
          nullptr),  // statistics_provider
      was_start_enrollment_called_(false),
      enrollment_status_(
          EnrollmentStatus::ForStatus(EnrollmentStatus::SUCCESS)) {}

FakeDeviceCloudPolicyInitializer::~FakeDeviceCloudPolicyInitializer() = default;

void FakeDeviceCloudPolicyInitializer::Init() {
}

void FakeDeviceCloudPolicyInitializer::Shutdown() {
}

void FakeDeviceCloudPolicyInitializer::PrepareEnrollment(
    DeviceManagementService* device_management_service,
    chromeos::ActiveDirectoryJoinDelegate* ad_join_delegate,
    const EnrollmentConfig& enrollment_config,
    DMAuth auth,
    EnrollmentCallback enrollment_callback) {
  enrollment_callback_ = std::move(enrollment_callback);
}

void FakeDeviceCloudPolicyInitializer::StartEnrollment() {
  if (enrollment_callback_)
    std::move(enrollment_callback_).Run(enrollment_status_);
  was_start_enrollment_called_ = true;
}

}  // namespace policy
