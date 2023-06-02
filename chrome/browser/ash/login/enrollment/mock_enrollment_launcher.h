// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Mocks out EnrollmentLauncher.
class MockEnrollmentLauncher : public EnrollmentLauncher {
 public:
  MockEnrollmentLauncher();
  ~MockEnrollmentLauncher() override;

  EnrollmentStatusConsumer* status_consumer() const;

  MOCK_METHOD3(Setup,
               void(const policy::EnrollmentConfig& enrollment_config,
                    const std::string& enrolling_user_domain,
                    policy::LicenseType license_type));
  MOCK_METHOD1(EnrollUsingAuthCode, void(const std::string& auth_code));
  MOCK_METHOD1(EnrollUsingToken, void(const std::string& token));
  MOCK_METHOD0(EnrollUsingAttestation, void());
  MOCK_METHOD0(RestoreAfterRollback, void());
  MOCK_METHOD0(GetDeviceAttributeUpdatePermission, void());
  MOCK_METHOD2(UpdateDeviceAttributes,
               void(const std::string& asset_id, const std::string& location));
  MOCK_METHOD1(ClearAuth, void(base::OnceClosure callback));
  MOCK_METHOD(bool, InProgress, (), (const, override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_
