// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/capture_mode/camera_video_frame_renderer.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/style/icon_button.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace media {
struct VideoCaptureFormat;
}  // namespace media

namespace video_capture::mojom {
class VideoSource;
}  // namespace video_capture::mojom

namespace views {
class NativeViewHost;
}  // namespace views

namespace ash {

class CaptureModeCameraController;
class ScopedA11yOverrideWindowSetter;

// Resize button inside the camera preview view.
class CameraPreviewResizeButton
    : public IconButton,
      public CaptureModeSessionFocusCycler::HighlightableView {
  METADATA_HEADER(CameraPreviewResizeButton, IconButton)

 public:
  CameraPreviewResizeButton(CameraPreviewView* camera_preview_view,
                            views::Button::PressedCallback callback,
                            const gfx::VectorIcon& icon);
  CameraPreviewResizeButton(const CameraPreviewResizeButton&) = delete;
  CameraPreviewResizeButton& operator=(const CameraPreviewResizeButton&) =
      delete;
  ~CameraPreviewResizeButton() override;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;
  void PseudoFocus() override;
  void PseudoBlur() override;

 private:
  const raw_ptr<CameraPreviewView> camera_preview_view_;  // not owned.
};

// A view that acts as the contents view of the camera preview widget. It will
// be responsible for painting the latest camera video frame inside its bounds.
class CameraPreviewView
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView,
      public AccessibilityObserver {
  METADATA_HEADER(CameraPreviewView, views::View)

 public:
  CameraPreviewView(
      CaptureModeCameraController* camera_controller,
      const CameraId& camera_id,
      mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
      const media::VideoCaptureFormat& capture_format,
      bool should_flip_frames_horizontally);
  CameraPreviewView(const CameraPreviewView&) = delete;
  CameraPreviewView& operator=(const CameraPreviewView&) = delete;
  ~CameraPreviewView() override;

  const CameraId& camera_id() const { return camera_id_; }
  CameraPreviewResizeButton* resize_button() const { return resize_button_; }
  bool is_collapsible() const { return is_collapsible_; }
  bool should_flip_frames_horizontally() const {
    return camera_video_renderer_.should_flip_frames_horizontally();
  }

  // Initializes this view to start showing the camera video frames from the
  // associated camera device. Note that this should only be called after this
  // view has been added to a widget (see `AddedToWidget()` below).
  void Initialize();

  // Sets this camera preview collapsability to the given `value`, which will
  // update the resize button visibility.
  void SetIsCollapsible(bool value);

  // Returns true if the `event` has been handled by CameraPrevieView. It
  // happens if it is control+arrow keys, which will be used to move the camera
  // preview to different snap positions.
  bool MaybeHandleKeyEvent(const ui::KeyEvent* event);

  // Called to update visibility of the `resize_button_` when necessary.
  void RefreshResizeButtonVisibility();

  // Updates the a11y override window when focus state changes on the camera
  // preview. The a11y override window will be set to nullptr if neither the
  // camera preview nor the resize button has focus. This is done to make sure
  // a11y focus can be moved to others appropriately.
  void UpdateA11yOverrideWindow();

  // Removes the focus from camera preview when necessary. Happens if mouse
  // pressing outside of the camera preview when it has focus. A11y override
  // window will be updated as well.
  void MaybeBlurFocus(const ui::MouseEvent& event);

  // views::View:
  void AddedToWidget() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Layout(PassKey) override;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;
  std::unique_ptr<views::HighlightPathGenerator> CreatePathGenerator() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;
  void OnAccessibilityControllerShutdown() override;

  base::OneShotTimer* resize_button_hide_timer_for_test() {
    return &resize_button_hide_timer_;
  }

 private:
  friend class CameraPreviewResizeButton;
  friend class CaptureModeTestApi;

  // Called when the resize button is clicked or touched.
  void OnResizeButtonPressed();

  // Updates the icon of the `resize_button_` based on value of
  // `is_camera_preview_collapsed()` inferred from the `camera_controller`.
  void UpdateResizeButton();

  // Updates the tooltip of the `resize_button_`. The `resize_button_` can be an
  // expand button or collapse button.
  void UpdateResizeButtonTooltip();

  // Located events within the bounds of this view should be sent to, and
  // handled by this view only (e.g. for drag and drop). They should not be sent
  // to any native window hosting the camera video frames, otherwise we will
  // lose those events. This function disable event targeting for the
  // `camera_video_host_view_` and all the native windows it is hosting.
  void DisableEventHandlingInCameraVideoHostHierarchy();

  // Fades in or out the `resize_button_` and updates its visibility
  // accordingly.
  void FadeInResizeButton();
  void FadeOutResizeButton();

  // Called when the mouse exits the camera preview, after the latest tap inside
  // the camera preview or when focus being removed from the resize button to
  // start the `resize_button_hide_timer_`.
  void ScheduleRefreshResizeButtonVisibility();

  // Returns the target opacity for resize button.
  float CalculateResizeButtonTargetOpacity();

  // Removes the focus from the camera preview and updates the a11y override
  // window.
  void BlurA11yFocus();

  const raw_ptr<CaptureModeCameraController> camera_controller_;

  // The ID of the camera for which this preview was created.
  const CameraId camera_id_;

  // Renders the camera video frames into its `host_window()`.
  CameraVideoFrameRenderer camera_video_renderer_;

  // The view that hosts the native window `host_window()` of the
  // `camera_video_renderer_` into this view's hierarchy.
  const raw_ptr<views::NativeViewHost> camera_video_host_view_;

  const raw_ptr<CameraPreviewResizeButton> resize_button_;

  // Started when the mouse exits the camera preview or after the latest tap
  // inside the camera preview. Runs RefreshResizeButtonVisibility() to fade out
  // the resize button if possible.
  base::OneShotTimer resize_button_hide_timer_;

  // True if the size of the preview in the expanded state is big enough to
  // allow it to be collapsible.
  bool is_collapsible_ = true;

  // True only while handling a gesture tap event on this view.
  bool has_been_tapped_ = false;

  base::ScopedObservation<AccessibilityController, AccessibilityObserver>
      accessibility_observation_{this};

  // Helps to update the current a11y override window. It will be the native
  // window of the camera preview or nullptr, depends on whether the camera
  // preview has focus or not. Accessibility features will focus on the window
  // after it is set. Once `this` goes out of scope, the a11y override window is
  // set to nullptr.
  std::unique_ptr<ScopedA11yOverrideWindowSetter> scoped_a11y_overrider_;

  base::WeakPtrFactory<CameraPreviewView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
