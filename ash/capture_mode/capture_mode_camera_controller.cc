// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_controller.h"

#include <algorithm>
#include <cstring>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_properties.h"

namespace ash {

namespace {

// The maximum amount of time we allow a `selected_camera_` to remain
// disconnected before we consider it gone forever, and we clear its ID from
// `selected_camera_`.
constexpr base::TimeDelta kDisconnectionGracePeriod = base::Seconds(10);

// The animation duration for the bounds change operation on the camera preview.
constexpr base::TimeDelta kCameraBoundsChangeAnimationDuration =
    base::Milliseconds(150);

// The duration for the camera preview fading out process.
constexpr base::TimeDelta kCameraPreviewFadeOutDuration =
    base::Milliseconds(50);
// The duration for the camera preview fading in process.
constexpr base::TimeDelta kCameraPreviewFadeInDuration =
    base::Milliseconds(150);

// Defines a map type to map a camera model ID (or display name) to the number
// of cameras of that model that are currently connected.
using ModelIdToCountMap = std::map<std::string, int>;

// Using the given `cam_models_map` which tracks the number of cameras connected
// of each model, returns the next `CameraId::number` for the given
// `model_id_or_display_name`.
int GetNextCameraNumber(const std::string& model_id_or_display_name,
                        ModelIdToCountMap* cam_models_map) {
  return ++(*cam_models_map)[model_id_or_display_name];
}

// Returns a reference to either the model ID (if available) or the display name
// from the given `descriptor`.
const std::string& PickModelIdOrDisplayName(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  return descriptor.model_id.empty() ? descriptor.display_name()
                                     : descriptor.model_id;
}

// Returns true if the `incoming_list` (supplied by the video source provider)
// contains different items than the ones in `current_list` (which is the
// currently `available_cameras_` maintained by `CaptureModeCameraController`).
bool DidDevicesChange(
    const std::vector<media::VideoCaptureDeviceInfo>& incoming_list,
    const CameraInfoList& current_list) {
  if (incoming_list.size() != current_list.size())
    return true;

  ModelIdToCountMap cam_models_map;
  for (const auto& incoming_camera : incoming_list) {
    const auto& device_id = incoming_camera.descriptor.device_id;
    const auto iter = std::find_if(current_list.begin(), current_list.end(),
                                   [device_id](const CameraInfo& info) {
                                     return info.device_id == device_id;
                                   });
    if (iter == current_list.end())
      return true;

    const auto& model_id_or_display_name =
        PickModelIdOrDisplayName(incoming_camera.descriptor);
    const int cam_number =
        GetNextCameraNumber(model_id_or_display_name, &cam_models_map);

    const CameraInfo& found_info = *iter;
    if (found_info.display_name != incoming_camera.descriptor.display_name() ||
        found_info.camera_id.model_id_or_display_name() !=
            model_id_or_display_name ||
        found_info.camera_id.number() != cam_number) {
      // It is unexpected that the supported formats of the same camera device
      // change, so we ignore comparing them here.
      return true;
    }
  }

  return false;
}

// Picks and returns the most suitable supported camera format from the given
// list of `supported_formats` for the given camera `preview_widget_size`.
// Note that this assumes that `supported_formats` is sorted as described in the
// documentation of `CameraInfo::supported_formats`.
media::VideoCaptureFormat PickSuitableCaptureFormat(
    const gfx::Size& preview_widget_size,
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(!supported_formats.empty());
  DCHECK_EQ(preview_widget_size.height(), preview_widget_size.width())
      << "The preview widget is always assumed to be a square.";

  size_t result_index = 0;
  float current_frame_rate = 0.f;
  for (size_t i = 0; i < supported_formats.size(); ++i) {
    const auto& format = supported_formats[i];
    // Once we find a format with a larger height than the preview's, we stop
    // and return what we found so far.
    if (format.frame_size.height() > preview_widget_size.height())
      break;

    if (format.frame_rate >= current_frame_rate && format.frame_rate <= 30.f) {
      current_frame_rate = format.frame_rate;
      result_index = i;
    }
  }

  return supported_formats[result_index];
}

// Returns the CameraInfo item in `list` whose ID is equal to the given `id`, or
// nullptr if no such item exists.
const CameraInfo* GetCameraInfoById(const CameraId& id,
                                    const CameraInfoList& list) {
  const auto iter = std::find_if(
      list.begin(), list.end(),
      [&id](const CameraInfo& info) { return info.camera_id == id; });
  return iter == list.end() ? nullptr : &(*iter);
}

// Returns the widget init params needed to create the camera preview widget.
views::Widget::InitParams CreateWidgetParams(const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent = CaptureModeController::Get()->GetCameraPreviewParentWindow();
  params.bounds = bounds;
  // Need to set `params.child` to true here, otherwise camera preview widget
  // will be added as a transient child to `params.parent`. For more details,
  // please check `NativeWidgetAura::InitNativeWidget`.
  params.child = true;
  params.name = "CameraPreviewWidget";
  return params;
}

// Called by `ContinueDraggingPreview` to make sure camera preview is not
// dragged outside of the capture surface.
void AdjustBoundsWithinConfinedBounds(const gfx::Rect& confined_bounds,
                                      gfx::Rect& preview_bounds) {
  const int x = preview_bounds.x();
  if (int offset = x - confined_bounds.x(); offset < 0) {
    preview_bounds.set_x(x - offset);
  } else if (int offset = confined_bounds.right() - preview_bounds.right();
             offset < 0) {
    preview_bounds.set_x(x + offset);
  }

  const int y = preview_bounds.y();
  if (int offset = y - confined_bounds.y(); offset < 0) {
    preview_bounds.set_y(y - offset);
  } else if (int offset = confined_bounds.bottom() - preview_bounds.bottom();
             offset < 0) {
    preview_bounds.set_y(y + offset);
  }
}

gfx::Size GetInitialPreviewSize(bool is_camera_preview_collapsed) {
  return is_camera_preview_collapsed ? capture_mode::kCollapsedPreviewSize
                                     : capture_mode::kCameraPreviewSize;
}

// Returns the bounds that should be used in the bounds animation of the given
// `camera_preview_window`. If this window is parented to a window that uses
// screen coordinates, then the given `target_bounds` are in screen coordinates,
// and cannot be used for bounds animation (bounds animates relative to the
// window's parent). In this case, the bounds returned are in parent
// coordinates.
gfx::Rect GetTargetBoundsForBoundsAnimation(
    const gfx::Rect& target_bounds,
    aura::Window* camera_preview_window) {
  gfx::Rect result = target_bounds;
  auto* parent = camera_preview_window->parent();
  if (parent->GetProperty(wm::kUsesScreenCoordinatesKey))
    wm::ConvertRectFromScreen(parent, &result);
  return result;
}

gfx::Rect GetCollisionAvoidanceRect(aura::Window* root_window) {
  DCHECK(root_window);

  UnifiedSystemTray* tray = RootWindowController::ForWindow(root_window)
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();

  if (!tray->IsBubbleShown())
    return gfx::Rect();

  gfx::Rect collision_avoidance_rect = tray->GetBubbleBoundsInScreen();
  auto* message_center_bubble = tray->message_center_bubble();

  if (message_center_bubble->IsMessageCenterVisible())
    collision_avoidance_rect.Union(message_center_bubble->GetBoundsInScreen());

  // TODO(conniekxu): Return a vector of collision avoidance rects including
  // other system UIs, like launcher.
  return collision_avoidance_rect;
}

// Called to avoid overlap between camera preview and system UIs when camera
// preview's bounds are changed, camera preview is created or destroyed.
void UpdateFloatingPanelBoundsIfNeeded(aura::Window* root_window) {
  DCHECK(root_window);

  Shell::Get()->accessibility_controller()->UpdateFloatingPanelBoundsIfNeeded();

  auto* pip_window_container =
      root_window->GetChildById(kShellWindowId_PipContainer);

  for (aura::Window* pip_window : pip_window_container->children()) {
    auto* pip_window_state = WindowState::Get(pip_window);
    if (pip_window_state->IsPip())
      pip_window_state->UpdatePipBounds();
  }
}

// Defines a window targeter that will be installed on the camera preview
// widget's window so that we can allow located events outside of the camera
// preview circle to go through and not be consumed by the camera preview. This
// enables the user to interact with other UI components below camera preview.
class CameraPreviewTargeter : public aura::WindowTargeter {
 public:
  explicit CameraPreviewTargeter(aura::Window* camera_preview_window)
      : camera_preview_window_(camera_preview_window) {}
  CameraPreviewTargeter(const CameraPreviewTargeter&) = delete;
  CameraPreviewTargeter& operator=(const CameraPreviewTargeter&) = delete;
  ~CameraPreviewTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent()) {
      auto screen_location = event->AsLocatedEvent()->root_location();
      wm::ConvertPointToScreen(camera_preview_window_->GetRootWindow(),
                               &screen_location);
      const gfx::Rect camera_preview_bounds =
          camera_preview_window_->GetBoundsInScreen();
      const gfx::Point camera_preview_center_point =
          camera_preview_bounds.CenterPoint();
      const int camera_preview_radius = camera_preview_bounds.width() / 2;

      // Check if events are outside of the camera preview circle by comparing
      // if the distance between screen location and center of camera preview is
      // larger than camera preview circle's radius. If it's larger, allow the
      // events to go through so that they can be used by other UI components
      // below camera preview.
      if ((screen_location - camera_preview_center_point).LengthSquared() >
          camera_preview_radius * camera_preview_radius) {
        return nullptr;
      }
    }

    return aura::WindowTargeter::FindTargetForEvent(root, event);
  }

 private:
  aura::Window* const camera_preview_window_;
};

}  // namespace

// -----------------------------------------------------------------------------
// CameraId:

CameraId::CameraId(std::string model_id_or_display_name, int number)
    : model_id_or_display_name_(std::move(model_id_or_display_name)),
      number_(number) {
  DCHECK(!model_id_or_display_name_.empty());
  DCHECK_GE(number, 1);
}

bool CameraId::operator<(const CameraId& rhs) const {
  const int result = std::strcmp(model_id_or_display_name_.c_str(),
                                 rhs.model_id_or_display_name_.c_str());
  return result != 0 ? result : (number_ < rhs.number_);
}

std::string CameraId::ToString() const {
  return base::StringPrintf("%s:%0d", model_id_or_display_name_.c_str(),
                            number_);
}

// -----------------------------------------------------------------------------
// CameraInfo:

CameraInfo::CameraInfo(CameraId camera_id,
                       std::string device_id,
                       std::string display_name,
                       const media::VideoCaptureFormats& supported_formats)
    : camera_id(std::move(camera_id)),
      device_id(std::move(device_id)),
      display_name(std::move(display_name)),
      supported_formats(supported_formats) {}

CameraInfo::CameraInfo(CameraInfo&&) = default;

CameraInfo& CameraInfo::operator=(CameraInfo&&) = default;

CameraInfo::~CameraInfo() = default;

// -----------------------------------------------------------------------------
// CaptureModeCameraController:

CaptureModeCameraController::CaptureModeCameraController(
    CaptureModeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(base::SystemMonitor::Get())
      << "No instance of SystemMonitor exists. If this is a unit test, please "
         "create one.";

  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
  ReconnectToVideoSourceProvider();
  Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(this);
}

CaptureModeCameraController::~CaptureModeCameraController() {
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(this);
}

void CaptureModeCameraController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CaptureModeCameraController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CaptureModeCameraController::MaybeSelectFirstCamera() {
  if (!selected_camera_.is_valid() && !available_cameras_.empty())
    SetSelectedCamera(available_cameras_[0].camera_id);
}

bool CaptureModeCameraController::IsCameraDisabledByPolicy() const {
  return delegate_->IsCameraDisabledByPolicy();
}

std::string CaptureModeCameraController::GetDisplayNameOfSelectedCamera()
    const {
  if (selected_camera_.is_valid()) {
    const CameraInfo* camera_info =
        GetCameraInfoById(selected_camera_, available_cameras_);
    DCHECK(camera_info);
    return camera_info->display_name;
  }
  return std::string();
}

void CaptureModeCameraController::SetSelectedCamera(CameraId camera_id) {
  // When cameras are disabled by policy, we don't allow any camera selection.
  if (IsCameraDisabledByPolicy()) {
    LOG(WARNING) << "Camera is disabled by policy. Selecting camera: "
                 << camera_id.ToString() << " will be ignored.";
    camera_id = CameraId{};
  }

  if (selected_camera_ == camera_id)
    return;

  selected_camera_ = std::move(camera_id);
  camera_reconnect_timer_.Stop();

  for (auto& observer : observers_)
    observer.OnSelectedCameraChanged(selected_camera_);

  RefreshCameraPreview();
}

void CaptureModeCameraController::SetShouldShowPreview(bool value) {
  should_show_preview_ = value;
  RefreshCameraPreview();
}

void CaptureModeCameraController::MaybeReparentPreviewWidget() {
  if (!camera_preview_widget_)
    return;

  const bool was_visible_before = camera_preview_widget_->IsVisible();
  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  auto* parent = controller->GetCameraPreviewParentWindow();
  DCHECK(parent);
  auto* native_window = camera_preview_widget_->GetNativeWindow();

  if (parent != native_window->parent())
    views::Widget::ReparentNativeView(native_window, parent);

  MaybeUpdatePreviewWidgetBounds();
  if (was_visible_before != camera_preview_widget_->IsVisible()) {
    capture_mode_util::TriggerAccessibilityAlertSoon(
        was_visible_before ? IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_HIDDEN
                           : IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_ON);
  }
}

void CaptureModeCameraController::SetCameraPreviewSnapPosition(
    CameraPreviewSnapPosition value) {
  if (camera_preview_snap_position_ == value)
    return;

  camera_preview_snap_position_ = value;
  MaybeUpdatePreviewWidgetBounds();
}

void CaptureModeCameraController::MaybeUpdatePreviewWidgetBounds(bool animate) {
  if (!camera_preview_widget_)
    return;

  // The widget will be hidden if being parented to
  // `kShellWindowId_UnparentedContainer`, we do not need to update its bounds
  // in this case.
  if (camera_preview_widget_->GetNativeWindow()->parent()->GetId() ==
      kShellWindowId_UnparentedContainer) {
    return;
  }

  const gfx::Rect target_bounds = CalculatePreviewWidgetTargetBounds();
  const auto current_bounds =
      GetCurrentBoundsMatchingConfineBoundsCoordinates();
  if (target_bounds == current_bounds)
    return;

  auto* preview_window = camera_preview_widget_->GetNativeWindow();
  if (animate) {
    views::AnimationBuilder builder;
    builder.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    if (target_bounds.size() == current_bounds.size()) {
      // Use transform animation.
      camera_preview_widget_->SetBounds(target_bounds);
      gfx::Transform transform;
      transform.Translate(current_bounds.CenterPoint() -
                          target_bounds.CenterPoint());
      ui::Layer* layer = preview_window->layer();
      layer->SetTransform(transform);
      builder.Once()
          .SetDuration(kCameraBoundsChangeAnimationDuration)
          .SetTransform(layer, gfx::Transform(),
                        gfx::Tween::ACCEL_5_70_DECEL_90);
    } else {
      // Use bounds animation.
      const auto target_bounds_in_parent =
          GetTargetBoundsForBoundsAnimation(target_bounds, preview_window);
      builder.Once()
          .SetDuration(kCameraBoundsChangeAnimationDuration)
          .SetBounds(preview_window, target_bounds_in_parent,
                     gfx::Tween::ACCEL_20_DECEL_100);
    }
  } else {
    camera_preview_widget_->SetBounds(target_bounds);
  }

  auto* controller = CaptureModeController::Get();
  if (controller->IsActive())
    controller->capture_mode_session()->OnCameraPreviewBoundsChanged();

  UpdateFloatingPanelBoundsIfNeeded(preview_window->GetRootWindow());
}

void CaptureModeCameraController::StartDraggingPreview(
    const gfx::PointF& screen_location) {
  is_drag_in_progress_ = true;
  previous_location_in_screen_ = screen_location;

  camera_preview_view_->RefreshResizeButtonVisibility();

  auto* controller = CaptureModeController::Get();
  if (controller->IsActive())
    controller->capture_mode_session()->OnCameraPreviewDragStarted();

  // Use cursor compositing instead of the platform cursor when dragging to
  // ensure the cursor is aligned with the camera preview.
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void CaptureModeCameraController::ContinueDraggingPreview(
    const gfx::PointF& screen_location) {
  gfx::Rect current_bounds = GetCurrentBoundsMatchingConfineBoundsCoordinates();

  current_bounds.Offset(
      gfx::ToRoundedVector2d(screen_location - previous_location_in_screen_));
  AdjustBoundsWithinConfinedBounds(
      CaptureModeController::Get()->GetCameraPreviewConfineBounds(),
      current_bounds);
  camera_preview_widget_->SetBounds(current_bounds);
  previous_location_in_screen_ = screen_location;
}

void CaptureModeCameraController::EndDraggingPreview(
    const gfx::PointF& screen_location,
    bool is_touch) {
  ContinueDraggingPreview(screen_location);
  UpdateSnapPositionOnDragEnded();

  MaybeUpdatePreviewWidgetBounds(/*animate=*/true);

  is_drag_in_progress_ = false;
  camera_preview_view_->RefreshResizeButtonVisibility();

  // Disable cursor compositing at the end of the drag.
  Shell::Get()->UpdateCursorCompositingEnabled();

  auto* controller = CaptureModeController::Get();
  if (controller->IsActive()) {
    controller->capture_mode_session()->OnCameraPreviewDragEnded(
        gfx::ToRoundedPoint(screen_location), is_touch);
  }
}

void CaptureModeCameraController::ToggleCameraPreviewSize() {
  DCHECK(camera_preview_view_);
  is_camera_preview_collapsed_ = !is_camera_preview_collapsed_;
  // The order here matters, the preferred size for `camera_preview_view_`
  // should always be set before  `MaybeUpdatePreviewWidgetBounds`, since
  // `GetPreviewWidgetBounds` called by `MaybeUpdatePreviewWidgetBounds` has
  // dependency on the preferred size of the `camera_preview_view_`.
  auto preferred_size = camera_preview_view_->GetPreferredSize();
  const float scale_factor = is_camera_preview_collapsed_ ? 0.5f : 2.f;
  preferred_size = gfx::ScaleToFlooredSize(preferred_size, scale_factor);
  camera_preview_view_->SetPreferredSize(preferred_size);

  MaybeUpdatePreviewWidgetBounds(/*animate=*/true);
}

void CaptureModeCameraController::FadeInCameraPreview() {
  DCHECK(camera_preview_widget_);
  DCHECK(!camera_preview_widget_->GetNativeWindow()->TargetVisibility());

  camera_preview_widget_->Show();
  auto* layer = camera_preview_widget_->GetLayer();
  layer->SetOpacity(0.f);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kCameraPreviewFadeInDuration)
      .SetOpacity(layer, 1.f, gfx::Tween::LINEAR);
}

void CaptureModeCameraController::FadeOutCameraPreview() {
  DCHECK(camera_preview_widget_);
  DCHECK(camera_preview_widget_->GetNativeWindow()->TargetVisibility());

  auto* layer = camera_preview_widget_->GetLayer();
  DCHECK_EQ(layer->GetTargetOpacity(), 1.f);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CaptureModeCameraController> controller) {
            if (!controller || !controller->camera_preview_widget_)
              return;
            controller->camera_preview_widget_->Hide();
            controller->camera_preview_widget_->GetLayer()->SetOpacity(1.f);
          },
          weak_ptr_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kCameraPreviewFadeOutDuration)
      .SetOpacity(layer, 0.f, gfx::Tween::LINEAR);
}

void CaptureModeCameraController::OnRecordingStarted(
    bool is_in_projector_mode) {
  // Check if there's a camera disconnection that happened before recording
  // starts. In this case, we don't want the camera preview to show, even if the
  // camera reconnects within the allowed grace period.
  if (selected_camera_.is_valid() && !camera_preview_widget_)
    SetShouldShowPreview(false);

  in_recording_camera_disconnections_ = 0;

  const bool starts_with_camera = camera_preview_widget();
  RecordRecordingStartsWithCamera(starts_with_camera, is_in_projector_mode);
  RecordCameraSizeOnStart(is_camera_preview_collapsed_
                              ? CaptureModeCameraSize::kCollapsed
                              : CaptureModeCameraSize::kExpanded);
  RecordCameraPositionOnStart(camera_preview_snap_position_);
}

void CaptureModeCameraController::OnRecordingEnded() {
  DCHECK(in_recording_camera_disconnections_);
  SetShouldShowPreview(false);
  RecordCameraDisconnectionsDuringRecordings(
      *in_recording_camera_disconnections_);
  in_recording_camera_disconnections_.reset();
}

void CaptureModeCameraController::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE)
    GetCameraDevices();
}

void CaptureModeCameraController::OnSystemTrayBubbleShown() {
  MaybeUpdatePreviewWidgetBounds(/*animate=*/true);
}

void CaptureModeCameraController::ReconnectToVideoSourceProvider() {
  video_source_provider_remote_.reset();
  most_recent_request_id_ = 0;
  delegate_->ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(base::BindOnce(
      &CaptureModeCameraController::ReconnectToVideoSourceProvider,
      base::Unretained(this)));
  GetCameraDevices();
}

void CaptureModeCameraController::GetCameraDevices() {
  DCHECK(video_source_provider_remote_);

  video_source_provider_remote_->GetSourceInfos(base::BindOnce(
      &CaptureModeCameraController::OnCameraDevicesReceived,
      weak_ptr_factory_.GetWeakPtr(), ++most_recent_request_id_));
}

void CaptureModeCameraController::OnCameraDevicesReceived(
    RequestId request_id,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  if (request_id < most_recent_request_id_) {
    // Ignore any out-dated requests replies, since a reply from a more recent
    // request is pending.
    return;
  }

  DCHECK_EQ(request_id, most_recent_request_id_);

  // Run the optional for-test closure at the exit of this function's scope.
  base::ScopedClosureRunner deferred_runner;
  if (on_camera_list_received_for_test_) {
    deferred_runner.ReplaceClosure(
        std::move(on_camera_list_received_for_test_));
  }

  if (!DidDevicesChange(devices, available_cameras_))
    return;

  available_cameras_.clear();
  ModelIdToCountMap cam_models_map;
  for (const auto& device : devices) {
    const auto& descriptor = device.descriptor;
    const auto& model_id_or_display_name = PickModelIdOrDisplayName(descriptor);
    const int cam_number =
        GetNextCameraNumber(model_id_or_display_name, &cam_models_map);
    available_cameras_.emplace_back(
        CameraId(model_id_or_display_name, cam_number), descriptor.device_id,
        descriptor.display_name(), device.supported_formats);
  }

  for (auto& observer : observers_)
    observer.OnAvailableCamerasChanged(available_cameras_);

  RefreshCameraPreview();
}

void CaptureModeCameraController::RefreshCameraPreview() {
  bool was_visible_before = false;
  aura::Window* old_root = nullptr;
  if (camera_preview_widget_) {
    was_visible_before = camera_preview_widget_->IsVisible();
    old_root = camera_preview_widget_->GetNativeWindow()->GetRootWindow();
  }
  // Trigger a11y alert and update floating windows bounds when camera preview
  // is created or destroyed. The reason to trigger
  // `RunPostRefreshCameraPreview` at the exit of this function is we should
  // wait for camera preview's creation or destruction to be finished.
  base::ScopedClosureRunner deferred_runner(
      base::BindOnce(&CaptureModeCameraController::RunPostRefreshCameraPreview,
                     weak_ptr_factory_.GetWeakPtr(), was_visible_before));

  const CameraInfo* camera_info = nullptr;
  if (selected_camera_.is_valid()) {
    if (camera_info = GetCameraInfoById(selected_camera_, available_cameras_);
        camera_info) {
      if (camera_reconnect_timer_.IsRunning()) {
        const base::TimeDelta remaining_time =
            camera_reconnect_timer_.desired_run_time() - base::TimeTicks::Now();
        const int reconnect_duration_in_seconds =
            (kDisconnectionGracePeriod - remaining_time).InSeconds();
        RecordCameraReconnectDuration(reconnect_duration_in_seconds,
                                      kDisconnectionGracePeriod.InSeconds());
      }
      // When a selected camera becomes available, we stop any grace period
      // timer (if any), and decide whether to show or hide the preview widget
      // based on the current value of `should_show_preview_`.
      camera_reconnect_timer_.Stop();
      if (!should_show_preview_)
        camera_info = nullptr;
    } else {
      // Here the selected camera is disconnected, we'll give it a grace period
      // just in case it may reconnect again (this helps in the case of flaky
      // camera connections).
      camera_reconnect_timer_.Start(
          FROM_HERE, kDisconnectionGracePeriod, this,
          &CaptureModeCameraController::OnSelectedCameraDisconnected);

      if (in_recording_camera_disconnections_)
        ++(*in_recording_camera_disconnections_);
    }
  }

  if (!camera_info) {
    camera_preview_widget_.reset();
    camera_preview_view_ = nullptr;
    if (old_root)
      UpdateFloatingPanelBoundsIfNeeded(old_root);
    return;
  }

  // Destroying the existing camera preview widget before recreating a new one
  // when a different camera was selected.
  if (camera_preview_view_ &&
      camera_preview_view_->camera_id() != selected_camera_) {
    camera_preview_widget_.reset();
    camera_preview_view_ = nullptr;
  }

  DCHECK(!IsCameraDisabledByPolicy());

  if (!camera_preview_widget_) {
    const auto preview_bounds = CalculatePreviewWidgetTargetBounds();
    camera_preview_widget_ = std::make_unique<views::Widget>();
    camera_preview_widget_->Init(CreateWidgetParams(preview_bounds));
    auto* camera_preview_window = camera_preview_widget_->GetNativeWindow();
    camera_preview_window->SetEventTargeter(
        std::make_unique<CameraPreviewTargeter>(camera_preview_window));
    mojo::Remote<video_capture::mojom::VideoSource> camera_video_source;
    video_source_provider_remote_->GetVideoSource(
        camera_info->device_id,
        camera_video_source.BindNewPipeAndPassReceiver());
    camera_preview_view_ = camera_preview_widget_->SetContentsView(
        std::make_unique<CameraPreviewView>(
            this, selected_camera_, preview_bounds.size(),
            std::move(camera_video_source),
            PickSuitableCaptureFormat(preview_bounds.size(),
                                      camera_info->supported_formats)));
    ui::Layer* layer = camera_preview_widget_->GetLayer();
    layer->SetFillsBoundsOpaquely(false);
    layer->SetMasksToBounds(true);
  }

  DCHECK(camera_preview_view_);
  DCHECK_EQ(selected_camera_, camera_preview_view_->camera_id());

  camera_preview_widget_->Show();
}

void CaptureModeCameraController::OnSelectedCameraDisconnected() {
  DCHECK(selected_camera_.is_valid());

  LOG(WARNING)
      << "Selected camera: " << selected_camera_.ToString()
      << " remained disconnected for longer than the grace period. Clearing.";
  SetSelectedCamera(CameraId());
}

gfx::Rect CaptureModeCameraController::CalculatePreviewWidgetTargetBounds() {
  auto* controller = CaptureModeController::Get();
  aura::Window* parent =
      camera_preview_widget_
          ? camera_preview_widget_->GetNativeWindow()->parent()
          : controller->GetCameraPreviewParentWindow();
  DCHECK(parent);
  const gfx::Rect collision_rect_screen =
      GetCollisionAvoidanceRect(parent->GetRootWindow());

  std::vector<CameraPreviewSnapPosition> snap_positions = {
      CameraPreviewSnapPosition::kBottomRight,
      CameraPreviewSnapPosition::kTopRight, CameraPreviewSnapPosition::kTopLeft,
      CameraPreviewSnapPosition::kBottomLeft};

  // Move `camera_preview_snap_position_` to the beginning of `snap_positions`
  // vector, since we should always try the current snap position first.
  base::EraseIf(snap_positions,
                [this](CameraPreviewSnapPosition snap_position) {
                  return snap_position == camera_preview_snap_position_;
                });
  snap_positions.insert(snap_positions.begin(), camera_preview_snap_position_);

  // Cache the current preview bounds and return it directly when we find no
  // other snap position with no collisions.
  gfx::Rect current_preview_bounds;
  for (CameraPreviewSnapPosition snap_position : snap_positions) {
    gfx::Rect preview_bounds =
        GetPreviewWidgetBoundsForSnapPosition(snap_position);
    if (!current_preview_bounds.IsEmpty())
      current_preview_bounds = preview_bounds;

    gfx::Rect preview_bounds_in_screen = preview_bounds;

    // Need to convert preview bounds to screen bounds if it's not since we
    // need to check whether it intersects with system tray bounds in screen.
    // Make sure we use the same coordinate system before we make the
    // comparison.
    if (!parent->GetProperty(wm::kUsesScreenCoordinatesKey))
      wm::ConvertRectToScreen(parent, &preview_bounds_in_screen);

    if (!preview_bounds_in_screen.Intersects(collision_rect_screen)) {
      camera_preview_snap_position_ = snap_position;
      // Notice return `preview_bounds` instead of `preview_bounds_in_screen`,
      // since it's the target bounds for camera preview in its parent's
      // coordinate system.
      return preview_bounds;
    }
  }
  return current_preview_bounds;
}

gfx::Rect CaptureModeCameraController::GetPreviewWidgetBoundsForSnapPosition(
    CameraPreviewSnapPosition snap_position) const {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive() || controller->is_recording_in_progress());
  const gfx::Rect confine_bounds = controller->GetCameraPreviewConfineBounds();

  // If `camera_preview_view_` is available, the preferred size of the
  // `camera_preview_view_` should be used otherwise the initial preview size
  // inferred from the `is_camera_preview_collapsed_` will be used.
  const gfx::Size preview_size =
      camera_preview_view_
          ? camera_preview_view_->GetPreferredSize()
          : GetInitialPreviewSize(is_camera_preview_collapsed_);

  if (confine_bounds.IsEmpty())
    return gfx::Rect(preview_size);

  gfx::Point origin;
  switch (snap_position) {
    case CameraPreviewSnapPosition::kTopLeft:
      origin = confine_bounds.origin();
      origin.Offset(capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                    capture_mode::kSpaceBetweenCameraPreviewAndEdges);
      break;
    case CameraPreviewSnapPosition::kBottomLeft:
      origin = gfx::Point(
          confine_bounds.x() + capture_mode::kSpaceBetweenCameraPreviewAndEdges,
          confine_bounds.bottom() - preview_size.height() -
              capture_mode::kSpaceBetweenCameraPreviewAndEdges);
      break;
    case CameraPreviewSnapPosition::kBottomRight:
      origin = gfx::Point(confine_bounds.right() - preview_size.width() -
                              capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                          confine_bounds.bottom() - preview_size.height() -
                              capture_mode::kSpaceBetweenCameraPreviewAndEdges);
      break;
    case CameraPreviewSnapPosition::kTopRight:
      origin = gfx::Point(confine_bounds.right() - preview_size.width() -
                              capture_mode::kSpaceBetweenCameraPreviewAndEdges,
                          confine_bounds.y() +
                              capture_mode::kSpaceBetweenCameraPreviewAndEdges);
      break;
  }
  return gfx::Rect(origin, preview_size);
}

void CaptureModeCameraController::UpdateSnapPositionOnDragEnded() {
  const gfx::Point center_point_of_preview_widget =
      GetCurrentBoundsMatchingConfineBoundsCoordinates().CenterPoint();
  const gfx::Point center_point_of_confine_bounds =
      CaptureModeController::Get()
          ->GetCameraPreviewConfineBounds()
          .CenterPoint();

  if (center_point_of_preview_widget.x() < center_point_of_confine_bounds.x()) {
    if (center_point_of_preview_widget.y() < center_point_of_confine_bounds.y())
      camera_preview_snap_position_ = CameraPreviewSnapPosition::kTopLeft;
    else
      camera_preview_snap_position_ = CameraPreviewSnapPosition::kBottomLeft;
  } else {
    if (center_point_of_preview_widget.y() < center_point_of_confine_bounds.y())
      camera_preview_snap_position_ = CameraPreviewSnapPosition::kTopRight;
    else
      camera_preview_snap_position_ = CameraPreviewSnapPosition::kBottomRight;
  }
}

gfx::Rect CaptureModeCameraController::
    GetCurrentBoundsMatchingConfineBoundsCoordinates() {
  aura::Window* preview_window = camera_preview_widget_->GetNativeWindow();
  aura::Window* parent = preview_window->parent();
  if (parent->GetProperty(wm::kUsesScreenCoordinatesKey))
    return preview_window->GetBoundsInScreen();
  return preview_window->bounds();
}

void CaptureModeCameraController::RunPostRefreshCameraPreview(
    bool was_preview_visible_before) {
  const bool is_preview_visible_now =
      camera_preview_widget_ && camera_preview_widget_->IsVisible();
  if (was_preview_visible_before != is_preview_visible_now) {
    capture_mode_util::TriggerAccessibilityAlertSoon(
        was_preview_visible_before
            ? IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_HIDDEN
            : IDS_ASH_SCREEN_CAPTURE_CAMERA_PREVIEW_ON);
  }

  if (camera_preview_widget_) {
    UpdateFloatingPanelBoundsIfNeeded(
        camera_preview_widget_->GetNativeWindow()->GetRootWindow());
  }
}

}  // namespace ash
