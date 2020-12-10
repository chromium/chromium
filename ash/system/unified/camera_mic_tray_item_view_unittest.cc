// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/camera_mic_tray_item_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/media_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
using Type = CameraMicTrayItemView::Type;
}  // namespace

class CameraMicTrayItemViewTest : public AshTestBase,
                                  public testing::WithParamInterface<Type> {
 public:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kVmCameraMicIndicatorsAndNotifications);
    AshTestBase::SetUp();

    camera_mic_tray_item_view_ =
        std::make_unique<CameraMicTrayItemView>(GetPrimaryShelf(), GetParam());

    // Relogin to make sure `OnActiveUserSessionChanged` is triggered.
    ClearLogin();
    SimulateUserLogin("user@test.com");
  }

  void TearDown() override {
    camera_mic_tray_item_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CameraMicTrayItemView> camera_mic_tray_item_view_;
};

TEST_P(CameraMicTrayItemViewTest, OnVmMediaNotificationChanged) {
  Type type = GetParam();
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(/*camera=*/true,
                                                           /*mic=*/false);
  EXPECT_EQ(camera_mic_tray_item_view_->GetVisible(), type == Type::kCamera);

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(/*camera=*/false,
                                                           /*mic=*/true);
  EXPECT_EQ(camera_mic_tray_item_view_->GetVisible(), type == Type::kMic);

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(/*camera=*/true,
                                                           /*mic=*/true);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(/*camera=*/false,
                                                           /*mic=*/false);
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

TEST_P(CameraMicTrayItemViewTest, HideForNonPrimaryUser) {
  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(/*camera=*/true,
                                                           /*mic=*/true);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  SimulateUserLogin("user2@test.com");
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CameraMicTrayItemViewTest,
                         testing::Values(Type::kCamera, Type::kMic));

}  // namespace ash
