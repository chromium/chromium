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

MediaCaptureState GetRelevantCaptureState(Type type) {
  switch (type) {
    case Type::kCamera:
      return MediaCaptureState::kVideo;
    case Type::kMic:
      return MediaCaptureState::kAudio;
  }
}

MediaCaptureState GetIrrelevantCaptureState(Type type) {
  switch (type) {
    case Type::kCamera:
      return MediaCaptureState::kAudio;
    case Type::kMic:
      return MediaCaptureState::kVideo;
  }
}

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

TEST_P(CameraMicTrayItemViewTest, OnVmMediaCaptureChanged) {
  Type type = GetParam();
  MediaCaptureState relevant = GetRelevantCaptureState(type);
  MediaCaptureState irrelevant = GetIrrelevantCaptureState(type);

  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaCaptureChanged(relevant);
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaCaptureChanged(irrelevant);
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaCaptureChanged(
      static_cast<MediaCaptureState>(static_cast<int>(relevant) |
                                     static_cast<int>(irrelevant)));
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  camera_mic_tray_item_view_->OnVmMediaCaptureChanged(MediaCaptureState::kNone);
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

TEST_P(CameraMicTrayItemViewTest, HideForNonPrimaryUser) {
  camera_mic_tray_item_view_->OnVmMediaCaptureChanged(
      GetRelevantCaptureState(GetParam()));
  EXPECT_TRUE(camera_mic_tray_item_view_->GetVisible());

  SimulateUserLogin("user2@test.com");
  EXPECT_FALSE(camera_mic_tray_item_view_->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CameraMicTrayItemViewTest,
                         testing::Values(Type::kCamera, Type::kMic));

}  // namespace ash
