// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_controller.h"

#include <algorithm>
#include <cstring>

#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_properties.h"

namespace ash {

namespace {

// The maximum amount of time we allow a `selected_camera_` to remain
// disconnected before we consider it gone forever, and we clear its ID from
// `selected_camera_`.
constexpr base::TimeDelta kDisconnectionGracePeriod = base::Seconds(10);

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

// Returns the CameraInfo item in `list` whose ID is equal to the given `id`, or
// nullptr if no such item exists.
const CameraInfo* GetCameraInfoById(const CameraId& id,
                                    const CameraInfoList& list) {
  const auto iter = std::find_if(
      list.begin(), list.end(),
      [&id](const CameraInfo& info) { return info.camera_id == id; });
  return iter == list.end() ? nullptr : &(*iter);
}

// Stacking the camera preview window on top of all children of its parent so
// that it can show up in the recording above everything else.
void StackingPreviewAtTop(views::Widget* preview_widget) {
  DCHECK(preview_widget);
  auto* preview_window = preview_widget->GetNativeWindow();
  auto* parent = preview_window->parent();
  DCHECK(parent);
  parent->StackChildAtTop(preview_window);
}

std::unique_ptr<views::Widget> CreateCameraPreviewWidget(
    const gfx::Rect& bounds) {
  auto camera_preview_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent = CaptureModeController::Get()->GetCameraPreviewParentWindow();
  params.bounds = bounds;
  // Need to set `params.child` to true here, otherwise camera preview widget
  // will be added as a transient child to `params.parent`. For more details,
  // please check `NativeWidgetAura::InitNativeWidget`.
  params.child = true;
  params.name = "CameraPreviewWidget";
  camera_preview_widget->Init(std::move(params));
  StackingPreviewAtTop(camera_preview_widget.get());
  return camera_preview_widget;
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
}

CaptureModeCameraController::~CaptureModeCameraController() {
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void CaptureModeCameraController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CaptureModeCameraController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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
  if (selected_camera_ == camera_id)
    return;

  selected_camera_ = std::move(camera_id);
  camera_reconnect_timer_.Stop();
  camera_preview_widget_.reset();
  camera_preview_view_ = nullptr;

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

  auto* controller = CaptureModeController::Get();
  DCHECK(!controller->is_recording_in_progress());
  auto* parent = controller->GetCameraPreviewParentWindow();
  DCHECK(parent);
  auto* native_window = camera_preview_widget_->GetNativeWindow();
  if (parent != native_window->parent()) {
    views::Widget::ReparentNativeView(native_window, parent);
    StackingPreviewAtTop(camera_preview_widget_.get());
  }
  MaybeUpdatePreviewWidgetBounds();
}

void CaptureModeCameraController::SetCameraPreviewSnapPosition(
    CameraPreviewSnapPosition value) {
  if (camera_preview_snap_position_ == value)
    return;

  camera_preview_snap_position_ = value;
  MaybeUpdatePreviewWidgetBounds();
}

void CaptureModeCameraController::MaybeUpdatePreviewWidgetBounds() {
  if (!camera_preview_widget_)
    return;

  // The widget will be hidden if being parented to
  // `kShellWindowId_UnparentedContainer`, we do not need to update its bounds
  // in this case.
  if (camera_preview_widget_->GetNativeWindow()->parent()->GetId() ==
      kShellWindowId_UnparentedContainer) {
    return;
  }

  camera_preview_widget_->SetBounds(GetPreviewWidgetBounds());
}

void CaptureModeCameraController::StartDraggingPreview(
    const gfx::PointF& screen_location) {
  previous_location_in_screen_ = screen_location;

  is_drag_in_progress_ = true;
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
  UpdateSnapPostionOnDragEnded();

  const gfx::Rect current_bounds =
      GetCurrentBoundsMatchingConfineBoundsCoordinates();
  const gfx::Rect target_bounds = GetPreviewWidgetBounds();
  camera_preview_widget_->SetBounds(target_bounds);
  gfx::Transform transform;
  transform.Translate(current_bounds.CenterPoint() -
                      target_bounds.CenterPoint());
  ui::Layer* layer = camera_preview_widget_->GetLayer();
  layer->SetTransform(transform);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(120))
      .SetTransform(layer, gfx::Transform());

  is_drag_in_progress_ = false;
  // Disable cursor compositing at the end of the drag.
  Shell::Get()->UpdateCursorCompositingEnabled();

  // Make sure cursor is updated correctly after camera preview is snapped.
  if (CaptureModeController::Get()->IsActive()) {
    CaptureModeController::Get()->capture_mode_session()->UpdateCursor(
        gfx::ToRoundedPoint(screen_location), is_touch);
  }
}

void CaptureModeCameraController::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE)
    GetCameraDevices();
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
  bool create_or_keep_widget = false;
  if (selected_camera_.is_valid()) {
    if (const CameraInfo* camera_info =
            GetCameraInfoById(selected_camera_, available_cameras_);
        camera_info) {
      // When a selected camera becomes available, we stop any grace period
      // timer (if any), and decide whether to show or hide the preview widget
      // based on the current value of `should_show_preview_`.
      camera_reconnect_timer_.Stop();
      create_or_keep_widget = should_show_preview_;
    } else {
      // Here the selected camera is disconnected, we'll give it a grace period
      // just in case it may reconnect again (this helps in the case of flaky
      // camera connections).
      camera_reconnect_timer_.Start(
          FROM_HERE, kDisconnectionGracePeriod, this,
          &CaptureModeCameraController::OnSelectedCameraDisconnected);

      // TODO(afakhry): Clear the camera immediately if it gets disconnected
      // during count down and before recording starts.
    }
  }

  if (!create_or_keep_widget) {
    camera_preview_widget_.reset();
    camera_preview_view_ = nullptr;
    return;
  }

  if (!camera_preview_widget_) {
    camera_preview_widget_ =
        CreateCameraPreviewWidget(GetPreviewWidgetBounds());
    camera_preview_view_ = camera_preview_widget_->SetContentsView(
        std::make_unique<CameraPreviewView>(this));
  }
  camera_preview_widget_->Show();
}

void CaptureModeCameraController::OnSelectedCameraDisconnected() {
  DCHECK(selected_camera_.is_valid());

  LOG(WARNING)
      << "Selected camera: " << selected_camera_.ToString()
      << " remained disconnected for longer than the grace period. Clearing.";
  SetSelectedCamera(CameraId());
}

gfx::Rect CaptureModeCameraController::GetPreviewWidgetBounds() const {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive() || controller->is_recording_in_progress());
  const gfx::Rect confine_bounds = controller->GetCameraPreviewConfineBounds();
  const gfx::Size preview_size = camera_preview_view_
                                     ? camera_preview_view_->GetPreferredSize()
                                     : capture_mode::kCameraPreviewSize;
  if (confine_bounds.IsEmpty())
    return gfx::Rect(preview_size);

  gfx::Point origin;
  switch (camera_preview_snap_position_) {
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

void CaptureModeCameraController::UpdateSnapPostionOnDragEnded() {
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

}  // namespace ash
