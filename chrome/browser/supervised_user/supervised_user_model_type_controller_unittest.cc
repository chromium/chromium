// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_sync_model_type_controller.h"

#include "base/functional/callback_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/data_type_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataTypeController;

class SupervisedUserSyncModelTypeControllerTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SupervisedUserSyncModelTypeControllerTest,
       SupervisedUserMeetsPreconditions) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<Profile> child_profile = builder.Build();
  ASSERT_TRUE(child_profile->IsChild());

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, child_profile.get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSyncModelTypeControllerTest,
       NonSupervisedUserDoesNotMeetPreconditions) {
  TestingProfile::Builder builder;
  std::unique_ptr<Profile> non_child_profile = builder.Build();
  ASSERT_FALSE(non_child_profile->IsChild());

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, non_child_profile.get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndClearData,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSyncModelTypeControllerTest, HasTransportModeDelegate) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<Profile> child_profile = builder.Build();
  ASSERT_TRUE(child_profile->IsChild());

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, child_profile.get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_TRUE(
      controller.GetDelegateForTesting(syncer::SyncMode::kTransportOnly));
}
