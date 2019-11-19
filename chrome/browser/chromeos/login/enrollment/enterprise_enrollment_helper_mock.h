// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_MOCK_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_MOCK_H_

#include <string>

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

// Mocks out EnterpriseEnrollmentHelper.
class EnterpriseEnrollmentHelperMock : public EnterpriseEnrollmentHelper {
 public:
  EnterpriseEnrollmentHelperMock();
  ~EnterpriseEnrollmentHelperMock() override;

  EnrollmentStatusConsumer* status_consumer() const;

  MOCK_METHOD3(Setup,
               void(ActiveDirectoryJoinDelegate* ad_join_delegate,
                    const policy::EnrollmentConfig& enrollment_config,
                    const std::string& enrolling_user_domain));
  MOCK_METHOD1(EnrollUsingAuthCode, void(const std::string& auth_code));
  MOCK_METHOD1(EnrollUsingToken, void(const std::string& token));
  MOCK_METHOD1(EnrollUsingEnrollmentToken, void(const std::string& token));
  MOCK_METHOD0(EnrollUsingAttestation, void());
  MOCK_METHOD0(EnrollForOfflineDemo, void());
  MOCK_METHOD0(RestoreAfterRollback, void());
  MOCK_METHOD1(UseLicenseType, void(policy::LicenseType type));
  MOCK_METHOD0(GetDeviceAttributeUpdatePermission, void());
  MOCK_METHOD2(UpdateDeviceAttributes,
               void(const std::string& asset_id, const std::string& location));
  MOCK_METHOD1(ClearAuth, void(base::OnceClosure callback));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_MOCK_H_
