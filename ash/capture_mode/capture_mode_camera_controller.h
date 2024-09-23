// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/system/system_monitor.h"
#include "base/timer/timer.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class CameraPreviewView;
class CaptureModeBehavior;
class CaptureModeDelegate;

// The ID used internally in capture mode to identify the camera.
class ASH_EXPORT CameraId {
 public:
  CameraId() = default;
  CameraId(std::string model_id, int number);
  CameraId(const CameraId&) = default;
  CameraId(CameraId&&) = default;
  CameraId& operator=(const CameraId&) = default;
  CameraId& operator=(CameraId&&) = default;
  ~CameraId() = default;

  bool is_valid() const { return !model_id_or_display_name_.empty(); }
  const std::string& model_id_or_display_name() const {
    return model_id_or_display_name_;
  }
  int number() const { return number_; }

  bool operator==(const CameraId& rhs) const {
    return model_id_or_display_name_ == rhs.model_id_or_display_name_ &&
           number_ == rhs.number_;
  }
  bool operator!=(const CameraId& rhs) const { return !(*this == rhs); }

  bool operator<(const CameraId& rhs) const;

  std::string ToString() const;

 private:
  // A unique hardware ID of the camera device in the form of
  // "[Vendor ID]:[Product ID]" (e.g. "0c45:6713"). Note that if multiple
  // cameras from the same vendor and of the same model are connected to the
  // device, they will all have the same `model_id`.
  // Note that in some cases, `media::VideoCaptureDeviceDescriptor::model_id`
  // may not be present. In this case, this will be filled by the camera's
  // display name.
  std::string model_id_or_display_name_;

  // A number that disambiguates cameras of the same type. For example if we
  // have two connected cameras of the same type, the first one will have
  // `number` set to 1, and the second's will be 2.
  int number_ = 0;
};

struct CameraInfo {
  CameraInfo(CameraId camera_id,
             std::string device_id,
             std::string display_name,
             const media::VideoCaptureFormats& supported_formats,
             media::VideoFacingMode camera_facing_mode);
  CameraInfo(CameraInfo&&);
  CameraInfo& operator=(CameraInfo&&);
  ~CameraInfo();

  // The ID used to identify the camera device internally to the capture mode
  // code, which should be more stable than the below `device_id` which may
  // change multiple times for the same camera.
  CameraId camera_id;

  // The ID of the camera device given to it by the system in its current
  // connection instance (e.g. "/dev/video2"). Note that the same camera device
  // can disconnect and reconnect with a different `device_id` (e.g. when the
  // cable is flaky). This ID is used to identify the camera to the video source
  // provider in the video capture service.
  std::string device_id;

  // The name of the camera device as shown to the end user (e.g. "Integrated
  // Webcam").
  std::string display_name;

  // A list of supported capture formats by this camera. This list is sorted
  // (See `media::VideoCaptureSystemImpl::DevicesInfoReady()`) by the frame size
  // area, then by frame width, then by the *largest* frame rate.
  media::VideoCaptureFormats supported_formats;

  // Whether the camera is facing the user (e.g. for internal front cameras), or
  // the environment (e.g. internal rear cameras), or unknown (e.g. usually for
  // external USB cameras).
  media::VideoFacingMode camera_facing_mode;
};

using CameraInfoList = std::vector<CameraInfo>;

// Controls detecting camera devices additions and removals and keeping a list
// of all currently connected cameras to the device. It also tracks all the
// capture mode selfie camera settings.
class ASH_EXPORT CaptureModeCameraController
    : public base::SystemMonitor::DevicesChangedObserver,
      public SystemTrayObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called to notify the observer that the list of `available_cameras_` has
    // changed, and provides that list as `cameras`.
    virtual void OnAvailableCamerasChanged(const CameraInfoList& cameras) = 0;

    // Called to notify the observer that a camera with `camera_id` was selected
    // and will be used to show a camera preview when possible.
    // Note that when `camera_id.is_valid()` is false, it means no camera is
    // currently selected.
    virtual void OnSelectedCameraChanged(const CameraId& camera_id) = 0;

   protected:
    ~Observer() override = default;
  };

  explicit CaptureModeCameraController(CaptureModeDelegate* delegate);
  CaptureModeCameraController(const CaptureModeCameraController&) = delete;
  CaptureModeCameraController& operator=(const CaptureModeCameraController&) =
      delete;
  ~CaptureModeCameraController() override;

  const CameraInfoList& available_cameras() const { return available_cameras_; }
  const CameraId& selected_camera() const { return selected_camera_; }
  views::Widget* camera_preview_widget() const {
    return camera_preview_widget_.get();
  }
  CameraPreviewView* camera_preview_view() const {
    return camera_preview_view_;
  }
  bool should_show_preview() const { return should_show_preview_; }
  CameraPreviewSnapPosition camera_preview_snap_position() const {
    return camera_preview_snap_position_;
  }
  bool is_drag_in_progress() const { return is_drag_in_progress_; }
  bool is_camera_preview_collapsed() const {
    return is_camera_preview_collapsed_;
  }
  bool did_user_ever_change_camera() const {
    return did_user_ever_change_camera_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Selects the first camera in the `available_cameras_` list (if any), and
  // only if no other camera is already selected.
  void MaybeSelectFirstCamera();

  // Reverts the automatic selection of the first available camera if one was
  // made by calling the `MaybeSelectFirstCamera()`.
  void MaybeRevertAutoCameraSelection();

  // Returns true if camera support is disabled by admins via
  // the `SystemFeaturesDisableList` policy, false otherwise.
  bool IsCameraDisabledByPolicy() const;

  // Returns the display name of `selected_camera_`. Returns an empty string if
  // the selected camera is not set.
  std::string GetDisplayNameOfSelectedCamera() const;

  // Sets the currently selected camera to the whose ID is the given
  // `camera_id`. If `camera_id` is invalid (see CameraId::is_valid()), this
  // clears the selected camera. `by_user` is true if the selection was made
  // explicitly by the user, false otherwise.
  void SetSelectedCamera(CameraId camera_id, bool by_user = false);

  // Sets `should_show_preview_` to the given `value`, and refreshes the state
  // of the camera preview.
  void SetShouldShowPreview(bool value);

  // Updates the parent of the `camera_preview_widget_` when necessary. E.g,
  // capture source type changes, selected recording window changes etc.
  void MaybeReparentPreviewWidget();

  // Sets `camera_preview_snap_position_` and updates the preview widget's
  // bounds accordingly. If `animate` is set to true, the camera preview will
  // animate to its new snap position.
  void SetCameraPreviewSnapPosition(CameraPreviewSnapPosition value,
                                    bool animate = false);

  // Updates the bounds and visibility of `camera_preview_widget_` according to
  // the current state of the capture surface within which the camera preview
  // is confined and snapped to one of its corners. If `animate` is set to true,
  // the widget will animate to the new target bounds.
  void MaybeUpdatePreviewWidget(bool animate = false);

  // Handles drag events forwarded from `camera_preview_view_`.
  void StartDraggingPreview(const gfx::PointF& screen_location);
  void ContinueDraggingPreview(const gfx::PointF& screen_location);
  void EndDraggingPreview(const gfx::PointF& screen_location, bool is_touch);

  // Updates the bounds of the preview widget and the value of
  // `is_camera_preview_collapsed_` when the resize button is pressed.
  void ToggleCameraPreviewSize();

  // Called when a capture session gets started so we can refresh the cameras
  // list, since the cros-camera service might have not been running when we
  // tried to refresh the cameras at the beginning. (See
  // http://b/230917107#comment12 for more details).
  void OnCaptureSessionStarted();

  void OnRecordingStarted(const CaptureModeBehavior* active_behavior);
  void OnRecordingEnded();

  // Called when the `CameraVideoFrameHandler` of the current
  // `camera_preview_widget_` encounters a fatal error. This is considered a
  // camera disconnection, and sometimes doesn't get reported via
  // `OnDevicesChanged()` below, or may get delayed a lot. We manually remove
  // the current camera from `available_cameras_`, delete its preview, and
  // request a new list of cameras from the video capture service.
  // https://crbug/1316230.
  void OnFrameHandlerFatalError();

  // Called when the device is shutting down. After this call, we don't do any
  // operations that interacts with the video capture service.
  void OnShuttingDown();

  // As `camera_preview_view_` is a
  // CaptureModeSessionFocusCycler::HighlightableView. This will show the focus
  // ring and trigger setting a11y focus on the camera preview. Note, this is
  // only for focusing the preview while recording is in progress.
  void PseudoFocusCameraPreview();

  void OnActiveUserSessionChanged();

  // base::SystemMonitor::DevicesChangedObserver:
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  // SystemTrayObserver:
  void OnSystemTrayBubbleShown() override;
  void OnFocusLeavingSystemTray(bool reverse) override {}
  void OnStatusAreaAnchoredBubbleVisibilityChanged(TrayBubbleView* tray_bubble,
                                                   bool visible) override;

  void SetOnCameraListReceivedForTesting(base::OnceClosure callback) {
    on_camera_list_received_for_test_ = std::move(callback);
  }

  base::OneShotTimer* camera_reconnect_timer_for_test() {
    return &camera_reconnect_timer_;
  }

 private:
  friend class CaptureModeTestApi;

  // Called to connect to the video capture services's video source provider for
  // the first time, or when the connection to it is lost. It also queries the
  // list of currently available cameras by calling the below
  // GetCameraDevices().
  void ReconnectToVideoSourceProvider();

  // Retrieves the list of currently available cameras from the video source
  // provider.
  void GetCameraDevices();

  // Called back asynchronously by the video source provider to give us the list
  // of currently available camera `devices`. The ID used to make the request to
  // which this reply belongs is `request_id`. We will ignore any replies for
  // any older requests than the `most_recent_request_id_`.
  using RequestId = size_t;
  void OnCameraDevicesReceived(
      RequestId request_id,
      video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Shows or hides a preview of the currently selected camera depending on
  // whether it's currently allowed and whether one is currently selected.
  void RefreshCameraPreview();

  // Triggered when the `camera_reconnect_timer_` fires, indicating that a
  // previously `selected_camera_` remained disconnected for longer than the
  // allowed grace period, and therefore it will be cleared.
  void OnSelectedCameraDisconnected();

  // Returns the bounds of the preview widget which doesn't intersect with
  // system tray, which should be confined within the given `confine_bounds`,
  // and have the given `preview_size`. Always tries the current
  // `camera_preview_snap_position_` first. Once a snap position with which the
  // preview has no collisions is found, it will be set in
  // `camera_preview_snap_position_`. If the camera preview at all possible snap
  // positions intersects with system tray, returns the bounds for the current
  // `camera_preview_snap_position_`.
  gfx::Rect CalculatePreviewWidgetTargetBounds(const gfx::Rect& confine_bounds,
                                               const gfx::Size& preview_size);

  // Called by `CalculatePreviewWidgetTargetBounds` above. Returns the bounds of
  // the preview widget that matches the coordinate system of the given
  // `confine_bounds` with the given `preview_size` at the given
  // `snap_position`.
  gfx::Rect GetPreviewWidgetBoundsForSnapPosition(
      const gfx::Rect& confine_bounds,
      const gfx::Size& preview_size,
      CameraPreviewSnapPosition snap_position) const;

  // Returns the new snap position of the camera preview on drag ended.
  CameraPreviewSnapPosition CalculateSnapPositionOnDragEnded() const;

  // Returns the current bounds of camemra preview widget that match the
  // coordinate system of the confine bounds.
  gfx::Rect GetCurrentBoundsMatchingConfineBoundsCoordinates() const;

  // Does post works for camera preview after RefreshCameraPreview(). It
  // triggers a11y alert based on `was_preview_visible_before` and the current
  // visibility of `camera_preview_widget_`. `was_preview_visible_before` is the
  // visibility of the camera preview when RefreshCameraPreview() was called.
  // It also triggers floating windows bounds update to avoid overlap between
  // camera preview and floating windows, such as PIP windows and some a11y
  // panels.
  void RunPostRefreshCameraPreview(bool was_preview_visible_before);

  // Sets the given `target_bounds` on the camera preview widget, potentially
  // animating to it if `animate` is true. Returns true if the bounds actually
  // changed from the current.
  bool SetCameraPreviewBounds(const gfx::Rect& target_bounds, bool animate);

  // Owned by CaptureModeController and guaranteed to be not null and to outlive
  // `this`.
  const raw_ptr<CaptureModeDelegate> delegate_;

  // The remote end to the video source provider that exists in the video
  // capture service.
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;

  CameraInfoList available_cameras_;

  // The currently selected camera. If its `is_valid()` is false, then no camera
  // is currently selected.
  CameraId selected_camera_;

  base::ObserverList<Observer> observers_;

  // If bound, will be invoked at the end of the scope of
  // `OnCameraDevicesReceived()` regardless of whether there was a change in the
  // available cameras or not, which is different from the behavior of
  // `Observer::OnAvailableCamerasChanged()` which is called only when there is
  // a change.
  base::OnceClosure on_camera_list_received_for_test_;

  // The camera preview widget and its contents view.
  views::UniqueWidgetPtr camera_preview_widget_;
  raw_ptr<CameraPreviewView> camera_preview_view_ = nullptr;

  // A timer used to give a `selected_camera_` that got disconnected a grace
  // period, so if it reconnects again within this period, its ID is kept around
  // in `selected_camera_`, otherwise the ID is cleared, effectively resetting
  // back the camera setting to "Off".
  base::OneShotTimer camera_reconnect_timer_;

  // Set to true when a preview of the currently selected camera (if any) should
  // be shown. This happens when CaptureModeSession is started and switched to
  // a video recording mode before recording starts. It is reset back to false
  // when:
  // - Video recording ends.
  // - The selected camera is disconnected for longer than a grace period during
  //   recording.
  // - The capture mode session ends without starting any recording.
  // - The capture mode session is switched to an image capture mode.
  bool should_show_preview_ = false;

  // The ID used for the most recent request made to the video source provider
  // to get the list of cameras in GetCameraDevices(). More recent requests will
  // have a larger value IDs than older requests.
  RequestId most_recent_request_id_ = 0;

  CameraPreviewSnapPosition camera_preview_snap_position_ =
      CameraPreviewSnapPosition::kBottomRight;

  // The location of the previous drag event in screen coordinate.
  gfx::PointF previous_location_in_screen_;

  // True when the dragging for `camera_preview_view_` is in progress.
  bool is_drag_in_progress_ = false;

  // True if the camera preview is collapsed. Its value will be updated when
  // the resize button is clicked. The size of the preview widget and the icon
  // of the resize button will be updated based on it.
  bool is_camera_preview_collapsed_ = false;

  // True if it's the first time to update the camera preview's bounds after
  // it's created.
  bool is_first_bounds_update_ = false;

  // True when the device is shutting down, and we should no longer make any
  // requests to the video capture service.
  bool is_shutting_down_ = false;

  // Valid only during recording to track the number of camera disconnections
  // while recording is in progress.
  std::optional<int> in_recording_camera_disconnections_;

  // Will be set to true the first time the number of connected cameras is
  // reported.
  bool did_report_number_of_cameras_before_ = false;

  // Will be set to true the first user logs in. And we should only request the
  // camera devices after the first user logs in.
  bool did_first_user_login_ = false;

  // True if the first available camera was auto-selected by calling
  // `MaybeSelectFirstCamera()`, false otherwise or if
  // `MaybeRevertAutoCameraSelection()` was called to revert back this automatic
  // selection.
  bool did_make_camera_auto_selection_ = false;

  // True if the user ever made an explicit camera selection (i.e. from the
  // capture mode settings menu).
  bool did_user_ever_change_camera_ = false;

  base::WeakPtrFactory<CaptureModeCameraController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_CONTROLLER_H_
