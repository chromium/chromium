// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/federated_service_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::federated {

// Fake federated service connection is prepared by AshTestHelper.
class FederatedServiceControllerImplTestBase : public NoSessionAshTestBase {
 public:
  FederatedServiceControllerImplTestBase()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFederatedService,
                              features::kFederatedServiceScheduleTasks},
        /*disabled_features=*/{});
  }

  FederatedServiceControllerImplTestBase(
      const FederatedServiceControllerImplTestBase&) = delete;
  FederatedServiceControllerImplTestBase& operator=(
      const FederatedServiceControllerImplTestBase&) = delete;

  ~FederatedServiceControllerImplTestBase() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = Shell::Get()->federated_service_controller();
  }

 protected:
  raw_ptr<FederatedServiceControllerImpl, DanglingUntriaged> controller_ =
      nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FederatedServiceControllerImplTestBase, NormalUserLogin) {
  SimulateUserLogin("user@gmail.com");
  EXPECT_TRUE(controller_->IsServiceAvailable());

  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(controller_->IsServiceAvailable());

  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(controller_->IsServiceAvailable());

  // Signing out means ash-chrome exit and NoSessionAshTestBase does not
  // simulate exactly what happens in production.
  // On a real ChromeOS device when the user signs out, ash-chrome exits and
  // re-starts, hence a brand-new federated_service_controller. On ChromeOS side
  // the federated_service daemon also quits because of broken mojo connection,
  // and waits to be re-launched by the new federated_service_controller.
  GetSessionControllerClient()->RequestSignOut();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller_->IsServiceAvailable());
}

TEST_F(FederatedServiceControllerImplTestBase, ChildUserLogin) {
  SimulateUserLogin("user@gmail.com", user_manager::UserType::kChild);
  EXPECT_TRUE(controller_->IsServiceAvailable());
}

TEST_F(FederatedServiceControllerImplTestBase, InvalidLoginStatusAndUserType) {
  SimulateGuestLogin();
  EXPECT_FALSE(controller_->IsServiceAvailable());
  ClearLogin();

  SimulateKioskMode(user_manager::UserType::kWebKioskApp);

  EXPECT_FALSE(controller_->IsServiceAvailable());
  ClearLogin();

  SimulateKioskMode(user_manager::UserType::kKioskApp);

  EXPECT_FALSE(controller_->IsServiceAvailable());
  ClearLogin();

  EXPECT_FALSE(controller_->IsServiceAvailable());
}

}  // namespace ash::federated
