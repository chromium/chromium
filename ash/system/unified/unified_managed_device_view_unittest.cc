// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_managed_device_view.h"

#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"

#include "ash/system/unified/unified_system_tray_view.h"

namespace ash {

class UnifiedManagedDeviceViewTest : public AshTestBase {
 public:
  UnifiedManagedDeviceViewTest() = default;
  ~UnifiedManagedDeviceViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    managed_device_view_ = std::make_unique<UnifiedManagedDeviceView>();
  }

  void TearDown() override {
    managed_device_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<UnifiedManagedDeviceView> managed_device_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedManagedDeviceViewTest);
};

TEST_F(UnifiedManagedDeviceViewTest, EnterpriseManagedDevice) {
  // By default, UnifiedManagedDeviceView is not shown.
  EXPECT_FALSE(managed_device_view_->GetVisible());

  // Simulate enterprise information becoming available.
  const bool active_directory = false;
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseDisplayDomain("example.com", active_directory);

  EXPECT_TRUE(managed_device_view_->GetVisible());
}

TEST_F(UnifiedManagedDeviceViewTest, ActiveDirectoryManagedDevice) {
  // Simulate active directory information becoming available.
  const std::string empty_domain;
  const bool active_directory = true;
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseDisplayDomain(empty_domain, active_directory);

  EXPECT_TRUE(managed_device_view_->GetVisible());
}

using UnifiedManagedDeviceViewNoSessionTest = NoSessionAshTestBase;

TEST_F(UnifiedManagedDeviceViewNoSessionTest, SupervisedUserDevice) {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Before login the UnifiedManagedDeviceView is invisible.
  std::unique_ptr<UnifiedManagedDeviceView> managed_device_view =
      std::make_unique<UnifiedManagedDeviceView>();
  EXPECT_FALSE(managed_device_view->GetVisible());
  managed_device_view.reset();

  // Simulate a supervised user logging in.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_SUPERVISED);
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  UserSession user_session = *session->GetUserSession(0);
  user_session.custodian_email = "parent@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Now the UnifiedManagedDeviceView is visible.
  managed_device_view = std::make_unique<UnifiedManagedDeviceView>();
  ASSERT_TRUE(managed_device_view->GetVisible());
}

}  // namespace ash
