// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/message_center/message_center.h"

namespace ash {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaSessionInfo;

namespace {

bool IsMediaNotificationShown() {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
      "media-session");
}

int GetVisibleNotificationCount() {
  return message_center::MessageCenter::Get()->GetVisibleNotifications().size();
}

int GetPopupNotificationCount() {
  return message_center::MessageCenter::Get()->GetPopupNotifications().size();
}

}  // namespace

class MediaNotificationControllerTest : public AshTestBase {
 public:
  MediaNotificationControllerTest() = default;
  ~MediaNotificationControllerTest() override = default;

  // AshTestBase
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaSessionNotification);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationControllerTest);
};

TEST_F(MediaNotificationControllerTest, OnFocusGainedLost) {
  EXPECT_FALSE(IsMediaNotificationShown());
  EXPECT_EQ(0, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnFocusGained(
      MediaSessionInfo::New(), AudioFocusType::kGain);
  EXPECT_TRUE(IsMediaNotificationShown());
  EXPECT_EQ(1, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnFocusGained(
      MediaSessionInfo::New(), AudioFocusType::kGain);
  EXPECT_TRUE(IsMediaNotificationShown());
  EXPECT_EQ(1, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      MediaSessionInfo::New());
  EXPECT_FALSE(IsMediaNotificationShown());
  EXPECT_EQ(0, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());
}

TEST_F(MediaNotificationControllerTest, OnFocusLost_Noop) {
  EXPECT_FALSE(IsMediaNotificationShown());

  Shell::Get()->media_notification_controller()->OnFocusLost(
      MediaSessionInfo::New());
  EXPECT_FALSE(IsMediaNotificationShown());
}

}  // namespace ash
