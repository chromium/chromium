// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/test/mock_projector_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "projector_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_list.h"

namespace ash {

class MockMessageCenterObserver : public message_center::MessageCenterObserver {
 public:
  MockMessageCenterObserver() = default;
  MockMessageCenterObserver(const MockMessageCenterObserver&) = delete;
  MockMessageCenterObserver& operator=(const MockMessageCenterObserver&) =
      delete;
  ~MockMessageCenterObserver() override = default;

  MOCK_METHOD1(OnNotificationAdded, void(const std::string& notification_id));
  MOCK_METHOD2(OnNotificationRemoved,
               void(const std::string& notification_id, bool by_user));
  MOCK_METHOD2(OnNotificationDisplayed,
               void(const std::string& notification_id,
                    const message_center::DisplaySource source));
};

class ProjectorUiControllerTest : public AshTestBase {
 public:
  ProjectorUiControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kProjector, features::kProjectorAnnotator}, {});
  }

  ProjectorUiControllerTest(const ProjectorUiControllerTest&) = delete;
  ProjectorUiControllerTest& operator=(const ProjectorUiControllerTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = Shell::Get()->projector_controller()->ui_controller();
  }

 protected:
  ProjectorUiController* controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorUiControllerTest, ShowAndCloseToolbar) {
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  controller_->ShowToolbar();
  EXPECT_TRUE(projector_annotation_tray->visible_preferred());
  controller_->CloseToolbar();
  EXPECT_FALSE(projector_annotation_tray->visible_preferred());
}

// Verifies that toggling on the laser pointer on Projector tools propagates to
// the laser pointer controller.
TEST_F(ProjectorUiControllerTest, EnablingDisablingLaserPointer) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  // Enable marker.
  controller_->OnMarkerPressed();
  EXPECT_TRUE(controller_->is_annotator_enabled());
  EXPECT_FALSE(laser_pointer_controller->is_enabled());

  controller_->OnLaserPointerPressed();
  EXPECT_TRUE(laser_pointer_controller->is_enabled());
  EXPECT_FALSE(controller_->is_annotator_enabled());

  controller_->OnMarkerPressed();
  EXPECT_TRUE(controller_->is_annotator_enabled());
  EXPECT_FALSE(laser_pointer_controller->is_enabled());

  auto* laser_pointer_controller_ = Shell::Get()->laser_pointer_controller();
  laser_pointer_controller_->SetEnabled(true);
  EXPECT_TRUE(laser_pointer_controller->is_enabled());
  EXPECT_FALSE(controller_->is_annotator_enabled());
}

TEST_F(ProjectorUiControllerTest, ShowFailureNotification) {
  MockMessageCenterObserver mock_message_center_observer;
  message_center::MessageCenter::Get()->AddObserver(
      &mock_message_center_observer);

  EXPECT_CALL(
      mock_message_center_observer,
      OnNotificationAdded(/*notification_id=*/"projector_error_notification"))
      .Times(2);
  EXPECT_CALL(mock_message_center_observer,
              OnNotificationDisplayed(
                  /*notification_id=*/"projector_error_notification",
                  message_center::DisplaySource::DISPLAY_SOURCE_POPUP));

  ProjectorUiController::ShowFailureNotification(
      IDS_ASH_PROJECTOR_FAILURE_MESSAGE_SAVE_SCREENCAST);

  EXPECT_CALL(
      mock_message_center_observer,
      OnNotificationRemoved(/*notification_id=*/"projector_error_notification",
                            /*by_user=*/false));

  ProjectorUiController::ShowFailureNotification(
      IDS_ASH_PROJECTOR_FAILURE_MESSAGE_DRIVEFS);

  const message_center::NotificationList::Notifications& notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ((*notifications.begin())->id(), "projector_error_notification");
  EXPECT_EQ(
      (*notifications.begin())->message(),
      l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_FAILURE_MESSAGE_DRIVEFS));
}

}  // namespace ash
