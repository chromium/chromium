// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class PointF;
class Rect;
class Transform;
}  // namespace gfx

namespace ui {
class ColorProvider;
class Layer;
class LocatedEvent;
}  // namespace ui

namespace views {
class View;
class Widget;
class Label;
class BoxLayout;
}  // namespace views

namespace ash {

class StopRecordingButtonTray;

namespace capture_mode_util {

// Returns true if the capture mode feature is enabled and capture mode is
// active. This method allows callers to avoid including the full header for
// CaptureModeController, which has many transitive includes.
ASH_EXPORT bool IsCaptureModeActive();

// Retrieves the point on the |rect| associated with |position|.
ASH_EXPORT gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                                     FineTunePosition position);

// Return whether |position| is a corner.
bool IsCornerFineTunePosition(FineTunePosition position);

// Returns the stop recording tray button on the given `root`. It may return
// `nullptr` during shutdown or if `root` is destroying.
StopRecordingButtonTray* GetStopRecordingButtonForRoot(aura::Window* root);

// Sets the visibility of the stop-recording button in the Shelf's status area
// widget of the given |root| window.
void SetStopRecordingButtonVisibility(aura::Window* root, bool visible);

// Triggers an accessibility alert to give the user feedback.
void TriggerAccessibilityAlert(const std::string& message);
void TriggerAccessibilityAlert(int message_id);

// Same as TriggerAccessibilityAlert above, but triggers the alert
// asynchronously as soon as possible. This is used to make sure consecutive
// alerts do not override one another, so all of them can be announced by
// ChromeVox.
void TriggerAccessibilityAlertSoon(const std::string& message);
void TriggerAccessibilityAlertSoon(int message_id);

// Returns the next horizontal or vertical snap position based on the current
// camera preview snap position `current` and the movement. Returns `current` if
// the movement is not doable based on current snap position.
CameraPreviewSnapPosition GetCameraNextHorizontalSnapPosition(
    CameraPreviewSnapPosition current,
    bool going_left);
CameraPreviewSnapPosition GetCameraNextVerticalSnapPosition(
    CameraPreviewSnapPosition current,
    bool going_up);

// Notification Utils //
// The notification ID prefix used for notifications corresponding to captured
// images and videos.
constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

// Constants related to the banner view on the image capture notifications.
constexpr int kBannerHeightDip = 36;
constexpr int kBannerHorizontalInsetDip = 12;
constexpr int kBannerVerticalInsetDip = 8;
constexpr int kBannerIconTextSpacingDip = 8;
constexpr int kBannerIconSizeDip = 20;

// Constants related to the play icon view for video capture notifications.
constexpr int kPlayIconSizeDip = 24;
constexpr int kPlayIconBackgroundCornerRadiusDip = 20;
constexpr gfx::Size kPlayIconViewSize{40, 40};

std::unique_ptr<views::View> CreateClipboardShortcutView();
// Creates the banner view that will show on top of the notification image.
std::unique_ptr<views::View> CreateBannerView();

// Creates the play icon view which shows on top of the video thumbnail in the
// notification.
std::unique_ptr<views::View> CreatePlayIconView();

// Returns the local center point of the given `layer`.
gfx::Point GetLocalCenterPoint(ui::Layer* layer);

// Returns a transform that scales the given `layer` by the given `scale` factor
// in both X and Y around its local center point.
gfx::Transform GetScaleTransformAboutCenter(ui::Layer* layer, float scale);

// Defines an object to hold the values of the camera preview size specs.
struct CameraPreviewSizeSpecs {
  // The size to which the camera preview should be set under the current
  // conditions.
  const gfx::Size size;

  // True if the expanded size of the camera preview is big enough to allow it
  // to be collapsible. False otherwise.
  const bool is_collapsible;

  // Whether the camera preview should be hidden or shown. The visibility of the
  // preview can be determined by a number of things, e.g.:
  // - The surface within which the camera preview should be confined is too
  //   small.
  // - We're inside a `kRegion` session and the region is being adjusted or
  //   empty.
  const bool should_be_visible;

  // True if the surface within which the camera preview is confined is too
  // small, and the preview should be hidden.
  const bool is_surface_too_small;
};

// Calculates the size specs of the camera preview which will be confined within
// the given `confine_bounds_size` and will be either expanded or collapsed
// based on the given `is_collapsed`.
ASH_EXPORT CameraPreviewSizeSpecs
CalculateCameraPreviewSizeSpecs(const gfx::Size& confine_bounds_size,
                                bool is_collapsed);

// Gets the top-most window that is capturable under the mouse/touch position.
// The windows that are not capturable include the camera preview widget and
// capture label. There will be a crash if the capture label widget gets picked
// since the snapshot code tries to snap a deleted window.
aura::Window* GetTopMostCapturableWindowAtPoint(const gfx::Point& screen_point);

bool GetWidgetCurrentVisibility(views::Widget* widget);

// Defines an object to hold the animation params used for setting the widget's
// visibility.
struct AnimationParams {
  const base::TimeDelta animation_duration;

  const gfx::Tween::Type tween_type;

  // When it's true, the scale up transform should be applied in the fade in
  // animiation.
  const bool apply_scale_up_animation;
};

// Sets the visibility of the given `widget` to the given `target_visibility`
// with the given `animation_params`, returns true only if the
// `target_visibility` is different than the current.
bool SetWidgetVisibility(views::Widget* widget,
                         bool target_visibility,
                         absl::optional<AnimationParams> animation_params);

// Gets the root window associated with `location_in_screen` if given, otherwise
// gets the root window associated with the `CursorManager`.
aura::Window* GetPreferredRootWindow(
    absl::optional<gfx::Point> location_in_screen = absl::nullopt);

// Configures style for the `label_view` in the settings menu.
void ConfigLabelView(views::Label* label_view);

// Initializes the box layout for the `view` in the settings menu.
views::BoxLayout* CreateAndInitBoxLayoutForView(views::View* view);

// Gets the notification ID of a screen capture given its filepath.
ASH_EXPORT std::string GetScreenCaptureNotificationIdForPath(
    const base::FilePath& path);

// If the privacy indicators feature is enabled, the below functions update the
// camera and microphone capture mode indicators according to the given values.
void MaybeUpdateCameraPrivacyIndicator(bool camera_on);
void MaybeUpdateMicrophonePrivacyIndicator(bool mic_on);

ui::ColorProvider* GetColorProviderForNativeTheme();

// Returns true if the given located `event` is targeted on a window that is a
// descendant of the given `widget`. Note that `widget` can be provided as null
// if it no longer exists, in this case this function returns false.
bool IsEventTargetedOnWidget(const ui::LocatedEvent& event,
                             views::Widget* widget);

// Calculates the highlight layer bounds based on `center_point` which is in the
// coordinates of the window being recorded.
ASH_EXPORT gfx::Rect CalculateHighlightLayerBounds(
    const gfx::PointF& center_point,
    int highlight_layer_radius);

}  // namespace capture_mode_util

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
