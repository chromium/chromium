// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/test/mock_projector_client.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "projector_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_list.h"

namespace ash {

namespace {

constexpr char kProjectorCreationFlowErrorHistogramName[] =
    "Ash.Projector.CreationFlowError.ClamshellMode";
constexpr char kProjectorMarkerColorHistogramName[] =
    "Ash.Projector.MarkerColor.ClamshellMode";
constexpr char kProjectorToolbarHistogramName[] =
    "Ash.Projector.Toolbar.ClamshellMode";

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

    auto* projector_controller = Shell::Get()->projector_controller();
    projector_controller->SetClient(&projector_client_);
    controller_ = projector_controller->ui_controller();
  }

 protected:
  ProjectorUiController* controller_;
  MockProjectorClient projector_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorUiControllerTest, ShowAndHideTray) {
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(projector_annotation_tray->visible_preferred());
  controller_->HideAnnotationTray();
  EXPECT_FALSE(projector_annotation_tray->visible_preferred());
}

TEST_F(ProjectorUiControllerTest, ShowAndHideTrayMultipleDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  auto* primary_display_tray = Shell::GetPrimaryRootWindowController()
                                   ->GetStatusAreaWidget()
                                   ->projector_annotation_tray();
  auto* external_display_tray = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->projector_annotation_tray();

  // Show tray on primary root window.
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());

  // Change the root window of the recorded window.
  controller_->OnRecordedWindowChangingRoot(roots[1]);
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_TRUE(external_display_tray->visible_preferred());

  controller_->HideAnnotationTray();
  EXPECT_FALSE(primary_display_tray->visible_preferred());
  EXPECT_FALSE(external_display_tray->visible_preferred());
}

TEST_F(ProjectorUiControllerTest, HideTrayWhenAnnotatorIsEnabled) {
  base::HistogramTester histogram_tester;

  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(projector_annotation_tray->visible_preferred());

  controller_->EnableAnnotatorTool();
  EXPECT_TRUE(controller_->is_annotator_enabled());

  controller_->HideAnnotationTray();
  EXPECT_FALSE(projector_annotation_tray->visible_preferred());
  EXPECT_FALSE(controller_->is_annotator_enabled());

  histogram_tester.ExpectUniqueSample(kProjectorToolbarHistogramName,
                                      ProjectorToolbar::kMarkerTool,
                                      /*count=*/1);
}

// Verifies that toggling on the marker on Projector tools enables the
// annotator.
TEST_F(ProjectorUiControllerTest, EnablingDisablingMarker) {
  base::HistogramTester histogram_tester;

  // Enable marker.
  controller_->EnableAnnotatorTool();
  EXPECT_TRUE(controller_->is_annotator_enabled());

  EXPECT_CALL(projector_client_, Clear());
  controller_->ResetTools();
  EXPECT_FALSE(controller_->is_annotator_enabled());

  histogram_tester.ExpectUniqueSample(kProjectorToolbarHistogramName,
                                      ProjectorToolbar::kMarkerTool,
                                      /*count=*/1);
}

TEST_F(ProjectorUiControllerTest, ToggleMarkerWithKeyboardShortcut) {
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  controller_->OnCanvasInitialized(true);

  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(controller_->is_annotator_enabled());

  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(controller_->is_annotator_enabled());
}

TEST_F(ProjectorUiControllerTest, SetAnnotatorTool) {
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  base::HistogramTester histogram_tester;
  AnnotatorTool tool;
  tool.color = kProjectorDefaultPenColor;
  EXPECT_CALL(projector_client_, SetTool(tool));

  // Assert that the initial pref is unset.
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(
      pref_service->GetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor),
      0u);

  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  controller_->OnCanvasInitialized(true);
  LeftClickOn(projector_annotation_tray);
  EXPECT_TRUE(controller_->is_annotator_enabled());

  controller_->HideAnnotationTray();
  EXPECT_FALSE(controller_->is_annotator_enabled());
  // Check that the last used color is stored in the pref.
  EXPECT_EQ(
      pref_service->GetUint64(prefs::kProjectorAnnotatorLastUsedMarkerColor),
      kProjectorDefaultPenColor);
  histogram_tester.ExpectUniqueSample(kProjectorMarkerColorHistogramName,
                                      ProjectorMarkerColor::kMagenta,
                                      /*count=*/1);
}

// Tests that right clicking the ProjectorAnnotationTray shows a bubble.
TEST_F(ProjectorUiControllerTest, RightClickShowsBubble) {
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  controller_->OnCanvasInitialized(true);

  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();

  // Right click the tray item, it should show a bubble.
  RightClickOn(projector_annotation_tray);
  EXPECT_TRUE(projector_annotation_tray->GetBubbleWidget());
}

// Tests that long pressing the ProjectorAnnotationTray shows a bubble.
TEST_F(ProjectorUiControllerTest, LongPressShowsBubble) {
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  controller_->OnCanvasInitialized(true);

  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();

  // Long press the tray item, it should show a bubble.
  gfx::Point location =
      projector_annotation_tray->GetBoundsInScreen().CenterPoint();

  // Temporarily reconfigure gestures so that the long press takes 2
  // milliseconds.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int old_long_press_time_in_ms = gesture_config->long_press_time_in_ms();
  const base::TimeDelta old_short_press_time =
      gesture_config->short_press_time();
  const int old_show_press_delay_in_ms =
      gesture_config->show_press_delay_in_ms();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_short_press_time(base::Milliseconds(1));
  gesture_config->set_show_press_delay_in_ms(1);

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(location);
  event_generator->PressTouch();

  // Hold the press down for 2 ms, to trigger a long press.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
  run_loop.Run();

  gesture_config->set_long_press_time_in_ms(old_long_press_time_in_ms);
  gesture_config->set_short_press_time(old_short_press_time);
  gesture_config->set_show_press_delay_in_ms(old_show_press_delay_in_ms);

  event_generator->ReleaseTouch();

  EXPECT_TRUE(projector_annotation_tray->GetBubbleWidget());
}

// Tests that tapping the ProjectorAnnotationTray enables annotation.
TEST_F(ProjectorUiControllerTest, TapEnabledAnnotation) {
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());
  controller_->OnCanvasInitialized(true);

  GestureTapOn(Shell::GetPrimaryRootWindowController()
                   ->GetStatusAreaWidget()
                   ->projector_annotation_tray());

  EXPECT_TRUE(controller_->is_annotator_enabled());
}

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
}

TEST_F(ProjectorUiControllerTest, OnCanvasInitialized) {
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  controller_->ShowAnnotationTray(Shell::GetPrimaryRootWindow());

  EXPECT_FALSE(projector_annotation_tray->GetEnabled());

  controller_->OnCanvasInitialized(/*success=*/true);
  EXPECT_TRUE(projector_annotation_tray->GetEnabled());

  controller_->OnCanvasInitialized(/*success=*/false);
  EXPECT_FALSE(projector_annotation_tray->GetEnabled());
}

}  // namespace ash
