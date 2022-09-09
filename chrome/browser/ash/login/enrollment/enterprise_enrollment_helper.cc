// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_impl.h"

namespace ash {

EnterpriseEnrollmentHelper*
    EnterpriseEnrollmentHelper::mock_enrollment_helper_ = nullptr;

EnterpriseEnrollmentHelper::~EnterpriseEnrollmentHelper() {}

// static
void EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(
    std::unique_ptr<EnterpriseEnrollmentHelper> mock) {
  if (mock_enrollment_helper_) {
    delete mock_enrollment_helper_;
  }
  mock_enrollment_helper_ = mock.release();
}

// static
std::unique_ptr<EnterpriseEnrollmentHelper> EnterpriseEnrollmentHelper::Create(
    EnrollmentStatusConsumer* status_consumer,
    policy::ActiveDirectoryJoinDelegate* ad_join_delegate,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain,
    policy::LicenseType license_type) {
  std::unique_ptr<EnterpriseEnrollmentHelper> result;

  // Create a mock instance.
  if (mock_enrollment_helper_) {
    result = base::WrapUnique(mock_enrollment_helper_);
    mock_enrollment_helper_ = nullptr;
  } else {
    result = std::make_unique<EnterpriseEnrollmentHelperImpl>();
  }
  result->set_status_consumer(status_consumer);
  result->Setup(ad_join_delegate, enrollment_config, enrolling_user_domain,
                license_type);
  return result;
}

EnterpriseEnrollmentHelper::EnterpriseEnrollmentHelper() {}

void EnterpriseEnrollmentHelper::set_status_consumer(
    EnrollmentStatusConsumer* status_consumer) {
  DCHECK(status_consumer);
  status_consumer_ = status_consumer;
}

}  // namespace ash
