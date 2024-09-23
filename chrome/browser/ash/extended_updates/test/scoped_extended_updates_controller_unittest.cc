// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/test/scoped_extended_updates_controller.h"

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/ash/extended_updates/test/mock_extended_updates_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
ExtendedUpdatesController* GetController() {
  return ExtendedUpdatesController::Get();
}
}  // namespace

// Verify that scoped controller can replace and restore global controller
// instance correctly.
TEST(ScopedExtendedUpdatesControllerTest, ScopedReplaceInstanceWithMock) {
  ExtendedUpdatesController* old_controller = GetController();
  ::testing::StrictMock<MockExtendedUpdatesController> mock_controller;
  ASSERT_NE(&mock_controller, old_controller);

  Profile* profile = nullptr;
  ExtendedUpdatesController::Params params{
      .eol_passed = false,
      .extended_date_passed = true,
      .opt_in_required = false,
  };
  EXPECT_CALL(mock_controller, IsOptInEligible(profile, params))
      .WillOnce(::testing::Return(true));

  // Before mocking.
  EXPECT_FALSE(GetController()->IsOptInEligible(profile, params));

  {
    // Replace instance with mock.
    ScopedExtendedUpdatesController scoped_controller(&mock_controller);
    EXPECT_EQ(GetController(), &mock_controller);
    EXPECT_TRUE(GetController()->IsOptInEligible(profile, params));
  }

  // After instance is restored.
  EXPECT_EQ(GetController(), old_controller);
  EXPECT_FALSE(GetController()->IsOptInEligible(profile, params));
}

}  // namespace ash
