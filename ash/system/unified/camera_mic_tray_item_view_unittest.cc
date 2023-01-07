// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/camera_mic_tray_item_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/media_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {
using Type = CameraMicTrayItemView::Type;
}  // namespace

class BaseCameraMicTrayItemViewTest : public AshTestBase {
 public:
  void SetUpWithType(Type type) {
    AshTestBase::SetUp();

    camera_mic_tray_item_view_ =
        std::make_unique<CameraMicTrayItemView>(GetPrimaryShelf(), type);

    // Relogin to make sure `OnActiveUserSessionChanged` is triggered.
    ClearLogin();
    SimulateUserLogin("user@test.com");
  }

  void TearDown() override {
    camera_mic_tray_item_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<CameraMicTrayItemView> camera_mic_tray_item_view_;
};

class CameraMicTrayItemViewTest : public BaseCameraMicTrayItemViewTest,
                                  public testing::WithParamInterface<Type> {
 public:
  // AshTestBase:
  void SetUp() override { SetUpWithType(GetParam()); }
};

TEST_P(CameraMicTrayItemViewTest, GetVisible) {
  Type type = GetParam();
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/false,
      /*camera_and_mic=*/false);
  EXPECT_EQ(camera_mic_tray_item_view_->GetVisible(), type == Type::kCamera);

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/false,
      /*mic=*/false,
      /*camera_and_mic=*/true);
  EXPECT_EQ(camera_mic_tray_item_view_->GetVisible(), type == Type::kCamera);

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/false,
      /*mic=*/true,
      /*camera_and_mic=*/false);
  EXPECT_EQ(camera_mic_tray_item_view_->GetVisible(), type == Type::kMic);

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/true,
      /*camera_and_mic=*/false);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/true,
      /*camera_and_mic=*/true);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/false,
      /*mic=*/false,
      /*camera_and_mic=*/false);
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

TEST_P(CameraMicTrayItemViewTest, HideForNonPrimaryUser) {
  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/true,
      /*camera_and_mic=*/true);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  SimulateUserLogin("user2@test.com");
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CameraMicTrayItemViewTest,
                         testing::Values(Type::kCamera, Type::kMic));

// For testing that the camera tray item switch the message depending on whether
// the "camera and mic" notification is active.
class CameraMicTrayItemViewMessageTest : public BaseCameraMicTrayItemViewTest {
  // AshTestBase:
  void SetUp() override { SetUpWithType(Type::kCamera); }
};

TEST_F(CameraMicTrayItemViewMessageTest, Message) {
  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/false,
      /*camera_and_mic=*/false);
  EXPECT_EQ(camera_mic_tray_item_view_->GetAccessibleNameString(),
            l10n_util::GetStringUTF16(IDS_ASH_CAMERA_MIC_VM_USING_CAMERA));

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/false,
      /*mic=*/false,
      /*camera_and_mic=*/true);
  EXPECT_EQ(
      camera_mic_tray_item_view_->GetAccessibleNameString(),
      l10n_util::GetStringUTF16(IDS_ASH_CAMERA_MIC_VM_USING_CAMERA_AND_MIC));

  camera_mic_tray_item_view_->OnVmMediaNotificationChanged(
      /*camera=*/true,
      /*mic=*/false,
      /*camera_and_mic=*/true);
  EXPECT_EQ(
      camera_mic_tray_item_view_->GetAccessibleNameString(),
      l10n_util::GetStringUTF16(IDS_ASH_CAMERA_MIC_VM_USING_CAMERA_AND_MIC));
}

}  // namespace ash
