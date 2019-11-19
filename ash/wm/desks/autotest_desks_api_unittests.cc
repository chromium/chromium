// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_desks_api.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AutotestDesksApiTest : public AshTestBase {
 public:
  AutotestDesksApiTest() = default;
  ~AutotestDesksApiTest() override = default;

  AutotestDesksApiTest(const AutotestDesksApiTest& other) = delete;
  AutotestDesksApiTest& operator=(const AutotestDesksApiTest& rhs) = delete;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutotestDesksApiTest, CreateNewDesk) {
  AutotestDesksApi test_api;
  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_FALSE(test_api.CreateNewDesk());
}

TEST_F(AutotestDesksApiTest, ActivateDeskAtIndex) {
  AutotestDesksApi test_api;
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(-1, base::DoNothing()));
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(4, base::DoNothing()));

  // Activating already active desk does nothing.
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(0, base::DoNothing()));

  // Create 4 desks and switch between all of them.
  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_EQ(4u, controller->desks().size());
  constexpr int kIndices[] = {1, 2, 3, 0};
  for (const int index : kIndices) {
    base::RunLoop run_loop;
    EXPECT_TRUE(test_api.ActivateDeskAtIndex(index, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(controller->active_desk(), controller->desks()[index].get());
  }
}

TEST_F(AutotestDesksApiTest, RemoveActiveDesk) {
  AutotestDesksApi test_api;
  EXPECT_FALSE(test_api.RemoveActiveDesk(base::DoNothing()));

  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_EQ(4u, controller->desks().size());
  EXPECT_EQ(controller->active_desk(), controller->desks()[0].get());

  // List of desks that will be activated after each time we invoke
  // RemoveActiveDesk().
  const Desk* desks_after_removal[] = {
      controller->desks()[1].get(),
      controller->desks()[2].get(),
      controller->desks()[3].get(),
  };

  for (const Desk* desk : desks_after_removal) {
    base::RunLoop run_loop;
    EXPECT_TRUE(test_api.RemoveActiveDesk(run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(controller->active_desk(), desk);
  }

  // Can no longer remove desks.
  EXPECT_FALSE(test_api.RemoveActiveDesk(base::DoNothing()));
}

}  // namespace ash
