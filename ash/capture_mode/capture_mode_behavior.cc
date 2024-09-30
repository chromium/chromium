// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_behavior.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/capture_region_overlay_controller.h"
#include "ash/capture_mode/game_capture_bar_view.h"
#include "ash/capture_mode/normal_capture_bar_view.h"
#include "ash/capture_mode/sunfish_capture_bar_view.h"
#include "ash/constants/ash_features.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

// Width of the full capture bar, which includes all the elements of the normal
// capture bar.
constexpr int kFullCaptureBarWidth = 376;

// Width of the game capture bar.
constexpr int kGameCaptureBarWidth = 250;

// Returns the current configs before been overwritten by the client-initiated
// capture mode session
CaptureModeSessionConfigs GetCaptureModeSessionConfigs() {
  CaptureModeController* controller = CaptureModeController::Get();
  CaptureModeSessionConfigs configs = CaptureModeSessionConfigs{
      controller->type(), controller->source(), controller->recording_type(),
      controller->audio_recording_mode(), controller->enable_demo_tools()};
  return configs;
}

void SetCaptureModeSessionConfigs(const CaptureModeSessionConfigs& configs) {
  CaptureModeController* controller = CaptureModeController::Get();
  controller->SetType(configs.type);
  controller->SetSource(configs.source);
  controller->SetRecordingType(configs.recording_type);
  controller->SetAudioRecordingMode(configs.audio_recording_mode);
  controller->EnableDemoTools(configs.demo_tools_enabled);
}

// -----------------------------------------------------------------------------
// DefaultBehavior:
// Implements the `CaptureModeBehavior` interface with behavior defined for the
// default capture mode.
class DefaultBehavior : public CaptureModeBehavior {
 public:
  DefaultBehavior()
      : CaptureModeBehavior(
            {CaptureModeType::kImage, CaptureModeSource::kRegion,
             RecordingType::kWebM, AudioRecordingMode::kOff,
             /*demo_tools_enabled=*/false},
            BehaviorType::kDefault) {}

  DefaultBehavior(const DefaultBehavior&) = delete;
  DefaultBehavior& operator=(const DefaultBehavior&) = delete;
  ~DefaultBehavior() override = default;

  // CaptureModeBehavior:
  void AttachToSession() override {
    // Do not override the session configs in `DefaultBehavior`, since the
    // source and type may have been set before the session was started and
    // initialized.
  }
  void DetachFromSession() override {}
};

// -----------------------------------------------------------------------------
// ProjectorBehavior:
// Implements the `CaptureModeBehavior` interface with behavior defined for the
// projector-initiated capture mode.
class ProjectorBehavior : public CaptureModeBehavior {
 public:
  ProjectorBehavior()
      : CaptureModeBehavior(
            {CaptureModeType::kVideo, CaptureModeSource::kFullscreen,
             RecordingType::kWebM, AudioRecordingMode::kMicrophone,
             /*demo_tools_enabled=*/true},
            BehaviorType::kProjector) {}

  ProjectorBehavior(const ProjectorBehavior&) = delete;
  ProjectorBehavior& operator=(const ProjectorBehavior&) = delete;
  ~ProjectorBehavior() override = default;

  bool ShouldImageCaptureTypeBeAllowed() const override { return false; }
  bool ShouldSaveToSettingsBeIncluded() const override { return false; }
  bool ShouldGifBeSupported() const override { return false; }
  bool ShouldShowPreviewNotification() const override { return false; }
  bool SupportsAudioRecordingMode(AudioRecordingMode mode) const override {
    switch (mode) {
      case AudioRecordingMode::kOff:
      case AudioRecordingMode::kSystem:
        // Projector does not support turning off audio recording nor recording
        // the system audio separately without the microphone.
        return false;
      case AudioRecordingMode::kMicrophone:
      case AudioRecordingMode::kSystemAndMicrophone:
        return true;
    }
  }
  bool ShouldCreateAnnotationsOverlayController() const override {
    return true;
  }
  bool ShouldShowUserNudge() const override { return false; }
  bool ShouldAutoSelectFirstCamera() const override { return true; }
  bool RequiresCaptureFolderCreation() const override { return true; }
  void CreateCaptureFolder(OnCaptureFolderCreatedCallback callback) override {
    ProjectorControllerImpl::Get()->CreateScreencastContainerFolder(
        base::BindOnce(&ProjectorBehavior::OnScreencastContainerFolderCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  std::vector<RecordingType> GetSupportedRecordingTypes() const override {
    return std::vector<RecordingType>{RecordingType::kWebM};
  }
  const char* GetClientMetricComponent() const override { return "Projector."; }

 protected:
  int GetCaptureBarWidth() const override {
    return kFullCaptureBarWidth - capture_mode::kButtonSize.width() -
           capture_mode::kSpaceBetweenCaptureModeTypeButtons;
  }

 private:
  // Called when the Projector controller creates the DriveFS folder that will
  // host the video file along with the associated metadata file created by the
  // Projector session.
  void OnScreencastContainerFolderCreated(
      OnCaptureFolderCreatedCallback callback,
      const base::FilePath& capture_file_full_path) {
    base::FilePath path;
    // An empty path is sent to indicate an error.
    if (!capture_file_full_path.empty()) {
      path = capture_file_full_path.AddExtension("webm");
    }
    std::move(callback).Run(path);
  }

  base::WeakPtrFactory<ProjectorBehavior> weak_ptr_factory_{this};
};

// -----------------------------------------------------------------------------
// GameDashboardBehavior:
// Implements the `CaptureModeBehavior` interface with behaviors defined by the
// game dashboard-initiated capture mode.
class GameDashboardBehavior : public CaptureModeBehavior,
                              public aura::WindowObserver {
 public:
  GameDashboardBehavior()
      : CaptureModeBehavior(
            {CaptureModeType::kVideo, CaptureModeSource::kWindow,
             RecordingType::kWebM, AudioRecordingMode::kSystemAndMicrophone,
             /*demo_tools_enabled=*/false},
            BehaviorType::kGameDashboard) {}

  GameDashboardBehavior(const GameDashboardBehavior&) = delete;
  GameDashboardBehavior operator=(const GameDashboardBehavior&) = delete;
  ~GameDashboardBehavior() override = default;

  // CaptureModeBehavior:
  void AttachToSession() override {
    CaptureModeBehavior::AttachToSession();

    CaptureModeController* controller = CaptureModeController::Get();
    BaseCaptureModeSession* session = controller->capture_mode_session();
    CHECK(session);
    if (!pre_selected_window_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<GameDashboardBehavior> game_dashboard_behavior,
                 CaptureModeController* controller) {
                if (controller->IsActive()) {
                  controller->Stop();
                }
              },
              weak_ptr_factory_.GetWeakPtr(), controller));
    } else {
      session->SetPreSelectedWindow(pre_selected_window_);
    }
  }

  void DetachFromSession() override {
    CaptureModeBehavior::DetachFromSession();

    if (pre_selected_window_) {
      pre_selected_window_->RemoveObserver(this);
      pre_selected_window_ = nullptr;
    }
  }

  bool ShouldImageCaptureTypeBeAllowed() const override { return false; }
  bool ShouldFulscreenCaptureSourceBeAllowed() const override { return false; }
  bool ShouldRegionCaptureSourceBeAllowed() const override { return false; }
  bool ShouldDemoToolsSettingsBeIncluded() const override { return false; }
  bool ShouldGifBeSupported() const override { return false; }
  bool ShouldShowUserNudge() const override { return false; }
  bool ShouldAutoSelectFirstCamera() const override {
    return !CaptureModeController::Get()
                ->camera_controller()
                ->did_user_ever_change_camera();
  }

  std::unique_ptr<CaptureModeBarView> CreateCaptureModeBarView() override {
    return std::make_unique<GameCaptureBarView>();
  }

  void SetPreSelectedWindow(aura::Window* pre_selected_window) override {
    CHECK(!pre_selected_window_);
    pre_selected_window_ = pre_selected_window;
    pre_selected_window_->AddObserver(this);
  }

  const char* GetClientMetricComponent() const override {
    return "GameDashboard.";
  }

  std::vector<message_center::ButtonInfo> GetNotificationButtonsInfo(
      bool for_video) const override {
    return {message_center::ButtonInfo{l10n_util::GetStringUTF16(
                for_video ? IDS_ASH_SCREEN_CAPTURE_SHARE_TO_YOUTUBE
                          : IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT)},
            message_center::ButtonInfo{l10n_util::GetStringUTF16(
                IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE)}};
  }

  void OnAudioRecordingModeChanged() override {
    capture_mode_configs_.audio_recording_mode =
        CaptureModeController::Get()->audio_recording_mode();
  }

  void OnDemoToolsSettingsChanged() override {
    capture_mode_configs_.demo_tools_enabled =
        CaptureModeController::Get()->enable_demo_tools();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    CHECK_EQ(window, pre_selected_window_);
    pre_selected_window_->RemoveObserver(this);
    pre_selected_window_ = nullptr;
  }

 protected:
  // CaptureModeBehavior:
  gfx::Rect GetBarAnchorBoundsInScreen(aura::Window* root) const override {
    CHECK(pre_selected_window_);
    return pre_selected_window_->GetBoundsInScreen();
  }

  int GetCaptureBarBottomPadding() const override {
    return capture_mode::kGameCaptureBarBottomPadding;
  }

  int GetCaptureBarWidth() const override { return kGameCaptureBarWidth; }

 private:
  raw_ptr<aura::Window> pre_selected_window_ = nullptr;
  base::WeakPtrFactory<GameDashboardBehavior> weak_ptr_factory_{this};
};

// -----------------------------------------------------------------------------
// SunfishBehavior:
// Implements the `CaptureModeBehavior` interface with behavior defined for the
// sunfish capture mode.
class SunfishBehavior : public CaptureModeBehavior {
 public:
  SunfishBehavior()
      : CaptureModeBehavior(
            {CaptureModeType::kImage, CaptureModeSource::kRegion,
             RecordingType::kWebM, AudioRecordingMode::kOff,
             /*demo_tools_enabled=*/false},
            BehaviorType::kSunfish) {}

  SunfishBehavior(const SunfishBehavior&) = delete;
  SunfishBehavior& operator=(const SunfishBehavior&) = delete;
  ~SunfishBehavior() override = default;

  // CaptureModeBehavior:
  void AttachToSession() override {
    CaptureModeBehavior::AttachToSession();
    if (auto* scanner_controller = Shell::Get()->scanner_controller()) {
      scanner_controller->StartNewSession();
    }
  }
  void DetachFromSession() override {
    CaptureModeBehavior::DetachFromSession();
    if (auto* scanner_controller = Shell::Get()->scanner_controller()) {
      scanner_controller->OnSessionUIClosed();
    }
  }
  bool ShouldShowUserNudge() const override { return false; }
  bool ShouldReShowUisAtPerformingCapture() const override { return true; }
  bool ShouldShowCaptureButtonAfterRegionSelected() const override {
    return false;
  }
  const std::u16string GetCaptureLabelRegionText() const override {
    return l10n_util::GetStringUTF16(IDS_ASH_SUNFISH_CAPTURE_LABEL);
  }
  int GetCaptureBarWidth() const override {
    // Return the height so the button is circular.
    return capture_mode::kCaptureBarHeight;
  }
  std::unique_ptr<CaptureModeBarView> CreateCaptureModeBarView() override {
    return std::make_unique<SunfishCaptureBarView>();
  }
  void PaintCaptureRegionOverlay(
      gfx::Canvas& canvas,
      const gfx::Rect& region_bounds_in_canvas) const override {
    capture_region_overlay_controller_.PaintCaptureRegionOverlay(
        canvas, region_bounds_in_canvas);
  }
  void OnRegionSelected() override {
    // `CaptureModeController` will perform DLP restriction checks and determine
    // whether the image can be sent for search.
    CaptureModeController::Get()->PerformCapture();
  }
  void OnEnterKeyPressed() override {}

 private:
  // Controls the overlay shown on the capture region to indicate detected text,
  // translations, etc.
  CaptureRegionOverlayController capture_region_overlay_controller_;
};

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeBehavior:

CaptureModeBehavior::CaptureModeBehavior(
    const CaptureModeSessionConfigs& configs,
    const BehaviorType behavior_type)
    : capture_mode_configs_(configs), behavior_type_(behavior_type) {}

// static
std::unique_ptr<CaptureModeBehavior> CaptureModeBehavior::Create(
    BehaviorType behavior_type) {
  switch (behavior_type) {
    case BehaviorType::kProjector:
      return std::make_unique<ProjectorBehavior>();
    case BehaviorType::kGameDashboard:
      return std::make_unique<GameDashboardBehavior>();
    case BehaviorType::kDefault:
      return std::make_unique<DefaultBehavior>();
    case BehaviorType::kSunfish:
      return std::make_unique<SunfishBehavior>();
  }
}

void CaptureModeBehavior::AttachToSession() {
  cached_configs_ = GetCaptureModeSessionConfigs();

  // Overwrite the current capture mode session with the behavior
  // configurations.
  SetCaptureModeSessionConfigs(capture_mode_configs_);
}

void CaptureModeBehavior::DetachFromSession() {
  CHECK(cached_configs_);

  // Restore the capture mode configurations after being overwritten with the
  // behavior-specific configurations.
  SetCaptureModeSessionConfigs(cached_configs_.value());
  cached_configs_.reset();
}

bool CaptureModeBehavior::ShouldImageCaptureTypeBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldVideoCaptureTypeBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldFulscreenCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldRegionCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldWindowCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::SupportsAudioRecordingMode(
    AudioRecordingMode mode) const {
  switch (mode) {
    case AudioRecordingMode::kOff:
    case AudioRecordingMode::kMicrophone:
    case AudioRecordingMode::kSystem:
    case AudioRecordingMode::kSystemAndMicrophone:
      return true;
  }
}

bool CaptureModeBehavior::ShouldCameraSelectionSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldDemoToolsSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldSaveToSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldGifBeSupported() const {
  return true;
}

bool CaptureModeBehavior::ShouldShowPreviewNotification() const {
  return true;
}

bool CaptureModeBehavior::ShouldSkipVideoRecordingCountDown() const {
  return false;
}

bool CaptureModeBehavior::ShouldCreateAnnotationsOverlayController() const {
  if (base::FeatureList::IsEnabled(ash::features::kAnnotatorMode)) {
    return true;
  }
  return false;
}

bool CaptureModeBehavior::ShouldShowUserNudge() const {
  return true;
}

bool CaptureModeBehavior::ShouldAutoSelectFirstCamera() const {
  return false;
}

bool CaptureModeBehavior::RequiresCaptureFolderCreation() const {
  return false;
}

bool CaptureModeBehavior::ShouldReShowUisAtPerformingCapture() const {
  // We don't need to bring capture mode UIs back if `type_` is
  // `CaptureModeType::kImage`, since the session is about to shutdown anyways
  // at these use cases, so it's better to avoid any wasted effort. In the case
  // of video recording, we need to reshow the UIs so that we can start the
  // 3-second count down animation.
  return CaptureModeController::Get()->type() != CaptureModeType::kImage;
}

bool CaptureModeBehavior::ShouldShowCaptureButtonAfterRegionSelected() const {
  return true;
}

void CaptureModeBehavior::CreateCaptureFolder(
    OnCaptureFolderCreatedCallback callback) {
  NOTREACHED();
}

std::vector<RecordingType> CaptureModeBehavior::GetSupportedRecordingTypes()
    const {
  std::vector<RecordingType> supported_recording_types;
  supported_recording_types.push_back(RecordingType::kWebM);
  if (features::IsGifRecordingEnabled()) {
    supported_recording_types.push_back(RecordingType::kGif);
  }
  return supported_recording_types;
}

void CaptureModeBehavior::SetPreSelectedWindow(
    aura::Window* pre_selected_window) {
  NOTREACHED();
}

const char* CaptureModeBehavior::GetClientMetricComponent() const {
  return "";
}

std::vector<message_center::ButtonInfo>
CaptureModeBehavior::GetNotificationButtonsInfo(bool for_video) const {
  std::vector<message_center::ButtonInfo> buttons_info;

  if (!for_video &&
      !Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    buttons_info.emplace_back(
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
  }

  buttons_info.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE));

  return buttons_info;
}

const std::u16string CaptureModeBehavior::GetCaptureLabelRegionText() const {
  CaptureModeController* controller = CaptureModeController::Get();
  DCHECK(controller->user_capture_region().IsEmpty());
  return l10n_util::GetStringUTF16(
      controller->type() == CaptureModeType::kImage
          ? IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_IMAGE_CAPTURE
          : IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_VIDEO_RECORD);
}

std::unique_ptr<CaptureModeBarView>
CaptureModeBehavior::CreateCaptureModeBarView() {
  return std::make_unique<NormalCaptureBarView>(this);
}

gfx::Rect CaptureModeBehavior::GetCaptureBarBounds(aura::Window* root) const {
  auto bounds = GetBarAnchorBoundsInScreen(root);
  const int bar_y = bounds.bottom() - GetCaptureBarBottomPadding() -
                    capture_mode::kCaptureBarHeight;
  bounds.ClampToCenteredSize(
      gfx::Size(GetCaptureBarWidth(), capture_mode::kCaptureBarHeight));
  bounds.set_y(bar_y);
  return bounds;
}

gfx::Rect CaptureModeBehavior::GetBarAnchorBoundsInScreen(
    aura::Window* root) const {
  CHECK(root);
  auto bounds = root->GetBoundsInScreen();
  int new_bottom = bounds.bottom();

  Shelf* shelf = Shelf::ForWindow(root);
  if (shelf->IsHorizontalAlignment()) {
    // Get the widget which has the shelf icons. This is the hotseat widget if
    // the hotseat is extended, shelf widget otherwise.
    const bool hotseat_extended =
        shelf->shelf_layout_manager()->hotseat_state() ==
        HotseatState::kExtended;
    views::Widget* shelf_widget =
        hotseat_extended ? static_cast<views::Widget*>(shelf->hotseat_widget())
                         : static_cast<views::Widget*>(shelf->shelf_widget());
    new_bottom = shelf_widget->GetWindowBoundsInScreen().y();
  }
  bounds.set_height(new_bottom - bounds.y());
  return bounds;
}

int CaptureModeBehavior::GetCaptureBarBottomPadding() const {
  return capture_mode::kCaptureBarBottomPadding;
}

int CaptureModeBehavior::GetCaptureBarWidth() const {
  return kFullCaptureBarWidth;
}

void CaptureModeBehavior::PaintCaptureRegionOverlay(
    gfx::Canvas& canvas,
    const gfx::Rect& region_bounds_in_canvas) const {}

void CaptureModeBehavior::OnAudioRecordingModeChanged() {}

void CaptureModeBehavior::OnDemoToolsSettingsChanged() {}

void CaptureModeBehavior::OnRegionSelected() {}

void CaptureModeBehavior::OnEnterKeyPressed() {
  CaptureModeController::Get()->PerformCapture();
}

}  // namespace ash
