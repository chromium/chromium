// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_ASH_TEST_BASE_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_ASH_TEST_BASE_H_

#include "ash/test/ash_test_base.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockUserEducationDelegate;

// Base class for tests of user education in Ash. Note that this class:
// * Installs a `MockUserEducationDelegate` during `SetUp()`.
// * Does NOT add user sessions during `SetUp()`.
class UserEducationAshTestBase : public NoSessionAshTestBase {
 protected:
  // Returns the mocked delegate which facilitates communication between Ash and
  // user education services in the browser.
  testing::NiceMock<MockUserEducationDelegate>* user_education_delegate() {
    return user_education_delegate_;
  }

 private:
  // NoSessionAshTestBase:
  void SetUp() override;

  // The mocked delegate which facilitates communication between Ash and user
  // education services in the browser. Created during `SetUp()`.
  testing::NiceMock<MockUserEducationDelegate>* user_education_delegate_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_ASH_TEST_BASE_H_
