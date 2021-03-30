// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_mock.h"

#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_impl.h"

namespace chromeos {

EnterpriseEnrollmentHelperMock::EnterpriseEnrollmentHelperMock() {}

EnterpriseEnrollmentHelperMock::~EnterpriseEnrollmentHelperMock() {}

EnterpriseEnrollmentHelper::EnrollmentStatusConsumer*
EnterpriseEnrollmentHelperMock::status_consumer() const {
  return EnterpriseEnrollmentHelper::status_consumer();
}

}  // namespace chromeos
