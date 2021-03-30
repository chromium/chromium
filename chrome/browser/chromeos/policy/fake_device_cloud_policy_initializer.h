// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"

namespace policy {

class DeviceManagementService;

class FakeDeviceCloudPolicyInitializer : public DeviceCloudPolicyInitializer {
 public:
  FakeDeviceCloudPolicyInitializer();
  ~FakeDeviceCloudPolicyInitializer() override;

  void Init() override;
  void Shutdown() override;

  void PrepareEnrollment(
      DeviceManagementService* device_management_service,
      chromeos::ActiveDirectoryJoinDelegate* ad_join_delegate,
      const EnrollmentConfig& enrollment_config,
      DMAuth auth,
      EnrollmentCallback enrollment_callback) override;

  void StartEnrollment() override;

  bool was_start_enrollment_called() const {
    return was_start_enrollment_called_;
  }

  void set_enrollment_status(EnrollmentStatus status) {
    enrollment_status_ = status;
  }

 private:
  bool was_start_enrollment_called_;
  EnrollmentStatus enrollment_status_;
  EnrollmentCallback enrollment_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceCloudPolicyInitializer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_INITIALIZER_H_
