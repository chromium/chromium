// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/tab_slider.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";
constexpr char kDefaultCameraDisplayName[] = "Default Cam";

// Dispatch the simulated virtual key event to the WindowEventDispatcher.
void DispatchVKEvent(ui::test::EventGenerator* event_generator,
                     bool is_press,
                     ui::KeyboardCode key_code,
                     int flags,
                     int source_device_id) {
  ui::EventType type =
      is_press ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased;
  ui::KeyEvent keyev(type, key_code, flags);

  keyev.SetProperties({{
      ui::kPropertyFromVK,
      std::vector<uint8_t>(ui::kPropertyFromVKSize),
  }});
  keyev.set_source_device_id(source_device_id);
  event_generator->Dispatch(&keyev);
}

}  // namespace

CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(source);
  controller->SetType(type);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  CHECK(controller->IsActive());
  return controller;
}

TestCaptureModeDelegate* GetTestDelegate() {
  return static_cast<TestCaptureModeDelegate*>(
      CaptureModeController::Get()->delegate_for_testing());
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void WaitForRecordingToStart() {
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress())
    return;
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnVideoRecordingStartedCallback(
      run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(controller->is_recording_in_progress());
}

void StartVideoRecordingImmediately() {
  CaptureModeController::Get()->StartVideoRecordingImmediatelyForTesting();
  WaitForRecordingToStart();
}

base::FilePath WaitForCaptureFileToBeSaved() {
  base::FilePath result;
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        result = path;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name) {
  base::FilePath custom_folder = CaptureModeController::Get()
                                     ->delegate_for_testing()
                                     ->GetUserDefaultDownloadsFolder()
                                     .Append(custom_folder_name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result = base::CreateDirectory(custom_folder);
  DCHECK(result);
  return custom_folder;
}

base::FilePath CreateFolderOnDriveFS(const std::string& custom_folder_name) {
  auto* test_delegate = CaptureModeController::Get()->delegate_for_testing();
  base::FilePath mount_point_path;
  EXPECT_TRUE(test_delegate->GetDriveFsMountPointPath(&mount_point_path));
  base::FilePath folder_on_drive_fs =
      mount_point_path.Append("root").Append(custom_folder_name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result = base::CreateDirectory(folder_on_drive_fs);
  EXPECT_TRUE(result);
  return folder_on_drive_fs;
}

void WaitForSeconds(int seconds) {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
  loop.Run();
}

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void LeaveTabletMode() {
  TabletModeControllerTestApi().LeaveTabletMode();
}

void TouchOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(view_center);
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
}

void ClickOrTapView(const views::View* view,
                    const bool in_tablet_mode,
                    ui::test::EventGenerator* event_generator) {
  if (in_tablet_mode)
    TouchOnView(view, event_generator);
  else
    ClickOnView(view, event_generator);
}

views::Widget* GetCaptureModeBarWidget() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return session->GetCaptureModeBarWidget();
}

CaptureModeBarView* GetCaptureModeBarView() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetCaptureModeBarView();
}

UserNudgeController* GetUserNudgeController() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetUserNudgeController();
}

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling) {
  DCHECK_EQ(layer->parent(), sibling->parent());
  const auto& children = layer->parent()->children();
  const int sibling_index =
      base::ranges::find(children, sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == layer;
}

void SetDeviceScaleFactor(float dsf) {
  auto* display_manager = Shell::Get()->display_manager();
  const auto display_id = display_manager->GetDisplayAt(0).id();
  display_manager->UpdateZoomFactor(display_id, dsf);
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress()) {
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    // Consume any pending video frame from before changing the DSF prior to
    // proceeding.
    test_delegate->RequestAndWaitForVideoFrame();
  }
}

views::Widget* EnableAndGetAutoClickBubbleWidget() {
  auto* autoclick_controller = Shell::Get()->autoclick_controller();
  autoclick_controller->SetEnabled(true, /*show_confirmation_dialog=*/false);
  Shell::Get()
      ->accessibility_controller()
      ->GetFeature(A11yFeatureType::kAutoclick)
      .SetEnabled(true);

  views::Widget* autoclick_bubble_widget =
      autoclick_controller->GetMenuBubbleControllerForTesting()
          ->GetBubbleWidgetForTesting();
  EXPECT_TRUE(autoclick_bubble_widget->IsVisible());
  return autoclick_bubble_widget;
}

void PressKeyOnVK(ui::test::EventGenerator* event_generator,
                  ui::KeyboardCode key_code,
                  int flags,
                  int source_device_id) {
  DispatchVKEvent(event_generator, /*is_press=*/true, key_code, flags,
                  source_device_id);
}

void ReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                    ui::KeyboardCode key_code,
                    int flags,
                    int source_device_id) {
  DispatchVKEvent(event_generator, /*is_press=*/false, key_code, flags,
                  source_device_id);
}

void PressAndReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                            ui::KeyboardCode key_code,
                            int flags,
                            int source_device_id) {
  PressKeyOnVK(event_generator, key_code, flags, source_device_id);
  ReleaseKeyOnVK(event_generator, key_code, flags, source_device_id);
}

gfx::Image ReadAndDecodeImageFile(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // No need to read the image file, if the path doesn't exist.
  if (!base::PathExists(image_path)) {
    return gfx::Image();
  }

  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read PNG file from disk.";
    return gfx::Image();
  }

  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(image_data)));

  if (image.IsEmpty()) {
    LOG(ERROR) << "Failed to decode PNG file.";
  }

  return image;
}

TabSliderButton* GetImageToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  auto* capture_type_view = GetCaptureModeBarView()->GetCaptureTypeView();
  return capture_type_view ? capture_type_view->image_toggle_button() : nullptr;
}

TabSliderButton* GetVideoToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  auto* capture_type_view = GetCaptureModeBarView()->GetCaptureTypeView();
  return capture_type_view ? capture_type_view->video_toggle_button() : nullptr;
}

TabSliderButton* GetFullscreenToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  auto* capture_source_view = GetCaptureModeBarView()->GetCaptureSourceView();
  return capture_source_view ? capture_source_view->fullscreen_toggle_button()
                             : nullptr;
}

TabSliderButton* GetRegionToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  auto* capture_source_view = GetCaptureModeBarView()->GetCaptureSourceView();
  return capture_source_view ? capture_source_view->region_toggle_button()
                             : nullptr;
}

TabSliderButton* GetWindowToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  auto* capture_source_view = GetCaptureModeBarView()->GetCaptureSourceView();
  return capture_source_view ? capture_source_view->window_toggle_button()
                             : nullptr;
}

PillButton* GetStartRecordingButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()->GetStartRecordingButton();
}

IconButton* GetSettingsButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()->settings_button();
}

IconButton* GetCloseButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()->close_button();
}

const message_center::Notification* GetPreviewNotification() {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (const message_center::Notification* notification : notifications) {
    if (notification->id() == kScreenCaptureNotificationId) {
      return notification;
    }
  }
  return nullptr;
}

void ClickOnNotification(std::optional<int> button_index) {
  const message_center::Notification* notification = GetPreviewNotification();
  CHECK(notification);
  notification->delegate()->Click(button_index, std::nullopt);
}

void AddFakeCamera(const std::string& device_id,
                   const std::string& display_name,
                   const std::string& model_id,
                   media::VideoFacingMode camera_facing_mode) {
  CameraDevicesChangeWaiter waiter;
  GetTestDelegate()->video_source_provider()->AddFakeCamera(
      device_id, display_name, model_id, camera_facing_mode);
  waiter.Wait();
}

void RemoveFakeCamera(const std::string& device_id) {
  CameraDevicesChangeWaiter waiter;
  GetTestDelegate()->video_source_provider()->RemoveFakeCamera(device_id);
  waiter.Wait();
}

void AddDefaultCamera() {
  AddFakeCamera(kDefaultCameraDeviceId, kDefaultCameraDisplayName,
                kDefaultCameraModelId);
}

void RemoveDefaultCamera() {
  RemoveFakeCamera(kDefaultCameraDeviceId);
}

size_t WaitForCameraAvailabilityWithTimeout(base::TimeDelta time_out) {
  CaptureModeTestApi test_api;
  int available_camera_num = test_api.GetNumberOfAvailableCameras();
  if (available_camera_num) {
    return available_camera_num;
  }
  base::RunLoop run_loop;
  const base::Time start_time = base::Time::Now();
  base::RepeatingTimer polling_timer;
  polling_timer.Start(
      FROM_HERE, base::Milliseconds(100), base::BindLambdaForTesting([&]() {
        available_camera_num = test_api.GetNumberOfAvailableCameras();
        base::TimeDelta time_difference = base::Time::Now() - start_time;
        if (available_camera_num > 0 || time_difference > time_out) {
          polling_timer.Stop();
          run_loop.Quit();
        }
      }));
  run_loop.Run();
  return available_camera_num;
}

void SelectCaptureModeRegion(ui::test::EventGenerator* event_generator,
                             const gfx::Rect& region_in_screen,
                             bool release_mouse,
                             bool verify_region) {
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
  event_generator->set_current_screen_location(region_in_screen.origin());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(region_in_screen.bottom_right());
  if (release_mouse) {
    event_generator->ReleaseLeftButton();
  }
  if (verify_region) {
    auto capture_region_in_root = region_in_screen;
    wm::ConvertRectFromScreen(
        controller->capture_mode_session()->current_root(),
        &capture_region_in_root);
    EXPECT_EQ(capture_region_in_root, controller->user_capture_region());
  }
}

void VerifyActiveBehavior(BehaviorType type) {
  auto* controller = CaptureModeController::Get();
  ASSERT_TRUE(controller->IsActive());
  CaptureModeBehavior* active_behavior =
      controller->capture_mode_session()->active_behavior();
  ASSERT_TRUE(active_behavior);
  EXPECT_EQ(active_behavior->behavior_type(), type);
}

// -----------------------------------------------------------------------------
// ProjectorCaptureModeIntegrationHelper:

ProjectorCaptureModeIntegrationHelper::ProjectorCaptureModeIntegrationHelper() =
    default;

void ProjectorCaptureModeIntegrationHelper::SetUp() {
  annotator_helper_.SetUp();
  auto* projector_controller = ProjectorController::Get();
  projector_controller->SetClient(&projector_client_);
  ON_CALL(projector_client_, StopSpeechRecognition)
      .WillByDefault(testing::Invoke([]() {
        ProjectorController::Get()->OnSpeechRecognitionStopped(
            /*forced=*/false);
      }));

  // Simulate the availability of speech recognition.
  SpeechRecognitionAvailability availability;
  availability.on_device_availability =
      OnDeviceRecognitionAvailability::kAvailable;
  ON_CALL(projector_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  EXPECT_CALL(projector_client_, IsDriveFsMounted())
      .WillRepeatedly(testing::Return(true));
}

bool ProjectorCaptureModeIntegrationHelper::CanStartProjectorSession() const {
  return ProjectorController::Get()->GetNewScreencastPrecondition().state !=
         NewScreencastPreconditionState::kDisabled;
}

void ProjectorCaptureModeIntegrationHelper::StartProjectorModeSession() {
  auto* projector_session = ProjectorSession::Get();
  EXPECT_FALSE(projector_session->is_active());
  auto* projector_controller = ProjectorController::Get();
  EXPECT_CALL(projector_client_, MinimizeProjectorApp());
  projector_controller->StartProjectorSession(
      base::SafeBaseName::Create("projector_data").value());
  EXPECT_TRUE(projector_session->is_active());
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

// -----------------------------------------------------------------------------
// ViewVisibilityChangeWaiter:

ViewVisibilityChangeWaiter ::ViewVisibilityChangeWaiter(views::View* view)
    : view_(view) {
  view_->AddObserver(this);
}

ViewVisibilityChangeWaiter::~ViewVisibilityChangeWaiter() {
  view_->RemoveObserver(this);
}

void ViewVisibilityChangeWaiter::Wait() {
  wait_loop_.Run();
}

void ViewVisibilityChangeWaiter::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  wait_loop_.Quit();
}

// -----------------------------------------------------------------------------
// CaptureNotificationWaiter:

CaptureNotificationWaiter::CaptureNotificationWaiter() {
  message_center::MessageCenter::Get()->AddObserver(this);
}

CaptureNotificationWaiter::~CaptureNotificationWaiter() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void CaptureNotificationWaiter::Wait() {
  run_loop_.Run();
}

void CaptureNotificationWaiter::OnNotificationAdded(
    const std::string& notification_id) {
  if (notification_id == kScreenCaptureNotificationId) {
    run_loop_.Quit();
  }
}

// -----------------------------------------------------------------------------
// CameraDevicesChangeWaiter:

CameraDevicesChangeWaiter::CameraDevicesChangeWaiter() {
  CaptureModeController::Get()->camera_controller()->AddObserver(this);
}

CameraDevicesChangeWaiter::~CameraDevicesChangeWaiter() {
  CaptureModeController::Get()->camera_controller()->RemoveObserver(this);
}

void CameraDevicesChangeWaiter::Wait() {
  loop_.Run();
}

void CameraDevicesChangeWaiter::OnAvailableCamerasChanged(
    const CameraInfoList& cameras) {
  ++camera_change_event_count_;
  loop_.Quit();
}

void CameraDevicesChangeWaiter::OnSelectedCameraChanged(
    const CameraId& camera_id) {
  ++selected_camera_change_event_count_;
}

}  // namespace ash
