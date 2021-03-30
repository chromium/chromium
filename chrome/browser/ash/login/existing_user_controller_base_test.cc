// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/existing_user_controller_base_test.h"

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace chromeos {

namespace {

class FakeUserManagerWithLocalState : public chromeos::FakeChromeUserManager {
 public:
  explicit FakeUserManagerWithLocalState(MockUserManager* mock_user_manager)
      : mock_user_manager_(mock_user_manager),
        test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

  MockUserManager* mock_user_manager() { return mock_user_manager_; }

 private:
  // Unowned pointer.
  MockUserManager* const mock_user_manager_;
  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;
};

}  // namespace

ExistingUserControllerBaseTest::ExistingUserControllerBaseTest()
    : mock_user_manager_(new MockUserManager()) {
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<FakeUserManagerWithLocalState>(mock_user_manager_));
}

ExistingUserControllerBaseTest::~ExistingUserControllerBaseTest() = default;

MockUserManager* ExistingUserControllerBaseTest::mock_user_manager() {
  return mock_user_manager_;
}

}  // namespace chromeos
