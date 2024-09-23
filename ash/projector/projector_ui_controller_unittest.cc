// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/test/mock_projector_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr char kProjectorCreationFlowErrorHistogramName[] =
    "Ash.Projector.CreationFlowError.ClamshellMode";
}  // namespace

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
  ProjectorUiControllerTest() = default;

  ProjectorUiControllerTest(const ProjectorUiControllerTest&) = delete;
  ProjectorUiControllerTest& operator=(const ProjectorUiControllerTest&) =
      delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto* projector_controller = Shell::Get()->projector_controller();
    projector_controller->SetClient(&projector_client_);
    controller_ = projector_controller->ui_controller();
  }

 protected:
  raw_ptr<ProjectorUiController, DanglingUntriaged> controller_;
  MockProjectorClient projector_client_;
};

TEST_F(ProjectorUiControllerTest, ShowFailureNotification) {
  base::HistogramTester histogram_tester;

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
      IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT);

  EXPECT_CALL(
      mock_message_center_observer,
      OnNotificationRemoved(/*notification_id=*/"projector_error_notification",
                            /*by_user=*/false));

  ProjectorUiController::ShowFailureNotification(
      IDS_ASH_PROJECTOR_FAILURE_MESSAGE_TRANSCRIPTION);

  const message_center::NotificationList::Notifications& notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ((*notifications.begin())->id(), "projector_error_notification");
  EXPECT_EQ((*notifications.begin())->message(),
            l10n_util::GetStringUTF16(
                IDS_ASH_PROJECTOR_FAILURE_MESSAGE_TRANSCRIPTION));

  histogram_tester.ExpectBucketCount(kProjectorCreationFlowErrorHistogramName,
                                     ProjectorCreationFlowError::kSaveError,
                                     /*count=*/1);
  histogram_tester.ExpectBucketCount(
      kProjectorCreationFlowErrorHistogramName,
      ProjectorCreationFlowError::kTranscriptionError,
      /*count=*/1);
  histogram_tester.ExpectTotalCount(kProjectorCreationFlowErrorHistogramName,
                                    /*count=*/2);

  message_center::MessageCenter::Get()->RemoveObserver(
      &mock_message_center_observer);
}

TEST_F(ProjectorUiControllerTest, ShowFailureNotificationWithTitle) {
  base::HistogramTester histogram_tester;

  MockMessageCenterObserver mock_message_center_observer;
  message_center::MessageCenter::Get()->AddObserver(
      &mock_message_center_observer);

  EXPECT_CALL(
      mock_message_center_observer,
      OnNotificationAdded(/*notification_id=*/"projector_error_notification"))
      .Times(1);
  EXPECT_CALL(mock_message_center_observer,
              OnNotificationDisplayed(
                  /*notification_id=*/"projector_error_notification",
                  message_center::DisplaySource::DISPLAY_SOURCE_POPUP));

  ProjectorUiController::ShowFailureNotification(
      IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TEXT,
      IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TITLE);

  const message_center::NotificationList::Notifications& notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ((*notifications.begin())->id(), "projector_error_notification");
  EXPECT_EQ(
      (*notifications.begin())->message(),
      l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TEXT));
  EXPECT_EQ(
      (*notifications.begin())->title(),
      l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_ABORT_BY_AUDIO_POLICY_TITLE));

  histogram_tester.ExpectBucketCount(
      kProjectorCreationFlowErrorHistogramName,
      ProjectorCreationFlowError::kSessionAbortedByAudioPolicyDisabled,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(kProjectorCreationFlowErrorHistogramName,
                                    /*count=*/1);

  message_center::MessageCenter::Get()->RemoveObserver(
      &mock_message_center_observer);
}

TEST_F(ProjectorUiControllerTest, ShowSaveFailureNotification) {
  base::HistogramTester histogram_tester;

  MockMessageCenterObserver mock_message_center_observer;
  message_center::MessageCenter::Get()->AddObserver(
      &mock_message_center_observer);

  EXPECT_CALL(mock_message_center_observer,
              OnNotificationAdded(
                  /*notification_id=*/"projector_save_error_notification"))
      .Times(2);
  EXPECT_CALL(mock_message_center_observer,
              OnNotificationDisplayed(
                  /*notification_id=*/"projector_save_error_notification",
                  message_center::DisplaySource::DISPLAY_SOURCE_POPUP));

  ProjectorUiController::ShowSaveFailureNotification();

  EXPECT_CALL(mock_message_center_observer,
              OnNotificationRemoved(
                  /*notification_id=*/"projector_save_error_notification",
                  /*by_user=*/false));

  ProjectorUiController::ShowSaveFailureNotification();

  const message_center::NotificationList::Notifications& notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ((*notifications.begin())->id(),
            "projector_save_error_notification");
  EXPECT_EQ((*notifications.begin())->message(),
            l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_SAVE_FAILURE_TEXT));

  histogram_tester.ExpectUniqueSample(kProjectorCreationFlowErrorHistogramName,
                                      ProjectorCreationFlowError::kSaveError,
                                      /*count=*/2);

  message_center::MessageCenter::Get()->RemoveObserver(
      &mock_message_center_observer);
}

}  // namespace ash
