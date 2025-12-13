// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/capture_mode/action_button_container_view.h"
#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/capture_region_overlay_controller.h"
#include "ash/capture_mode/capture_window_observer.h"
#include "ash/capture_mode/disclaimer_view.h"
#include "ash/capture_mode/folder_selection_dialog_controller.h"
#include "ash/capture_mode/normal_capture_bar_view.h"
#include "ash/capture_mode/recording_type_menu_view.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/capture_mode/user_nudge_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/url_constants.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/scanner/scanner_disclaimer.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/tab_slider_button.h"
#include "ash/utility/cursor_setter.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_dimmer.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_util.h"

namespace ash {

namespace {

// The visual radius of the drag affordance circles which are shown while
// resizing a drag region.
constexpr int kAffordanceCircleRadiusDp = 4;

// The hit radius of the drag affordance circles touch events.
constexpr int kAffordanceCircleTouchHitRadiusDp = 16;

// Capture region magnifier parameters.
constexpr MagnifierGlass::Params kMagnifierParams{
    /*scale=*/2.f,
    /*radius=*/60,
    /*border_size=*/2,
    /*border_outline_thickness=*/0,
    /*border_color=*/SK_ColorWHITE,
    /*border_outline_color=*/SK_ColorTRANSPARENT,
    /*bottom_shadow=*/
    gfx::ShadowValue(gfx::Vector2d(0, 1),
                     2,
                     SkColorSetARGB(0x4C, 0x00, 0x00, 0x00)),
    /*top_shadow=*/
    gfx::ShadowValue(gfx::Vector2d(0, 1),
                     3,
                     SkColorSetARGB(0x26, 0x00, 0x00, 0x00))};

constexpr int kSizeLabelBorderRadius = 4;

constexpr int kSizeLabelHorizontalPadding = 8;

// Values for the shadows of the capture region components.
constexpr int kRegionAffordanceCircleShadow2Blur = 6;
constexpr gfx::ShadowValue kRegionOutlineShadow(gfx::Vector2d(0, 0),
                                                2,
                                                SkColorSetARGB(41, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow1(
    gfx::Vector2d(0, 1),
    2,
    SkColorSetARGB(76, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow2(
    gfx::Vector2d(0, 2),
    kRegionAffordanceCircleShadow2Blur,
    SkColorSetARGB(38, 0, 0, 0));

// Values of the focus ring draw around the region or affordance circles.
constexpr int kFocusRingStrokeWidthDp = 2;
constexpr int kFocusRingSpacingDp = 2;

// When updating the capture region, request a repaint on the region and inset
// such that the border, affordance circles and affordance circle shadows are
// all repainted as well.
constexpr int kDamageOutsetDp = capture_mode::kCaptureRegionBorderStrokePx +
                                kAffordanceCircleRadiusDp +
                                kRegionAffordanceCircleShadow2Blur;

// The damage outset from the edge of the capture region to repaint when there
// is a glow animation.
constexpr int kDamageWithGlowOutsetDp =
    capture_mode::kRegionGlowMaxOutsetDp +
    2 * static_cast<int>(capture_mode::kRegionGlowAnimationMaxBlurDp);

// The minimum padding on each side of the capture region. If the capture button
// cannot be placed in the center of the capture region and maintain this
// padding, it will be placed below or above the capture region.
constexpr int kCaptureRegionMinimumPaddingDp = 16;

// Animation parameters needed when countdown starts.
// The animation duration that the label fades out and scales down before count
// down starts.
constexpr base::TimeDelta kCaptureLabelCountdownStartDuration =
    base::Milliseconds(267);
// The animation duration that the capture widgets (capture bar, capture
// settings) fade out before count down starts.
constexpr base::TimeDelta kCaptureWidgetFadeOutDuration =
    base::Milliseconds(167);
// The animation duration that the fullscreen shield fades out before count down
// starts.
constexpr base::TimeDelta kCaptureShieldFadeOutDuration =
    base::Milliseconds(333);
// If there is no text message was showing when count down starts, the label
// widget will shrink down from 120% -> 100% and fade in.
constexpr float kLabelScaleUpOnCountdown = 1.2;

// The animation duration that the label fades out and scales up when going from
// the selection phase to the fine tune phase.
constexpr base::TimeDelta kCaptureLabelRegionPhaseChangeDuration =
    base::Milliseconds(167);
// The delay before the label fades out and scales up.
constexpr base::TimeDelta kCaptureLabelRegionPhaseChangeDelay =
    base::Milliseconds(67);
// When going from the select region phase to the fine tune phase, the label
// widget will scale up from 80% -> 100%.
constexpr float kLabelScaleDownOnPhaseChange = 0.8;

// If the user is using keyboard only and they are on the selecting region
// phase, they can create default region which is centered and sized to this
// value times the root window's width and height.
constexpr float kRegionDefaultRatio = 0.24f;

// The radius of the painted capture region when in sunfish mode.
constexpr int kSunfishModeCaptureRegionRadiusDp = 16;

// The width of the border around the capture region when selecting or adjusting
// a region in sunfish mode.
constexpr int kSunfishModeCaptureRegionBorderWidthDp = 6;

// The radius of the focus ring drawn around the whole capture region in sunfish
// mode.
constexpr int kSunfishRegionFocusRingRadiusDp = 18;

// The radius of the focus ring drawn around a corner handle of the capture
// region in sunfish mode.
constexpr int kSunfishRegionCornerFocusRingRadiusDp = 16;

// The offset between a corner of a sunfish capture region and the center of the
// the corresponding corner handle. This is used to move the focus ring and hit
// test rect slightly inwards towards the capture region, to center it around
// the rounded corner.
constexpr int kSunfishRegionRoundedCornerOffsetDp =
    kSunfishModeCaptureRegionRadiusDp / 2;

// The damage outset from the edge of the capture region to repaint when
// updating the capture region in sunfish mode. The value here is just a
// heuristic but should be enough to cover all relevant UI around the capture
// region.
constexpr int kSunfishRegionDamageOutsetDp =
    kSunfishModeCaptureRegionBorderWidthDp +
    kSunfishRegionCornerFocusRingRadiusDp;

// The animation duration for fading in Scanner action buttons.
constexpr base::TimeDelta kScannerActionButtonFadeInDuration =
    base::Milliseconds(100);

// The delay before sending out an image search request after a capture region
// is adjusted using keyboard events. This is to prevent too many requests if
// the user needs to repeatedly adjust the capture region. Note that this delay
// does not apply to region adjustments that end in a mouse release event.
constexpr base::TimeDelta kImageSearchRequestStartDelay = base::Seconds(1);

// The preferred alignment of a widget positioned near the capture region.
enum class CaptureRegionWidgetAlignment {
  // At the center of the capture region.
  kCenter,
  // Right-aligned with the capture region (i.e. the right edges of the widget
  // and capture region align). We prefer to right-align the widget below the
  // capture region if possible, otherwise will try to right-align above the
  // capture region.
  kRight,
};

// Mouse cursor warping is disabled when the capture source is a custom region.
// Sets the mouse warp status to |enable| and return the original value.
bool SetMouseWarpEnabled(bool enable) {
  auto* mouse_cursor_filter = Shell::Get()->mouse_cursor_filter();
  const bool old_value = mouse_cursor_filter->mouse_warp_enabled();
  mouse_cursor_filter->set_mouse_warp_enabled(enable);
  return old_value;
}

// Returns the smallest rect that contains all of `points` if they are all
// within `root`'s bounds, otherwise constrains the rect to fit.
gfx::Rect GetRectEnclosingPoints(const std::vector<gfx::Point>& points,
                                 aura::Window* root) {
  DCHECK_GE(points.size(), 2u);
  CHECK(root);

  int x = INT_MAX;
  int y = INT_MAX;
  int right = INT_MIN;
  int bottom = INT_MIN;
  for (const gfx::Point& point : points) {
    x = std::min(point.x(), x);
    y = std::min(point.y(), y);
    right = std::max(point.x(), right);
    bottom = std::max(point.y(), bottom);
  }

  gfx::Rect new_rect(x, y, right - x, bottom - y);
  new_rect.Intersect(root->bounds());
  return new_rect;
}

// Returns the widget init params needed to create a widget associated with a
// capture session.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent,
                                             const gfx::Rect& bounds,
                                             const std::string& name) {
  // Use a popup widget to get transient properties, such as not needing to
  // click on the widget first to get capture before receiving events.
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = bounds;
  params.name = name;
  return params;
}

// In fullscreen or window capture mode, the mouse will change to a camera
// image icon if we're capturing image, or a video record image icon if we're
// capturing video.
ui::Cursor GetCursorForFullscreenOrWindowCapture(bool capture_image) {
  const display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(
          capture_mode_util::GetPreferredRootWindow());
  const float device_scale_factor = display.device_scale_factor();
  const gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          capture_image ? IDR_CAPTURE_IMAGE_CURSOR : IDR_CAPTURE_VIDEO_CURSOR);
  SkBitmap bitmap = *icon->bitmap();
  gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
  wm::ScaleAndRotateCursorBitmapAndHotpoint(
      device_scale_factor, display.panel_rotation(), &bitmap, &hotspot);
  ui::Cursor cursor = ui::Cursor::NewCustom(
      std::move(bitmap), std::move(hotspot), device_scale_factor);
  cursor.SetPlatformCursor(ui::CursorFactory::GetInstance()->CreateImageCursor(
      cursor.type(), cursor.custom_bitmap(), cursor.custom_hotspot(),
      cursor.image_scale_factor()));

  return cursor;
}

// Returns the expected cursor type for |position| in region capture.
ui::mojom::CursorType GetCursorTypeForFineTunePosition(
    FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeftVertex:
      return ui::mojom::CursorType::kNorthWestResize;
    case FineTunePosition::kBottomRightVertex:
      return ui::mojom::CursorType::kSouthEastResize;
    case FineTunePosition::kTopEdge:
    case FineTunePosition::kBottomEdge:
      return ui::mojom::CursorType::kNorthSouthResize;
    case FineTunePosition::kTopRightVertex:
      return ui::mojom::CursorType::kNorthEastResize;
    case FineTunePosition::kBottomLeftVertex:
      return ui::mojom::CursorType::kSouthWestResize;
    case FineTunePosition::kLeftEdge:
    case FineTunePosition::kRightEdge:
      return ui::mojom::CursorType::kEastWestResize;
    case FineTunePosition::kCenter:
      return ui::mojom::CursorType::kMove;
    default:
      return ui::mojom::CursorType::kCell;
  }
}

// Gets the amount of change that should happen to a region given |event_flags|.
int GetArrowKeyPressChange(int event_flags) {
  if ((event_flags & ui::EF_SHIFT_DOWN) != 0)
    return capture_mode::kShiftArrowKeyboardRegionChangeDp;
  if ((event_flags & ui::EF_CONTROL_DOWN) != 0)
    return capture_mode::kCtrlArrowKeyboardRegionChangeDp;
  return capture_mode::kArrowKeyboardRegionChangeDp;
}

void UpdateFloatingPanelBoundsIfNeeded() {
  Shell::Get()->accessibility_controller()->UpdateFloatingPanelBoundsIfNeeded();
}

views::Widget* GetCameraPreviewWidget() {
  return CaptureModeController::Get()
      ->camera_controller()
      ->camera_preview_widget();
}

bool ShouldPassEventToCameraPreview(ui::LocatedEvent* event) {
  auto* controller = CaptureModeController::Get();

  // If there's a video recording in progress, return false immediately, since
  // even camera preview exists, it doesn't belong to the current capture
  // session.
  if (controller->is_recording_in_progress())
    return false;

  auto* camera_preview_widget = GetCameraPreviewWidget();
  if (!camera_preview_widget || !camera_preview_widget->IsVisible())
    return false;

  if (controller->camera_controller()->is_drag_in_progress())
    return true;

  // If the event is targeted on the camera preview, even it's not located
  // on the camera preview, we should still pass the event to camera preview
  // to handle it. For example, when pressing on the resize button inside camera
  // preview, but release the press outside of camera preview, even the release
  // event is not on the camera preview, we should still pass the event to it,
  // otherwise camera preview will wait for the release event forever which will
  // make the regular drag for camera preview not work.
  auto* target = static_cast<aura::Window*>(event->target());
  if (camera_preview_widget->GetNativeWindow()->Contains(target))
    return true;

  return false;
}

// Returns true if the given `widget` intersects with the camera preview.
// Otherwise, returns false;
bool IsWidgetOverlappedWithCameraPreview(views::Widget* widget) {
  // Return false immediately if there's a video recording in propress since
  // the camera preview doesn't belong to the current capture session.
  if (CaptureModeController::Get()->is_recording_in_progress())
    return false;

  auto* camera_preview_widget = GetCameraPreviewWidget();
  if (!camera_preview_widget)
    return false;

  return camera_preview_widget->IsVisible() &&
         camera_preview_widget->GetLayer()->GetTargetOpacity() > 0.f &&
         camera_preview_widget->GetWindowBoundsInScreen().Intersects(
             widget->GetWindowBoundsInScreen());
}

int GetHitTestRadiusForFineTunePosition(bool is_touch,
                                        FineTunePosition position,
                                        CaptureModeBehavior* active_behavior) {
  if (is_touch) {
    return kAffordanceCircleTouchHitRadiusDp;
  }

  if (active_behavior->ShouldPaintSunfishCaptureRegion() &&
      capture_mode_util::IsCornerFineTunePosition(position)) {
    return kSunfishRegionCornerFocusRingRadiusDp;
  }

  return kAffordanceCircleRadiusDp;
}

gfx::Rect GetHitTestRectAroundPoint(gfx::Point point, int hit_radius) {
  return gfx::Rect(point.x() - hit_radius, point.y() - hit_radius,
                   hit_radius * 2, hit_radius * 2);
}

gfx::Rect GetHitTestRectForFineTunePosition(
    const gfx::Rect& capture_region_in_screen,
    FineTunePosition position,
    bool is_touch,
    CaptureModeBehavior* active_behavior) {
  const int hit_radius =
      GetHitTestRadiusForFineTunePosition(is_touch, position, active_behavior);

  // In sunfish mode, the capture region is painted with rounded corners. Inset
  // the capture region if needed so that the hit test rect is centered around
  // the corner drag handle.
  gfx::Rect corner_adjusted_capture_region = capture_region_in_screen;
  if (active_behavior->ShouldPaintSunfishCaptureRegion() &&
      capture_mode_util::IsCornerFineTunePosition(position)) {
    corner_adjusted_capture_region.Inset(kSunfishRegionRoundedCornerOffsetDp);
  }

  switch (position) {
    case FineTunePosition::kTopLeftVertex:
      return GetHitTestRectAroundPoint(corner_adjusted_capture_region.origin(),
                                       hit_radius);
    case FineTunePosition::kTopRightVertex:
      return GetHitTestRectAroundPoint(
          corner_adjusted_capture_region.top_right(), hit_radius);
    case FineTunePosition::kBottomRightVertex:
      return GetHitTestRectAroundPoint(
          corner_adjusted_capture_region.bottom_right(), hit_radius);
    case FineTunePosition::kBottomLeftVertex:
      return GetHitTestRectAroundPoint(
          corner_adjusted_capture_region.bottom_left(), hit_radius);
    case FineTunePosition::kTopEdge:
    case FineTunePosition::kBottomEdge: {
      const gfx::Size horizontal_size(
          capture_region_in_screen.width() - 2 * hit_radius, 2 * hit_radius);
      const int horizontal_x = capture_region_in_screen.x() + hit_radius;
      const int horizontal_y =
          position == FineTunePosition::kTopEdge
              ? capture_region_in_screen.y() - hit_radius
              : capture_region_in_screen.bottom() - hit_radius;
      return gfx::Rect(gfx::Point(horizontal_x, horizontal_y), horizontal_size);
    }
    case FineTunePosition::kLeftEdge:
    case FineTunePosition::kRightEdge: {
      const gfx::Size vertical_size(
          2 * hit_radius, capture_region_in_screen.height() - 2 * hit_radius);
      const int vertical_x =
          position == FineTunePosition::kLeftEdge
              ? capture_region_in_screen.x() - hit_radius
              : capture_region_in_screen.right() - hit_radius;
      const int vertical_y = capture_region_in_screen.y() + hit_radius;
      return gfx::Rect(gfx::Point(vertical_x, vertical_y), vertical_size);
    }
    default:
      NOTREACHED();
  }
}

// Calculates the bounds for a widget of `preferred_size` so that it appears
// along one of the edges of `capture_bounds`, or slightly above
// `capture_bar_bounds` if there is not a good edge.
// If non-empty, `other_container_root_bounds` specifies the root bounds of
// another container which should be avoided to prevent overlap.
gfx::Rect CalculateRegionEdgeBounds(
    const gfx::Size& preferred_size,
    const gfx::Rect& capture_bar_root_bounds,
    const gfx::Rect& capture_region_root_bounds,
    const gfx::Rect& other_container_root_bounds,
    aura::Window* root,
    CaptureRegionWidgetAlignment preferred_alignment) {
  // The capture button may be placed along the edge of a capture region if it
  // cannot be placed in the middle. This enum represents the possible edges.
  enum class Direction { kBottom, kTop, kLeft, kRight };
  // Start off with the bounds at the preferred horizontal position but centered
  // vertically. We will shift the bounds to slightly outside `capture_bounds`
  // for each direction if needed.
  gfx::Rect initial_bounds(preferred_size);
  // `directions` specifies the edges of the capture region in the order that we
  // will try positioning the widget.
  std::vector<Direction> directions;
  switch (preferred_alignment) {
    case CaptureRegionWidgetAlignment::kCenter:
      initial_bounds.set_x(capture_region_root_bounds.CenterPoint().x() -
                           preferred_size.width() / 2);
      // Prefer falling back to above the capture region rather than below the
      // capture region, to allow widgets with right alignment to keep their
      // default bottom right position if possible.
      directions = {Direction::kTop, Direction::kBottom, Direction::kLeft,
                    Direction::kRight};
      break;
    case CaptureRegionWidgetAlignment::kRight:
      initial_bounds.set_x(capture_region_root_bounds.right() -
                           preferred_size.width());
      // Prefer right alignment below the capture region if possible.
      directions = {Direction::kBottom, Direction::kTop, Direction::kLeft,
                    Direction::kRight};
      break;
  }
  initial_bounds.set_y(capture_region_root_bounds.CenterPoint().y() -
                       preferred_size.height() / 2);
  const int spacing = CaptureModeSession::kCaptureButtonDistanceFromRegionDp;

  // Try the directions in the preferred order. We will early out if one of
  // them is viable.
  gfx::Rect widget_bounds;
  for (Direction direction : directions) {
    widget_bounds = initial_bounds;

    switch (direction) {
      case Direction::kBottom:
        widget_bounds.set_y(capture_region_root_bounds.bottom() + spacing);
        break;
      case Direction::kTop:
        widget_bounds.set_y(capture_region_root_bounds.y() - spacing -
                            preferred_size.height());
        break;
      case Direction::kLeft:
        widget_bounds.set_x(capture_region_root_bounds.x() - spacing -
                            preferred_size.width());
        break;
      case Direction::kRight:
        widget_bounds.set_x(capture_region_root_bounds.right() + spacing);
        break;
    }

    // If `widget_bounds` does not overlap with `capture_bar_root_bounds` or
    // `other_container_root_bounds` and is fully contained in root,
    // we're good.
    bool intersects_action_buttons =
        !other_container_root_bounds.IsEmpty() &&
        widget_bounds.Intersects(other_container_root_bounds);
    if (!widget_bounds.Intersects(capture_bar_root_bounds) &&
        !intersects_action_buttons && root->bounds().Contains(widget_bounds)) {
      return widget_bounds;
    }
  }

  // Reaching here, we have not found a good edge to place the widget at. The
  // last attempt is to place it slightly above the capture bar.
  widget_bounds.set_size(preferred_size);
  widget_bounds.set_x(capture_bar_root_bounds.CenterPoint().x() -
                      preferred_size.width() / 2);
  widget_bounds.set_y(capture_bar_root_bounds.y() -
                      CaptureModeSession::kCaptureButtonDistanceFromRegionDp -
                      preferred_size.height());

  // TODO: crbug.com/383198941 - Align the action buttons and capture button
  // above the capture bar if they both would like to be placed there.
  // If both the action buttons and the capture button want to be above the
  // capture bar, move the capture button even higher.
  if (!other_container_root_bounds.IsEmpty() &&
      widget_bounds.Intersects(other_container_root_bounds)) {
    widget_bounds.set_y(widget_bounds.y() -
                        CaptureModeSession::kCaptureButtonDistanceFromRegionDp -
                        preferred_size.height());
  }

  return widget_bounds;
}

// Hides `widget` immediately, without animation.
void HideWidgetImmediately(views::Widget* widget) {
  // Disable animations before hiding the widget, to avoid a fade out animation.
  // Animations remain disabled until the widget is shown again, see
  // `ShowAllWidgets()`.
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  widget->Hide();
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeSession::ParentContainerObserver:

// The observer class to observer window added to or removed from the parent
// container `kShellWindowId_MenuContainer`. Capture UIs (capture bar, capture
// label, capture settings, camera preview) are all parented to the parent
// container, thus whenever there's a window added or removed, we need to call
// `RefreshStackingOrder` to ensure the stacking order is correct for them.
class CaptureModeSession::ParentContainerObserver
    : public aura::WindowObserver {
 public:
  ParentContainerObserver(aura::Window* parent_container,
                          CaptureModeSession* capture_mode_session)
      : parent_container_(parent_container),
        capture_mode_session_(capture_mode_session) {
    parent_container_->AddObserver(this);
  }

  ParentContainerObserver(const ParentContainerObserver&) = delete;
  ParentContainerObserver& operator=(const ParentContainerObserver&) = delete;

  ~ParentContainerObserver() override {
    if (parent_container_)
      parent_container_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* window) override {
    DeferredRefreshStackingOrder();
  }

  void OnWindowRemoved(aura::Window* window) override {
    DeferredRefreshStackingOrder();
  }

  void OnWindowDestroying(aura::Window* window) override {
    parent_container_->RemoveObserver(this);
    parent_container_ = nullptr;
  }

 private:
  // There might be other classes that track window hierarchy and re-ordering
  // windows during window adding / deleting will break them. Therefore, defer
  // the re-ordering.
  void DeferredRefreshStackingOrder() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<CaptureModeSession> capture_mode_session) {
              if (capture_mode_session) {
                capture_mode_session->RefreshStackingOrder();
                capture_mode_session->MaybeUpdateCaptureUisOpacity();
              }
            },
            capture_mode_session_->weak_ptr_factory_.GetWeakPtr()));
  }

  raw_ptr<aura::Window> parent_container_;

  // Pointer to current capture session. Not nullptr during this lifecycle.
  // Capture session owns `this`.
  const raw_ptr<CaptureModeSession> capture_mode_session_;
};

// -----------------------------------------------------------------------------
// CaptureModeSession:

CaptureModeSession::CaptureModeSession(CaptureModeController* controller,
                                       CaptureModeBehavior* active_behavior)
    : BaseCaptureModeSession(controller, active_behavior, SessionType::kReal),
      magnifier_glass_(kMagnifierParams),
      cursor_setter_(std::make_unique<CursorSetter>()),
      focus_cycler_(std::make_unique<CaptureModeSessionFocusCycler>(this)),
      capture_toast_controller_(this) {
  CHECK(current_root_);
}

CaptureModeSession::~CaptureModeSession() = default;

void CaptureModeSession::A11yAlertCaptureSource(bool trigger_now) {
  auto* controller = CaptureModeController::Get();
  const bool is_capturing_image = controller->type() == CaptureModeType::kImage;
  std::string message;

  switch (controller->source()) {
    case CaptureModeSource::kFullscreen:
      message = l10n_util::GetStringUTF8(
          is_capturing_image
              ? IDS_ASH_SCREEN_CAPTURE_ALERT_FULLSCREEN_SCREENSHOT
              : IDS_ASH_SCREEN_CAPTURE_ALERT_FULLSCREEN_RECORD);
      break;
    case CaptureModeSource::kRegion:
      if (!controller->user_capture_region().IsEmpty()) {
        message = l10n_util::GetStringUTF8(
            is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_ALERT_REGION_SCREENSHOT
                               : IDS_ASH_SCREEN_CAPTURE_ALERT_REGION_RECORD);
      }
      break;
    case CaptureModeSource::kWindow:
      // Selected window could be non-empty when switching to capture type.
      if (auto* window = GetSelectedWindow()) {
        message = l10n_util::GetStringFUTF8(
            is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_ALERT_WINDOW_SCREENSHOT
                               : IDS_ASH_SCREEN_CAPTURE_ALERT_WINDOW_RECORD,
            window->GetTitle());
      }
      break;
  }

  if (!message.empty()) {
    if (trigger_now)
      capture_mode_util::TriggerAccessibilityAlert(message);
    else
      capture_mode_util::TriggerAccessibilityAlertSoon(message);
  }
}

void CaptureModeSession::SetSettingsMenuShown(bool shown, bool by_key_event) {
  capture_mode_bar_view_->SetSettingsMenuShown(shown);

  if (!shown) {
    capture_mode_settings_widget_.reset();
    capture_mode_settings_view_ = nullptr;
    // After closing CaptureMode settings view, show CaptureLabel view if it has
    // been hidden.
    if (capture_label_widget_ && !capture_label_widget_->IsVisible())
      capture_label_widget_->Show();
    return;
  }

  if (!capture_mode_settings_widget_) {
    // Close the recording type menu if any. There can be only one menu visible
    // at any time.
    SetRecordingTypeMenuShown(false);

    auto* parent = GetParentContainer(current_root_);
    capture_mode_settings_widget_ = std::make_unique<views::Widget>();
    capture_toast_controller_.DismissCurrentToastIfAny();

    capture_mode_settings_widget_->Init(CreateWidgetParams(
        parent, CaptureModeSettingsView::GetBounds(capture_mode_bar_view_),
        "CaptureModeSettingsWidget"));
    capture_mode_settings_view_ =
        capture_mode_settings_widget_->SetContentsView(
            std::make_unique<CaptureModeSettingsView>(this, active_behavior_));
    OnCaptureFolderMayHaveChanged();

    parent->layer()->StackAtTop(capture_mode_settings_widget_->GetLayer());
    focus_cycler_->OnMenuOpened(
        capture_mode_settings_widget_.get(),
        CaptureModeSessionFocusCycler::FocusGroup::kPendingSettings,
        by_key_event);

    if (capture_label_widget_ && capture_label_widget_->IsVisible()) {
      // Hide CaptureLabel view if it overlaps with CaptureMode settings view.
      if (capture_mode_settings_widget_->GetWindowBoundsInScreen().Intersects(
              capture_label_widget_->GetWindowBoundsInScreen())) {
        capture_label_widget_->Hide();
      }
    }
    std::u16string capture_mode_settings_a11y_title =
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SETTINGS_A11Y_TITLE);
    capture_mode_settings_widget_->GetNativeWindow()->SetTitle(
        capture_mode_settings_a11y_title);
    capture_mode_settings_widget_->widget_delegate()->SetAccessibleTitle(
        capture_mode_settings_a11y_title);
    capture_mode_settings_widget_->Show();
  }
}

void CaptureModeSession::OpenFolderSelectionDialog() {
  DCHECK(!folder_selection_dialog_controller_);
  folder_selection_dialog_controller_ =
      std::make_unique<FolderSelectionDialogController>(/*delegate=*/this,
                                                        current_root_);
  // We have to call `MaybeUpdateCameraPreviewBounds()` here after
  // `folder_selection_dialog_controller_` has been set, since
  // `CalculateCameraPreviewTargetVisibility()` checks its existence to
  // determine the target visibility of the camera preview. We cannot however
  // call it in `OnSelectionWindowAdded()` since this function can be called
  // inside the constructor of `FolderSelectionDialogController` before the
  // `folder_selection_dialog_controller_` member is set above.
  MaybeUpdateCameraPreviewBounds();
}

bool CaptureModeSession::IsInCountDownAnimation() const {
  if (is_shutting_down_)
    return false;

  return capture_label_view_ && capture_label_view_->IsInCountDownAnimation();
}

bool CaptureModeSession::IsBarAnchoredToWindow() const {
  return capture_window_observer_ &&
         capture_window_observer_->bar_anchored_to_window();
}

void CaptureModeSession::UpdateCursor(const gfx::Point& location_in_screen,
                                      bool is_touch) {
  // No need to update cursor if `cursor_setter_` has been reset because of
  // OpenFolderSelectionDialog.
  if (is_shutting_down_ || !cursor_setter_) {
    return;
  }

  // Hide mouse cursor in tablet mode except for the dev tablet mode.
  if (display::Screen::Get()->InTabletMode() &&
      !Shell::Get()->tablet_mode_controller()->IsInDevTabletMode()) {
    cursor_setter_->HideCursor();
    return;
  }

  auto* root_window = capture_mode_util::GetPreferredRootWindow();
  if (IsInCountDownAnimation()) {
    cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kPointer);
    return;
  }

  // If the current located event should be handled by camera preview, use the
  // pointer cursor.
  if (should_pass_located_event_to_camera_preview_) {
    cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kPointer);
    return;
  }

  // If the current mouse event is on capture label button, and capture label
  // button can handle the event, show the hand mouse cursor.
  if (capture_label_view_) {
    const bool is_event_on_capture_button =
        capture_label_widget_->GetWindowBoundsInScreen().Contains(
            location_in_screen) &&
        capture_label_view_->ShouldHandleEvent();
    if (is_event_on_capture_button) {
      cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kHand);
      return;
    }
  }

  // TODO: crbug.com/375696216 - Further refine this so the area between buttons
  // does not show the hand cursor.
  // If the current mouse event is on an action button, show the hand mouse
  // cursor.
  const bool is_event_on_action_button =
      action_container_widget_ &&
      action_container_widget_->GetLayer()->GetTargetOpacity() &&
      action_container_widget_->GetWindowBoundsInScreen().Contains(
          location_in_screen);
  if (is_event_on_action_button) {
    cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kHand);
    return;
  }

  // As long as the settings menu, or the recording type menu are open, a
  // pointer cursor should be used as long as the cursor is not on top of the
  // capture button, since clicking anywhere outside the bounds of either of
  // them (the menus or the clickable capture button) will dismiss the menus.
  // Also if the event is on the bar, a pointer will also be used, as long as
  // the bar is visible.
  const bool is_event_on_capture_bar =
      capture_mode_bar_widget_->GetLayer()->GetTargetOpacity() &&
      capture_mode_bar_widget_->GetWindowBoundsInScreen().Contains(
          location_in_screen);
  if (capture_mode_settings_widget_ || is_event_on_capture_bar ||
      recording_type_menu_widget_) {
    cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kPointer);
    return;
  }

  const CaptureModeSource source = controller_->source();
  if (source == CaptureModeSource::kWindow &&
      !IsPointOverSelectedWindow(location_in_screen)) {
    // If we're in window capture mode and there is no selected window at the
    // moment, we should use a pointer cursor.
    cursor_setter_->UpdateCursor(root_window, ui::mojom::CursorType::kPointer);
    return;
  }

  if (source == CaptureModeSource::kFullscreen ||
      source == CaptureModeSource::kWindow) {
    // For fullscreen and other window capture cases, we should either use image
    // capture icon or screen record icon as the mouse icon.
    const CaptureModeType capture_mode_type = controller_->type();
    cursor_setter_->UpdateCursor(
        root_window,
        GetCursorForFullscreenOrWindowCapture(capture_mode_type ==
                                              CaptureModeType::kImage),
        static_cast<int>(capture_mode_type));
    return;
  }

  DCHECK_EQ(source, CaptureModeSource::kRegion);
  if (fine_tune_position_ != FineTunePosition::kNone) {
    // We're in fine tuning process.
    if (capture_mode_util::IsCornerFineTunePosition(fine_tune_position_)) {
      cursor_setter_->HideCursor();
    } else {
      cursor_setter_->UpdateCursor(
          root_window, GetCursorTypeForFineTunePosition(fine_tune_position_));
    }
  } else {
    // Otherwise update the cursor depending on the current cursor location.
    cursor_setter_->UpdateCursor(
        root_window, GetCursorTypeForFineTunePosition(
                         GetFineTunePosition(location_in_screen, is_touch)));
  }
}

void CaptureModeSession::HighlightWindowForTab(aura::Window* window) {
  DCHECK(window);
  DCHECK_EQ(CaptureModeSource::kWindow, controller_->source());
  MaybeChangeRoot(window->GetRootWindow(), /*root_window_will_shutdown=*/false);
  capture_window_observer_->SetSelectedWindow(window, /*a11y_alert_again=*/true,
                                              /*bar_anchored_to_window=*/false);
}

void CaptureModeSession::MaybeUpdateSettingsBounds() {
  if (!capture_mode_settings_widget_) {
    return;
  }

  // Set the content of the CaptureModeSettingsView to its maximum preferred
  // size so the menu will update and scroll properly.
  capture_mode_settings_view_->contents()->SetSize(
      capture_mode_settings_view_->contents()->GetPreferredSize());

  capture_mode_settings_widget_->SetBounds(CaptureModeSettingsView::GetBounds(
      capture_mode_bar_view_, capture_mode_settings_view_));
}

void CaptureModeSession::MaybeUpdateCaptureUisOpacity(
    std::optional<gfx::Point> cursor_screen_location) {
  if (is_shutting_down_) {
    return;
  }

  // TODO(conniekxu): Handle this for tablet mode which doesn't have a cursor
  // screen point.
  if (!cursor_screen_location) {
    cursor_screen_location = display::Screen::Get()->GetCursorScreenPoint();
  }

  base::flat_map<views::Widget*, /*opacity=*/float> widget_opacity_map;
  if (capture_mode_bar_widget_) {
    widget_opacity_map[capture_mode_bar_widget_.get()] = 1.f;
  }
  if (capture_label_widget_) {
    widget_opacity_map[capture_label_widget_.get()] = 1.f;
  }

  const bool is_settings_visible = capture_mode_settings_widget_ &&
                                   capture_mode_settings_widget_->IsVisible();
  const bool is_recording_type_menu_visible =
      recording_type_menu_widget_ && recording_type_menu_widget_->IsVisible();
  gfx::Rect capture_region = controller_->user_capture_region();
  wm::ConvertRectToScreen(current_root_, &capture_region);

  // TODO: crbug.com/376311324 - Refactor this logic, as the `for` loop is
  // getting messy with all the `if` statements.
  for (auto& pair : widget_opacity_map) {
    views::Widget* widget = pair.first;
    float& opacity = pair.second;
    DCHECK(widget->GetLayer());

    const gfx::Rect window_bounds_in_screen = widget->GetWindowBoundsInScreen();

    const bool is_cursor_on_top_of_widget =
        window_bounds_in_screen.Contains(*cursor_screen_location);

    if (widget == capture_mode_bar_widget_.get()) {
      // If capture setting is visible, capture bar should be fully opaque even
      // if it's overlapped with camera preview.
      if (is_settings_visible) {
        continue;
      }

      // If drag for capture region is in progress, capture bar should be
      // hidden.
      if (is_drag_in_progress_) {
        opacity = 0.f;
        continue;
      }

      if (is_cursor_on_top_of_widget) {
        continue;
      }

      // If the cursor is hovering on top of the capture label, capture bar
      // should be fully opaque.
      if (capture_label_widget_ &&
          capture_label_widget_->GetWindowBoundsInScreen().Contains(
              *cursor_screen_location)) {
        continue;
      }

      const bool capture_bar_intersects_region =
          controller_->source() == CaptureModeSource::kRegion &&
          window_bounds_in_screen.Intersects(capture_region);

      if (capture_bar_intersects_region) {
        opacity = capture_mode::kCaptureUiOverlapOpacity;
        continue;
      }

      if (focus_cycler_->CaptureBarFocused()) {
        continue;
      }
    }

    if (widget == capture_label_widget_.get() &&
        (is_cursor_on_top_of_widget || focus_cycler_->CaptureLabelFocused() ||
         is_recording_type_menu_visible)) {
      continue;
    }

    if (IsWidgetOverlappedWithCameraPreview(widget)) {
      opacity = capture_mode::kCaptureUiOverlapOpacity;
    }
  }

  for (const auto& pair : widget_opacity_map) {
    views::Widget* widget = pair.first;
    const float& opacity = pair.second;
    capture_mode_util::AnimateToOpacity(widget, opacity);
  }
}

void CaptureModeSession::RefreshBarWidgetBounds() {
  DCHECK(capture_mode_bar_widget_);
  // We need to update the capture bar bounds first and then settings bounds.
  // The sequence matters here since settings bounds depend on capture bar
  // bounds.
  capture_mode_bar_widget_->SetBounds(
      active_behavior_->GetCaptureBarBounds(current_root_));
  MaybeUpdateSettingsBounds();
  if (user_nudge_controller_) {
    user_nudge_controller_->Reposition();
  }
  capture_toast_controller_.MaybeRepositionCaptureToast();
}

views::Widget* CaptureModeSession::GetCaptureModeBarWidget() {
  return capture_mode_bar_widget_.get();
}

aura::Window* CaptureModeSession::GetSelectedWindow() const {
  return capture_window_observer_ ? capture_window_observer_->window()
                                  : nullptr;
}

void CaptureModeSession::SetPreSelectedWindow(
    aura::Window* pre_selected_window) {
  CHECK(capture_window_observer_);
  capture_window_observer_->SetSelectedWindow(pre_selected_window,
                                              /*a11y_alert_again=*/true,
                                              /*bar_anchored_to_window=*/true);
}

void CaptureModeSession::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_source_changed_ = true;

  if (new_source == CaptureModeSource::kWindow) {
    capture_window_observer_ = std::make_unique<CaptureWindowObserver>(this);
  } else {
    capture_window_observer_.reset();
  }

  if (new_source == CaptureModeSource::kRegion) {
    num_capture_region_adjusted_ = 0;
  }

  capture_mode_bar_view_->OnCaptureSourceChanged(new_source);
  UpdateDimensionsLabelWidget(/*is_resizing=*/false);
  layer()->SchedulePaint(layer()->bounds());
  UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  UpdateActionContainerWidget();
  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  if (focus_cycler_->RegionGroupFocused()) {
    focus_cycler_->ClearFocus();
  }

  A11yAlertCaptureSource(/*trigger_now=*/true);

  MaybeReparentCameraPreviewWidget();
  InvalidateImageSearch();
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
  MaybeUpdateSelfieCamInSessionVisibility();
  UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  UpdateActionContainerWidget();
  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  A11yAlertCaptureType();
  InvalidateImageSearch();
}

void CaptureModeSession::OnRecordingTypeChanged() {
  if (capture_label_view_) {
    capture_label_view_->UpdateIconAndText();
    UpdateCaptureLabelWidgetBounds(CaptureLabelAnimation::kNone);
  }
}

void CaptureModeSession::OnAudioRecordingModeChanged() {
  active_behavior_->OnAudioRecordingModeChanged();
}

void CaptureModeSession::OnDemoToolsSettingsChanged() {
  active_behavior_->OnDemoToolsSettingsChanged();
}

void CaptureModeSession::OnWaitingForDlpConfirmationStarted() {
  // In case the DLP manager chooses to show a system-modal dialog, hide all the
  // capture mode UIs and stop the consumption of input events, so users can
  // interact with the dialog.
  is_waiting_for_dlp_confirmation_ = true;

  HideAllUis();
}

void CaptureModeSession::OnWaitingForDlpConfirmationEnded(bool reshow_uis) {
  is_waiting_for_dlp_confirmation_ = false;

  // If `reshow_uis` is true, then we'll undo the hiding of all the capture mode
  // UIs done in `OnWaitingForDlpConfirmationStarted()`.
  if (reshow_uis) {
    ShowAllUis();
  }
}

void CaptureModeSession::ReportSessionHistograms() {
  const CaptureModeSource source = controller_->source();
  const RecordingType recording_type = controller_->recording_type();

  if (source == CaptureModeSource::kRegion) {
    RecordNumberOfCaptureRegionAdjustments(num_capture_region_adjusted_,
                                           active_behavior_);
    const auto region_in_root = controller_->user_capture_region();
    if (is_stopping_to_start_video_recording_ &&
        recording_type == RecordingType::kGif && !region_in_root.IsEmpty()) {
      RecordGifRegionToScreenRatio(100.0f * region_in_root.size().GetArea() /
                                   current_root_->bounds().size().GetArea());
    }
  }

  num_capture_region_adjusted_ = 0;

  RecordCaptureModeSwitchesFromInitialMode(capture_source_changed_);
  RecordCaptureModeConfiguration(controller_->type(), source, recording_type,
                                 controller_->GetEffectiveAudioRecordingMode(),
                                 active_behavior_);
}

void CaptureModeSession::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  DCHECK(capture_label_widget_);

  // Show CaptureLabel view if it has been hidden. Since `capture_label_widget_`
  // is the only widget which should be shown on 3-seconds count down starts,
  // there's no need to consider if it insects with
  // `capture_mode_settings_widget_` or not here.
  if (!capture_label_widget_->IsVisible()) {
    capture_label_widget_->Show();
  }

  DCHECK(capture_label_view_);
  capture_label_view_->StartCountDown(std::move(countdown_finished_callback));
  UpdateCaptureLabelWidgetBounds(CaptureLabelAnimation::kCountdownStart);

  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // Fade out the capture bar, capture settings, recording type menu, and the
  // capture toast if they exist.
  std::vector<ui::Layer*> layers_to_fade_out{
      capture_mode_bar_widget_->GetLayer()};
  if (capture_mode_settings_widget_) {
    layers_to_fade_out.push_back(capture_mode_settings_widget_->GetLayer());
  }
  if (recording_type_menu_widget_) {
    layers_to_fade_out.push_back(recording_type_menu_widget_->GetLayer());
  }
  if (auto* toast_layer = capture_toast_controller_.MaybeGetToastLayer()) {
    layers_to_fade_out.push_back(toast_layer);
  }

  for (auto* layer : layers_to_fade_out) {
    ui::ScopedLayerAnimationSettings layer_settings(layer->GetAnimator());
    layer_settings.SetTransitionDuration(kCaptureWidgetFadeOutDuration);
    layer_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    layer_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(0.f);
  }

  // Do a repaint to hide the affordance circles.
  RepaintRegion();

  // Fade out the shield if it's recording fullscreen.
  if (controller_->source() == CaptureModeSource::kFullscreen) {
    ui::ScopedLayerAnimationSettings shield_settings(layer()->GetAnimator());
    shield_settings.SetTransitionDuration(kCaptureShieldFadeOutDuration);
    shield_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    shield_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.f);
  }
}

void CaptureModeSession::OnCaptureFolderMayHaveChanged() {
  if (!capture_mode_settings_widget_) {
    return;
  }

  // Update the set of menu options in the settings menu and resize it so that
  // it fits its potentially new contents.
  DCHECK(capture_mode_settings_view_);
  capture_mode_settings_view_->OnCaptureFolderMayHaveChanged();
  MaybeUpdateSettingsBounds();
}

void CaptureModeSession::OnDefaultCaptureFolderSelectionChanged() {
  if (!capture_mode_settings_widget_) {
    return;
  }

  DCHECK(capture_mode_settings_view_);
  capture_mode_settings_view_->OnDefaultCaptureFolderSelectionChanged();
}

bool CaptureModeSession::CalculateCameraPreviewTargetVisibility() const {
  // A screenshot-only-session opened during recording should not affect the
  // visibility of a camera preview created for that on-going recording.
  if (controller_->is_recording_in_progress()) {
    return true;
  }

  // The camera preview should be hidden while the folder selection dialog is
  // shown in order to avoid it being on top of the dialog and blocking
  // interacting with it. This is consistent with what we do with the rest of
  // the capture mode UIs (see OnSelectionWindowAdded()).
  if (folder_selection_dialog_controller_) {
    return false;
  }

  // For fullscreen and window sources, the visibility of the camera preview is
  // determined by the preview's size specs, and whether there's a window source
  // selected. We only care about region sources here, since the visibility of
  // the preview in this case depends on what's happening to the region during
  // this session.
  if (controller_->source() != CaptureModeSource::kRegion) {
    return true;
  }

  if (controller_->user_capture_region().IsEmpty()) {
    return false;
  }

  // Adjusting the region from any of its affordance points should result in
  // hiding the preview. However, dragging it around from its center should not
  // change the visibility.
  return !is_drag_in_progress_ ||
         fine_tune_position_ == FineTunePosition::kCenter;
}

void CaptureModeSession::OnCameraPreviewDragStarted() {
  DCHECK(!controller_->is_recording_in_progress());

  // If settings menu is shown at the beginning of drag, we should close it.
  SetSettingsMenuShown(false);
  SetRecordingTypeMenuShown(false);

  // Hide capture UIs while dragging camera preview.
  HideAllUis();
}

void CaptureModeSession::OnCameraPreviewDragEnded(
    const gfx::Point& screen_location,
    bool is_touch) {
  // When drag for camera preview is ended, camera preview will be snapped to
  // one of the snap position, but cursor will leave at where the drag is
  // released. In order to update cursor type correctly after camera preview is
  // snapped, we should update `should_pass_located_event_to_camera_preview_` to
  // false if cursor is not on top of camera preview, since `UpdateCursor` will
  // rely on its value to decide whether cursor should be updated for camera
  // preview.
  auto* camera_preview_widget = GetCameraPreviewWidget();
  DCHECK(camera_preview_widget);
  if (!camera_preview_widget->GetWindowBoundsInScreen().Contains(
          screen_location)) {
    should_pass_located_event_to_camera_preview_ = false;
  }

  // If CaptureUIs (capture bar, capture label) are overlapped with camera
  // preview and cursor is not on top of it, its opacity should be updated to
  // `kCaptureUiOverlapOpacity` instead of fully opaque.
  MaybeUpdateCaptureUisOpacity(screen_location);

  // Show capture UIs which are hidden in `OnCameraPreviewDragStarted`.
  ShowAllUis();

  // Make sure cursor is updated correctly after camera preview is snapped.
  UpdateCursor(screen_location, is_touch);
}

void CaptureModeSession::OnCameraPreviewBoundsOrVisibilityChanged(
    bool capture_surface_became_too_small,
    bool did_bounds_or_visibility_change) {
  auto* camera_preview_widget = GetCameraPreviewWidget();
  DCHECK(camera_preview_widget);
  const bool is_parented_to_unparented_container =
      camera_preview_widget->GetNativeWindow()->parent()->GetId() ==
      kShellWindowId_UnparentedContainer;
  if (capture_surface_became_too_small && !is_drag_in_progress_ &&
      !is_parented_to_unparented_container) {
    user_nudge_controller_.reset();
    capture_toast_controller_.ShowCaptureToast(
        CaptureToastType::kCameraPreview);
  } else {
    capture_toast_controller_.MaybeDismissCaptureToast(
        CaptureToastType::kCameraPreview);
  }

  if (did_bounds_or_visibility_change) {
    MaybeUpdateCaptureUisOpacity();
  }
}

void CaptureModeSession::OnCameraPreviewDestroyed() {
  capture_toast_controller_.MaybeDismissCaptureToast(
      CaptureToastType::kCameraPreview);
}

void CaptureModeSession::MaybeDismissSunfishRegionNudgeForever() {
  if (user_nudge_controller_) {
    user_nudge_controller_->set_should_dismiss_nudge_forever(true);
  }
  user_nudge_controller_.reset();
}

void CaptureModeSession::MaybeChangeRoot(aura::Window* new_root,
                                         bool root_window_will_shutdown) {
  DCHECK(new_root->IsRootWindow());

  if (new_root == current_root_) {
    return;
  }

  auto* new_parent = GetParentContainer(new_root);
  parent_container_observer_ =
      std::make_unique<ParentContainerObserver>(new_parent, this);

  new_parent->layer()->Add(layer());
  layer()->SetBounds(new_parent->bounds());

  current_root_ = new_root;
  Observe(ColorUtil::GetColorProviderSourceForWindow(current_root_));
  // Update the bounds of the widgets after setting the new root. For region
  // capture, the capture bar will move at a later time, when the mouse is
  // released. If the root change is because of a display removal, the mouse
  // will not be released at a later point.
  if (root_window_will_shutdown ||
      controller_->source() != CaptureModeSource::kRegion) {
    RefreshBarWidgetBounds();
  }

  // Because we use custom cursors for region and full screen capture, we need
  // to update the cursor in case the display DSF changes.
  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // The following call to UpdateCaptureRegion will update the capture label
  // bounds, moving it onto the correct display, but will early return if the
  // region is already empty.
  if (controller_->user_capture_region().IsEmpty()) {
    UpdateCaptureLabelWidgetBounds(CaptureLabelAnimation::kNone);
  }

  // Start with a new region when we switch displays.
  is_selecting_region_ = true;
  UpdateCaptureRegion(gfx::Rect(), /*is_resizing=*/false, /*by_user=*/false,
                      root_window_will_shutdown);

  UpdateRootWindowDimmers();
  MaybeReparentCameraPreviewWidget();

  // Changing the root window may require updating the stacking order on the new
  // display.
  RefreshStackingOrder();
}

std::set<aura::Window*> CaptureModeSession::GetWindowsToIgnoreFromWidgets() {
  std::set<aura::Window*> ignore_windows;
  CHECK(GetCaptureModeBarWidget());
  ignore_windows.insert(GetCaptureModeBarWidget()->GetNativeWindow());

  if (capture_mode_settings_widget()) {
    ignore_windows.insert(capture_mode_settings_widget()->GetNativeWindow());
  }

  if (capture_label_widget()) {
    ignore_windows.insert(capture_label_widget()->GetNativeWindow());
  }

  if (auto* capture_toast_widget =
          capture_toast_controller()->capture_toast_widget()) {
    ignore_windows.insert(capture_toast_widget->GetNativeWindow());
  }
  return ignore_windows;
}

void CaptureModeSession::OnPerformCaptureForSearchStarting(
    PerformCaptureType capture_type) {
  // When performing capture after pressing the smart actions button, only hide
  // widgets that intersect the selected region. Other widgets are kept visible
  // so that we can preserve animations if possible.
  //
  // Ideally, we would also do this for other capture types. However for other
  // capture types (particularly text detection), capture UI widget bounds may
  // not have been set by the time capture is performed. So, we hide all widgets
  // to avoid widgets appearing in cases where bounds are set mid-capture.
  gfx::Rect capture_region_in_screen = controller_->user_capture_region();
  wm::ConvertRectToScreen(current_root_, &capture_region_in_screen);
  for (views::Widget* widget : GetAvailableWidgets()) {
    if (capture_type != PerformCaptureType::kScanner ||
        widget->GetWindowBoundsInScreen().Intersects(
            capture_region_in_screen)) {
      HideWidgetImmediately(widget);
    }
  }

  is_capturing_for_search_ = true;
  // Repaint the layer to hide the capture region border and affordance circles.
  RepaintRegion();
}

void CaptureModeSession::OnPerformCaptureForSearchEnded(
    PerformCaptureType capture_type) {
  is_capturing_for_search_ = false;
  // Repaint the layer to reveal the capture region border and affordance
  // circles.
  RepaintRegion();

  if (!active_behavior_->ShouldReShowUisAtPerformingCapture(capture_type)) {
    return;
  }
  ShowAllWidgets();

  if (active_behavior_->ShouldShowGlowWhileProcessingCaptureType(
          capture_type)) {
    CHECK(capture_region_overlay_controller_);
    capture_region_overlay_controller_->StartGlowAnimation(
        /*animation_delegate=*/this);
    // TODO(crbug.com/400798746): If Scanner is not enabled, the glow animation
    // should continue until OCR has completed. For now, just immediately pause
    // the animation in order to avoid the animation continuing indefinitely.
    ScannerController* scanner_controller = Shell::Get()->scanner_controller();
    if (!scanner_controller || !scanner_controller->CanStartSession()) {
      capture_region_overlay_controller_->PauseGlowAnimation();
    }
  }
}

base::WeakPtr<BaseCaptureModeSession>
CaptureModeSession::GetImageSearchToken() {
  return is_shutting_down_ ? nullptr : weak_token_factory_.GetWeakPtr();
}

ActionButtonView* CaptureModeSession::AddActionButton(
    views::Button::PressedCallback callback,
    std::u16string text,
    const gfx::VectorIcon* icon,
    ActionButtonRank rank,
    ActionButtonViewID id) {
  // This function is called asynchronously, and the conditions may have changed
  // since the action widget was updated.
  UpdateActionContainerWidget();

  // Another process may try to add an action button before the container is
  // created, or while it is invalid. In these cases, we don't want to do
  // anything.
  if (!action_container_widget_) {
    return nullptr;
  }

  // If the user is shown an action button, they have successfully selected a
  // region and invoked a backend response, so there is no need to show the
  // nudge anymore.
  MaybeDismissSunfishRegionNudgeForever();

  CHECK(action_container_view_);
  ActionButtonView* action_button = action_container_view_->AddActionButton(
      std::move(callback), text, icon, rank, id);
  CHECK(action_button);

  UpdateActionContainerWidget();

  return action_button;
}

void CaptureModeSession::AddSmartActionsButton() {
  // This should only be called from the default behavior.
  // This method is only called from `DefaultBehavior::OnRegionSelected…` and
  // `CaptureModeController::OnTextDetectionComplete`, which should only be
  // called by the default behavior, and early returns if the session changes
  // via `image_search_token`.
  CHECK_EQ(active_behavior_->behavior_type(), BehaviorType::kDefault);

  if (!ScannerController::CanShowUiForShell()) {
    RecordScannerFeatureUserState(
        ScannerFeatureUserState::
            kSmartActionsButtonNotShownDueToCanShowUiFalse);
    return;
  }

  if (controller_->IsNetworkConnectionOffline()) {
    RecordScannerFeatureUserState(
        ScannerFeatureUserState::kSmartActionsButtonNotShownDueToOffline);
    return;
  }

  RecordScannerFeatureUserState(
      ScannerFeatureUserState::kScreenCaptureModeScannerButtonShown);
  AddActionButton(
      base::BindRepeating(&CaptureModeSession::OnSmartActionsButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(IDS_ASH_SCANNER_SMART_ACTIONS_BUTTON_LABEL),
      /*icon=*/nullptr,
      ActionButtonRank{ActionButtonType::kScanner, /*weight=*/0},
      ActionButtonViewID::kSmartActionsButton);
}

void CaptureModeSession::MaybeShowScannerDisclaimer(
    ScannerEntryPoint entry_point,
    base::RepeatingClosure accept_callback,
    base::RepeatingClosure decline_callback) {
  bool is_reminder;
  switch (GetScannerDisclaimerType(
      *capture_mode_util::GetActiveUserPrefService(), entry_point)) {
    case ScannerDisclaimerType::kNone:
      if (accept_callback) {
        std::move(accept_callback).Run();
      }
      return;

    case ScannerDisclaimerType::kReminder:
      is_reminder = true;
      break;

    case ScannerDisclaimerType::kFull:
      is_reminder = false;
      break;
  }

  disclaimer_ = DisclaimerView::CreateWidget(
      capture_mode_util::GetPreferredRootWindow(), is_reminder,
      base::BindRepeating(&CaptureModeSession::OnDisclaimerAccepted,
                          weak_ptr_factory_.GetWeakPtr(), entry_point,
                          std::move(accept_callback)),
      base::BindRepeating(&CaptureModeSession::OnDisclaimerDeclined,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(decline_callback)),
      base::BindRepeating(&CaptureModeSession::OnDisclaimerLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(),
                          chrome::kGooglePrivacyPolicyUrl),
      base::BindRepeating(&CaptureModeSession::OnDisclaimerLinkPressed,
                          weak_ptr_factory_.GetWeakPtr(),
                          chrome::kScannerLearnMoreUrl));
  disclaimer_->Show();
  focus_cycler_->OnDisclaimerWidgetOpened(disclaimer_.get());
}

void CaptureModeSession::OnScannerActionsFetched(
    ScannerSession::FetchActionsResponse actions_response) {
  CHECK(capture_region_overlay_controller_);
  capture_region_overlay_controller_->PauseGlowAnimation();

  // Create the action container widget if needed.
  UpdateActionContainerWidget();
  if (!action_container_widget_) {
    return;
  }

  if (!actions_response.has_value()) {
    if (actions_response.error().can_try_again) {
      action_container_view_->ShowErrorView(
          actions_response.error().error_message,
          base::BindRepeating(&CaptureModeSession::OnScannerTryAgainPressed,
                              weak_ptr_factory_.GetWeakPtr()));
    } else {
      action_container_view_->ShowErrorView(
          actions_response.error().error_message);
    }
    UpdateActionContainerWidget();
    return;
  }

  // This is inefficient, as we repeatedly sort, insert and recalculate the
  // bounds for buttons one-by-one. However, this is ok since there can only be
  // a small number of action buttons.
  int size = static_cast<int>(actions_response->size());
  for (int i = 0; i < size; ++i) {
    ScannerActionViewModel& action = actions_response.value()[i];
    std::u16string text = action.GetText();
    const gfx::VectorIcon& icon = action.GetIcon();
    base::RepeatingClosure pressed_callback =
        base::BindRepeating(&CaptureModeSession::OnScannerActionButtonPressed,
                            weak_ptr_factory_.GetWeakPtr(), std::move(action));

    if (ActionButtonView* action_button =
            AddActionButton(std::move(pressed_callback), std::move(text), &icon,
                            ActionButtonRank{ActionButtonType::kScanner, i},
                            ActionButtonViewID::kScannerButton)) {
      action_button->PerformFadeInAnimation(kScannerActionButtonFadeInDuration);
    }
  }

  focus_cycler_->OnScannerActionsFetched();
}

void CaptureModeSession::ShowActionContainerError(
    const std::u16string& error_message) {
  if (!action_container_widget_) {
    return;
  }
  CHECK(action_container_view_);
  action_container_view_->ShowErrorView(error_message);
  UpdateActionContainerWidget();
}

void CaptureModeSession::OnDisclaimerDeclined(base::RepeatingClosure callback) {
  RecordScannerFeatureUserState(
      ScannerFeatureUserState::kConsentDisclaimerRejected);
  capture_mode_util::GetActiveUserPrefService()->SetBoolean(
      prefs::kScannerEnabled, false);

  disclaimer_.reset();

  if (active_behavior_->ShouldAnnounceCaptureModeUIOnDisclaimerDismissed()) {
    capture_mode_util::TriggerAccessibilityAlert(
        active_behavior_->GetCaptureModeOpenAnnouncement());
    // Create the capture label widget and announce it if needed.
    UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  }

  focus_cycler_->OnDisclaimerWidgetClosed();
  if (callback) {
    std::move(callback).Run();
  }
}

void CaptureModeSession::OnDisclaimerAccepted(ScannerEntryPoint entry_point,
                                              base::RepeatingClosure callback) {
  RecordScannerFeatureUserState(
      ScannerFeatureUserState::kConsentDisclaimerAccepted);
  SetScannerDisclaimerAcked(*capture_mode_util::GetActiveUserPrefService(),
                            entry_point);

  disclaimer_.reset();

  if (active_behavior_->ShouldAnnounceCaptureModeUIOnDisclaimerDismissed()) {
    capture_mode_util::TriggerAccessibilityAlert(
        active_behavior_->GetCaptureModeOpenAnnouncement());
    // Create the capture label widget and announce it if needed.
    UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  }

  focus_cycler_->OnDisclaimerWidgetClosed();
  if (callback) {
    std::move(callback).Run();
  }
}

void CaptureModeSession::OnDisclaimerLinkPressed(const char* url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);

  // End the session. `this` is destroyed here.
  controller_->Stop();
}

  void CaptureModeSession::OnSmartActionsButtonPressed() {
    MaybeShowScannerDisclaimer(
        ScannerEntryPoint::kSmartActionsButton,
        /*accept_callback=*/
        base::BindRepeating(
            &CaptureModeSession::OnSmartActionsButtonDisclaimerCheckSuccess,
            weak_ptr_factory_.GetWeakPtr()),
        /*decline_callback=*/
        base::BindRepeating(
            &CaptureModeSession::OnSmartActionsButtonDisclaimerDeclined,
            weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeSession::OnSmartActionsButtonDisclaimerCheckSuccess() {
  if (controller_->IsNetworkConnectionOffline()) {
    ShowActionContainerError(l10n_util::GetStringUTF16(
        IDS_ASH_SCREEN_CAPTURE_ACTION_ATTEMPTED_OFFLINE_ERROR));
    return;
  }

  CHECK(action_container_view_);
  action_container_view_->StartSmartActionsButtonTransition();

  // Fetch Scanner actions.
  auto* scanner_controller = Shell::Get()->scanner_controller();
  CHECK(scanner_controller);
  scanner_controller->StartNewSession();
  controller_->PerformCapture(PerformCaptureType::kScanner);
}

void CaptureModeSession::OnSmartActionsButtonDisclaimerDeclined() {
  action_container_view_->RemoveSmartActionsButton();
}

void CaptureModeSession::OnScannerActionButtonPressed(
    const ScannerActionViewModel& scanner_action) {
  Shell::Get()->scanner_controller()->ExecuteAction(scanner_action);
  controller_->CloseSearchResultsPanel();
  // End the session. `this` is destroyed here.
  controller_->Stop();
}

void CaptureModeSession::OnScannerTryAgainPressed() {
  CHECK(action_container_view_);
  action_container_view_->HideErrorView();
  UpdateActionContainerWidget();
  controller_->PerformCapture(PerformCaptureType::kScanner);
}

void CaptureModeSession::OnPaintLayer(const ui::PaintContext& context) {
  // If the drag of camera preview is in progress, we will hide other capture
  // UIs (capture bar, capture label), but we should still paint the layer to
  // indicate the capture surface where user can drag camera preview on.
  if (!is_all_uis_visible_ &&
      !controller_->camera_controller()->is_drag_in_progress()) {
    return;
  }

  const auto* color_provider_source = GetColorProviderSource();
  CHECK(color_provider_source);
  ui::PaintRecorder recorder(context, layer()->size());
  recorder.canvas()->DrawColor(
      color_provider_source->GetColorProvider()->GetColor(
          capture_mode::kDimmingShieldColor));

  PaintCaptureRegion(recorder.canvas());

  MaybePaintCaptureRegionOverlay(*recorder.canvas());
}

void CaptureModeSession::OnKeyEvent(ui::KeyEvent* event) {
  CHECK(focus_cycler_);

  // We don't consume any events while a DLP system-modal dialog might be shown,
  // so that the user may interact with it.
  if (is_waiting_for_dlp_confirmation_) {
    return;
  }

  if (folder_selection_dialog_controller_) {
    if (folder_selection_dialog_controller_->ShouldConsumeEvent(event)) {
      event->StopPropagation();
    }
    return;
  }

  // If the consent disclaimer is visible, let it handle key events.
  if (disclaimer_) {
    return;
  }

  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // If the search results panel is visible, and the panel is actually focused,
  // we will let the search results panel handle key events (i.e., pressing
  // Enter/Return to make a multimodal search) until it calls `TakeFocus` to
  // return focus back to the focus cycler. As an exception, pressing ESC can
  // still be used to exit the session.
  ui::KeyboardCode key_code = event->key_code();
  if (controller_->IsSearchResultsPanelVisible() &&
      controller_->GetSearchResultsPanel()->HasFocus() &&
      key_code != ui::VKEY_ESCAPE) {
    return;
  }

  auto* camera_preview_view =
      controller_->camera_controller()->camera_preview_view();
  if (camera_preview_view && camera_preview_view->MaybeHandleKeyEvent(event)) {
    event->StopPropagation();
    return;
  }

  bool should_update_opacity = false;

  // Run at the exit of this function to update opacity of capture UIs when
  // necessary. Captures `this` as a WeakPtr since the session might end and be
  // destroyed, e.g. if the escape key was pressed.
  absl::Cleanup deferred_runner = [session = weak_ptr_factory_.GetWeakPtr(),
                                   &should_update_opacity] {
    if (session && should_update_opacity) {
      session->MaybeUpdateCaptureUisOpacity();
    }
  };

  auto* capture_source_view = capture_mode_bar_view_->GetCaptureSourceView();
  const bool is_in_count_down = IsInCountDownAnimation();
  switch (key_code) {
    case ui::VKEY_ESCAPE: {
      event->StopPropagation();
      should_update_opacity = true;

      // We only dismiss the settings / recording type menus or clear the focus
      // on ESC key if the count down is not in progress.
      if (recording_type_menu_widget_ && !is_in_count_down) {
        SetRecordingTypeMenuShown(false);
      } else if (capture_mode_settings_widget_ && !is_in_count_down) {
        SetSettingsMenuShown(false);
      } else if (focus_cycler_->HasFocus() && !is_in_count_down) {
        focus_cycler_->ClearFocus();
      } else if (can_exit_on_escape_) {
        controller_->Stop();  // `this` is destroyed here.
      }

      return;
    }

    case ui::VKEY_RETURN: {
      event->StopPropagation();
      if (!is_in_count_down) {
        // Pressing enter while an item is focused should behave exactly like
        // pressing the space bar on it, unless it's the fullscreen source
        // button, and we are already in fullscreen mode, in this case hitting
        // enter should perform the capture.
        views::View* ignore_view = nullptr;
        if (capture_source_view &&
            controller_->source() == CaptureModeSource::kFullscreen) {
          ignore_view = capture_source_view->fullscreen_toggle_button();
        }
        if (focus_cycler_->MaybeActivateFocusedView(ignore_view)) {
          should_update_opacity = true;
          return;
        }

        active_behavior_
            ->OnEnterKeyPressed();  // `this` can be deleted after this.
      }
      return;
    }

    case ui::VKEY_SPACE: {
      event->StopPropagation();
      event->SetHandled();

      // Hitting space on the region toggle button when we are already in region
      // mode should do nothing as we will take care of it below by creating a
      // default region if needed.
      views::View* ignore_view = nullptr;
      const bool is_in_region_mode =
          controller_->source() == CaptureModeSource::kRegion;
      if (capture_source_view && is_in_region_mode) {
        ignore_view = capture_source_view->region_toggle_button();
      }
      if (focus_cycler_->MaybeActivateFocusedView(ignore_view)) {
        should_update_opacity = true;
        return;
      }

      // Create a default region if we are in region mode and there is no
      // existing region.
      if (is_in_region_mode && controller_->user_capture_region().IsEmpty()) {
        SelectDefaultRegion();
      }
      return;
    }

    case ui::VKEY_TAB: {
      // We only care about specific modifiers, e.g., the interaction between
      // Tab and Caps Lock doesn't concern us.
      constexpr ui::EventFlags kModifiers =
          ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
          ui::EF_COMMAND_DOWN;
      const auto shortcut_flags = kModifiers & event->flags();

      // Only advance focus if explicitly Tab or Shift + Tab are pressed,
      // otherwise keep propagating the event.
      if (shortcut_flags == ui::EF_NONE ||
          shortcut_flags == ui::EF_SHIFT_DOWN) {
        event->StopPropagation();
        event->SetHandled();
        focus_cycler_->AdvanceFocus(/*reverse=*/event->IsShiftDown());
        should_update_opacity = true;
      }
      return;
    }

    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      event->StopPropagation();
      event->SetHandled();
      UpdateRegionForArrowKeys(key_code, event->flags());
      return;
    }

    default:
      return;
  }
}

void CaptureModeSession::OnMouseEvent(ui::MouseEvent* event) {
  // We don't consume any events while a DLP system-modal dialog might be shown,
  // so that the user may interact with it.
  if (is_waiting_for_dlp_confirmation_) {
    return;
  }

  OnLocatedEvent(event, /*is_touch=*/false);
}

void CaptureModeSession::OnTouchEvent(ui::TouchEvent* event) {
  // We don't consume any events while a DLP system-modal dialog might be shown,
  // so that the user may interact with it.
  if (is_waiting_for_dlp_confirmation_) {
    return;
  }

  OnLocatedEvent(event, /*is_touch=*/true);
}

void CaptureModeSession::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, input_capture_window_);
  input_capture_window_->RemoveObserver(this);
  input_capture_window_ = nullptr;
}

void CaptureModeSession::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing when tablet state is still in the process of transition.
    return;
  }

  UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);
}

void CaptureModeSession::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }

  EndSelection();

  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  // Ensure the region still fits the root window after display changes.
  ClampCaptureRegionToRootWindowSize();

  // Update the bounds of all created widgets and repaint the entire layer.
  auto* parent = GetParentContainer(current_root_);
  DCHECK_EQ(parent->layer(), layer()->parent());
  layer()->SetBounds(parent->bounds());

  RefreshBarWidgetBounds();

  // Only need to update the camera preview's bounds if the capture source is
  // `kFullscreen`, since `ClampCaptureRegionToRootWindowSize` will take care of
  // it if the source is `kRegion`.
  // `CaptureWindowObserver::OnWindowBoundsChanged` will take care of it if the
  // source is `kWindow`.
  if (controller_->source() == CaptureModeSource::kFullscreen &&
      !controller_->is_recording_in_progress()) {
    controller_->camera_controller()->MaybeUpdatePreviewWidget();
  }

  if (capture_label_widget_) {
    UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  }
  layer()->SchedulePaint(layer()->bounds());
  MaybeUpdateCaptureUisOpacity();
}

void CaptureModeSession::OnFolderSelected(const base::FilePath& path) {
  CaptureModeController::Get()->SetCustomCaptureFolder(path);
  if (controller_->GetCurrentCaptureFolder().is_default_downloads_folder) {
    RecordSwitchToDefaultFolderReason(
        CaptureModeSwitchToDefaultReason::
            kUserSelectedFromFolderSelectionDialog);
  }
}

void CaptureModeSession::OnSelectionWindowAdded() {
  // Hide all the capture session UIs so that they don't show on top of the
  // selection dialog window and block it.
  HideAllUis();
}

void CaptureModeSession::OnSelectionWindowClosed() {
  DCHECK(folder_selection_dialog_controller_);

  ShowAllUis();

  const bool did_user_select_a_folder =
      folder_selection_dialog_controller_->did_user_select_a_folder();
  folder_selection_dialog_controller_.reset();

  // This has to be called here after the `folder_selection_dialog_controller_`
  // member was reset above, since `CalculateCameraPreviewTargetVisibility()`
  // relies on its existence or lack thereof to determine the target visibility
  // of the camera preview.
  MaybeUpdateCameraPreviewBounds();

  // If the selection window is closed by user selecting a folder, no need to
  // update the capture folder settings menu here, since it's covered by
  // `SetCustomCaptureFolder` via `OnFolderSelected`.
  if (!did_user_select_a_folder) {
    OnCaptureFolderMayHaveChanged();
  }

  // Explicitly hide any virtual keyboard that may have remained open from
  // interacting with the dialog selection window.
  keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
}

void CaptureModeSession::OnColorProviderChanged() {
  if (!is_shutting_down_) {
    layer()->SchedulePaint(layer()->bounds());
  }
}

void CaptureModeSession::AnimationProgressed(const gfx::Animation* animation) {
  if (capture_region_overlay_controller_ &&
      active_behavior_->CanPaintRegionOverlay()) {
    RefreshGlowRegion();
  }
}

void CaptureModeSession::A11yAlertCaptureType() {
  capture_mode_util::TriggerAccessibilityAlert(
      CaptureModeController::Get()->type() == CaptureModeType::kImage
          ? IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_TYPE_IMAGE
          : IDS_ASH_SCREEN_CAPTURE_ALERT_SELECT_TYPE_VIDEO);
  A11yAlertCaptureSource(/*trigger_now=*/false);
}

std::vector<views::Widget*> CaptureModeSession::GetAvailableWidgets() {
  std::vector<views::Widget*> result;
  DCHECK(capture_mode_bar_widget_);
  result.push_back(capture_mode_bar_widget_.get());
  if (capture_label_widget_)
    result.push_back(capture_label_widget_.get());
  if (recording_type_menu_widget_)
    result.push_back(recording_type_menu_widget_.get());
  if (capture_mode_settings_widget_)
    result.push_back(capture_mode_settings_widget_.get());
  if (dimensions_label_widget_)
    result.push_back(dimensions_label_widget_.get());
  if (auto* toast = capture_toast_controller_.capture_toast_widget())
    result.push_back(toast);
  if (action_container_widget_) {
    result.push_back(action_container_widget_.get());
  }
  if (disclaimer_) {
    result.push_back(disclaimer_.get());
  }

  return result;
}

void CaptureModeSession::HideAllUis() {
  is_all_uis_visible_ = false;
  cursor_setter_.reset();
  for (views::Widget* widget : GetAvailableWidgets()) {
    HideWidgetImmediately(widget);
  }
  // Refresh painting the layer, since we don't paint anything while a DLP
  // dialog might be shown.
  layer()->SchedulePaint(layer()->bounds());
}

void CaptureModeSession::ShowAllUis() {
  is_all_uis_visible_ = true;
  cursor_setter_ = std::make_unique<CursorSetter>();
  ShowAllWidgets();
  layer()->SchedulePaint(layer()->bounds());
}


void CaptureModeSession::ShowAllWidgets() {
  for (auto* widget : GetAvailableWidgets()) {
    // At this point the animation is still disabled, see
    // `HideWidgetImmediately()`. Show the window now before we re-enable
    // animations. This is to avoid having those widgets show up in the captured
    // images or videos in case this is used right before ending the session to
    // perform the capture.
    if (CanShowWidget(widget) && !widget->IsVisible()) {
      widget->GetLayer()->SetOpacity(1.0f);
      widget->Show();
    }
    widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                           false);
  }
}

bool CaptureModeSession::CanShowWidget(views::Widget* widget) const {
  // If `widget` is the toast widget, we shouldn't show it again in
  // `ShowAllUis()` unless there is an available toast type, and the toast was
  // never fully dismissed.
  if (widget == capture_toast_controller_.capture_toast_widget())
    return !!capture_toast_controller_.current_toast_type();

  // If widget is the capture label widget, we will show it only if it doesn't
  // intersect with the settings widget.
  return !(capture_label_widget_ && capture_mode_settings_widget_ &&
           capture_label_widget_.get() == widget &&
           capture_mode_settings_widget_->GetWindowBoundsInScreen().Intersects(
               capture_label_widget_->GetWindowBoundsInScreen()));
}

void CaptureModeSession::MaybeCreateSunfishRegionNudge() {
  user_nudge_controller_.reset();

  if (!active_behavior_->ShouldShowUserNudge()) {
    return;
  }

  if (!controller_->CanShowSunfishRegionNudge()) {
    return;
  }

  auto* region_button =
      capture_mode_bar_view_->GetCaptureSourceView()->region_toggle_button();
  CHECK(region_button);
  user_nudge_controller_ =
      std::make_unique<UserNudgeController>(this, region_button);
  user_nudge_controller_->SetVisible(true);
}

void CaptureModeSession::DoPerformCapture() {
  controller_->PerformCapture();  // `this` can be deleted after this.
}

void CaptureModeSession::OnSearchButtonPressed() {
  RecordSearchButtonPressed();
  // See if we can move this to `PerformImageSearch()`.
  controller_->PerformCapture(
      PerformCaptureType::kSearch);  // `this` can be deleted after this.
}

void CaptureModeSession::OnRecordingTypeDropDownButtonPressed(
    const ui::Event& event) {
  SetRecordingTypeMenuShown(
      !recording_type_menu_widget_ || !recording_type_menu_widget_->IsVisible(),
      event.IsKeyEvent());
}

void CaptureModeSession::RefreshStackingOrder() {
  if (is_shutting_down_)
    return;

  auto* parent_container = GetParentContainer(current_root_);
  DCHECK(parent_container);
  auto* session_layer = layer();
  auto* parent_container_layer = parent_container->layer();
  parent_container_layer->StackAtTop(session_layer);

  std::vector<views::Widget*> widget_in_order;

  auto* camera_preview_widget = GetCameraPreviewWidget();
  // We don't need to update the stacking order for camera preview if
  // there's a video recording in progress, since the camera preview don't
  // belong to the current capture session.
  if (camera_preview_widget && !controller_->is_recording_in_progress())
    widget_in_order.emplace_back(camera_preview_widget);
  if (auto* toast = capture_toast_controller_.capture_toast_widget())
    widget_in_order.emplace_back(toast);
  if (capture_label_widget_)
    widget_in_order.emplace_back(capture_label_widget_.get());
  if (action_container_widget_) {
    widget_in_order.emplace_back(action_container_widget_.get());
  }
  if (capture_mode_bar_widget_)
    widget_in_order.emplace_back(capture_mode_bar_widget_.get());
  if (recording_type_menu_widget_)
    widget_in_order.emplace_back(recording_type_menu_widget_.get());
  if (capture_mode_settings_widget_)
    widget_in_order.emplace_back(capture_mode_settings_widget_.get());

  for (auto* widget : widget_in_order) {
    auto* widget_window = widget->GetNativeWindow();
    // Make sure the order of `widget` layer and the order of `widget` window
    // match. Also notice we should stack layer later since when stacking
    // window, it will also stack window's layer which may mess the layer's
    // order if we stack layer first.
    if (widget_window->parent() == parent_container) {
      parent_container->StackChildAtTop(widget_window);
      parent_container_layer->StackAtTop(widget_window->layer());
    }
  }
}

void CaptureModeSession::PaintCaptureRegion(gfx::Canvas* canvas) {
  if (active_behavior_->ShouldPaintSunfishCaptureRegion()) {
    PaintSunfishCaptureRegion(canvas);
    return;
  }

  gfx::Rect region;
  bool adjustable_region = false;

  switch (controller_->source()) {
    case CaptureModeSource::kFullscreen:
      region = current_root_->bounds();
      break;

    case CaptureModeSource::kWindow:
      region = GetSelectedWindowTargetBounds();
      break;

    case CaptureModeSource::kRegion:
      region = controller_->user_capture_region();
      adjustable_region = true;
      break;
  }

  if (region.IsEmpty())
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  region = gfx::ScaleToEnclosingRect(region, dsf);

  const auto* color_provider = GetColorProviderSource()->GetColorProvider();

  if (!adjustable_region) {
    canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);
    canvas->FillRect(region,
                     color_provider->GetColor(kColorAshCaptureRegionColor));
    return;
  }

  if (capture_region_overlay_controller_ &&
      active_behavior_->CanPaintRegionOverlay()) {
    // Draw a glow effect around the capture region if needed. Note that this
    // needs to be drawn before the transparent region and region border are
    // drawn, since the glow should not cover those parts of the UI.
    capture_region_overlay_controller_->PaintCurrentGlowState(*canvas, region,
                                                              color_provider);
  }

  region.Inset(-capture_mode::kCaptureRegionBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);

  // If a screenshot is being taken for search, do not paint the region border
  // and affordance circles to avoid including them in the screenshot.
  if (is_capturing_for_search_) {
    return;
  }

  // Draw the region border.
  cc::PaintFlags border_flags;
  border_flags.setColor(capture_mode::kRegionBorderColor);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(capture_mode::kCaptureRegionBorderStrokePx);
  border_flags.setLooper(gfx::CreateShadowDrawLooper({kRegionOutlineShadow}));
  canvas->DrawRect(gfx::RectF(region), border_flags);

  // Draws the focus ring if the region or one of the affordance circles
  // currently has focus.
  auto maybe_draw_focus_ring = [&canvas, &region, dsf,
                                color_provider](FineTunePosition position) {
    if (position == FineTunePosition::kNone)
      return;

    cc::PaintFlags focus_ring_flags;
    focus_ring_flags.setAntiAlias(true);
    focus_ring_flags.setColor(color_provider->GetColor(ui::kColorAshFocusRing));
    focus_ring_flags.setStyle(cc::PaintFlags::kStroke_Style);
    focus_ring_flags.setStrokeWidth(kFocusRingStrokeWidthDp);

    if (position == FineTunePosition::kCenter) {
      gfx::RectF focus_rect(region);
      focus_rect.Inset(
          gfx::InsetsF(-kFocusRingSpacingDp - kFocusRingStrokeWidthDp / 2));
      canvas->DrawRect(focus_rect, focus_ring_flags);
      return;
    }

    const float radius =
        dsf * (kAffordanceCircleRadiusDp + kFocusRingSpacingDp +
               kFocusRingStrokeWidthDp / 2);
    canvas->DrawCircle(
        capture_mode_util::GetLocationForFineTunePosition(region, position),
        radius, focus_ring_flags);
  };

  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  if (is_selecting_region_ || fine_tune_position_ != FineTunePosition::kNone) {
    maybe_draw_focus_ring(focused_fine_tune_position);
    return;
  }

  if (IsInCountDownAnimation())
    return;

  // Draw the drag affordance circles.
  cc::PaintFlags circle_flags;
  circle_flags.setColor(capture_mode::kRegionBorderColor);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  circle_flags.setAntiAlias(true);
  circle_flags.setLooper(gfx::CreateShadowDrawLooper(
      {kRegionAffordanceCircleShadow1, kRegionAffordanceCircleShadow2}));

  auto draw_circle = [&canvas, &circle_flags,
                      &dsf](const gfx::Point& location) {
    canvas->DrawCircle(location, dsf * kAffordanceCircleRadiusDp, circle_flags);
  };

  draw_circle(region.origin());
  draw_circle(region.top_center());
  draw_circle(region.top_right());
  draw_circle(region.right_center());
  draw_circle(region.bottom_right());
  draw_circle(region.bottom_center());
  draw_circle(region.bottom_left());
  draw_circle(region.left_center());

  maybe_draw_focus_ring(focused_fine_tune_position);
}

void CaptureModeSession::PaintSunfishCaptureRegion(gfx::Canvas* canvas) {
  gfx::Rect region = controller_->user_capture_region();
  if (region.IsEmpty()) {
    return;
  }

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  region = gfx::ScaleToEnclosingRect(region, dsf);

  const auto* color_provider = GetColorProviderSource()->GetColorProvider();

  if (capture_region_overlay_controller_ &&
      active_behavior_->CanPaintRegionOverlay()) {
    // Draw a glow effect around the capture region if needed. Note that this
    // needs to be drawn before the region mask and region border are drawn,
    // since the glow should not cover those parts of the UI.
    capture_region_overlay_controller_->PaintCurrentGlowState(*canvas, region,
                                                              color_provider);
  }

  // Clear the area over the capture region.
  cc::PaintFlags region_mask_flags;
  region_mask_flags.setStyle(cc::PaintFlags::kFill_Style);
  region_mask_flags.setAntiAlias(true);
  region_mask_flags.setBlendMode(SkBlendMode::kClear);
  canvas->DrawRoundRect(region, kSunfishModeCaptureRegionRadiusDp,
                        region_mask_flags);

  // Draw the region border if the user is currently selecting a capture region
  // and not taking a screenshot. Note that this doesn't include fine tune
  // phases where the user adjusts the capture region.
  if (is_selecting_region_ && !is_capturing_for_search_) {
    cc::PaintFlags border_flags;
    border_flags.setShader(gfx::CreateGradientShader(
        region.origin(), region.top_right(),
        color_provider->GetColor(cros_tokens::kCrosSysMuted),
        color_provider->GetColor(cros_tokens::kCrosSysComplement)));
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(kSunfishModeCaptureRegionBorderWidthDp);
    border_flags.setAntiAlias(true);
    border_flags.setLooper(gfx::CreateShadowDrawLooper({kRegionOutlineShadow}));
    canvas->DrawRoundRect(region, kSunfishModeCaptureRegionRadiusDp,
                          border_flags);
  }

  // If the user is currently selecting or adjusting the capture region or
  // taking a screenshot, there is no need to paint drag handles so we can early
  // return.
  if (is_selecting_region_ || fine_tune_position_ != FineTunePosition::kNone ||
      is_capturing_for_search_) {
    return;
  }

  // Draw circular drag handles at the center of each edge of the capture
  // region.
  cc::PaintFlags circle_flags;
  circle_flags.setColor(capture_mode::kRegionBorderColor);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  circle_flags.setAntiAlias(true);
  circle_flags.setLooper(gfx::CreateShadowDrawLooper(
      {kRegionAffordanceCircleShadow1, kRegionAffordanceCircleShadow2}));
  auto draw_circle = [&canvas, &circle_flags,
                      &dsf](const gfx::Point& location) {
    canvas->DrawCircle(location, dsf * kAffordanceCircleRadiusDp, circle_flags);
  };

  draw_circle(region.top_center());
  draw_circle(region.right_center());
  draw_circle(region.bottom_center());
  draw_circle(region.left_center());

  // Draw drag handles at each corner of the capture region. Each handle is a
  // quarter arc with rounded ends.
  cc::PaintFlags corner_arc_flags;
  corner_arc_flags.setColor(capture_mode::kRegionBorderColor);
  corner_arc_flags.setStyle(cc::PaintFlags::kStroke_Style);
  corner_arc_flags.setAntiAlias(true);
  corner_arc_flags.setLooper(gfx::CreateShadowDrawLooper(
      {kRegionAffordanceCircleShadow1, kRegionAffordanceCircleShadow2}));
  corner_arc_flags.setStrokeWidth(kSunfishModeCaptureRegionBorderWidthDp);
  // Make the ends of each arc rounded.
  corner_arc_flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
  const float arc_diameter = dsf * 2 * kSunfishModeCaptureRegionRadiusDp;
  // Draws a 90 degree arc from the circle with diameter `arc_diameter`,
  // leftmost coordinate `circle_left`, and topmost coordinate `circle_top`.
  // `start_angle` indicates the starting angle of the arc in degrees, measured
  // clockwise from the positive x-axis. The arc is drawn clockwise from the
  // specified `start_angle`.
  auto draw_corner_arc = [&canvas, &corner_arc_flags, &arc_diameter](
                             int circle_left, int circle_top,
                             SkScalar start_angle) {
    const SkPath corner_arc =
        SkPathBuilder()
            .arcTo(/*oval=*/SkRect::MakeXYWH(circle_left, circle_top,
                                             arc_diameter, arc_diameter),
                   /*startAngle=*/start_angle,
                   /*sweepAngle=*/90, /*forceMoveTo=*/false)
            .detach();
    canvas->DrawPath(corner_arc, corner_arc_flags);
  };

  // Top left handle.
  draw_corner_arc(/*circle_left=*/region.x(), /*circle_top=*/region.y(),
                  /*start_angle=*/180);
  // Top right handle.
  draw_corner_arc(/*circle_left=*/region.right() - arc_diameter,
                  /*circle_top=*/region.y(),
                  /*start_angle=*/270);
  // Bottom right handle.
  draw_corner_arc(
      /*circle_left=*/region.right() - arc_diameter,
      /*circle_top=*/region.bottom() - arc_diameter,
      /*start_angle=*/0);
  // Bottom left handle.
  draw_corner_arc(/*circle_left=*/region.x(),
                  /*circle_top=*/region.bottom() - arc_diameter,
                  /*start_angle=*/90);

  // Draw a focus ring if the region or one of the drag handles circles
  // currently has focus.
  cc::PaintFlags focus_ring_flags;
  focus_ring_flags.setAntiAlias(true);
  focus_ring_flags.setColor(color_provider->GetColor(ui::kColorAshFocusRing));
  focus_ring_flags.setStyle(cc::PaintFlags::kStroke_Style);
  focus_ring_flags.setStrokeWidth(kFocusRingStrokeWidthDp);
  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  switch (focused_fine_tune_position) {
    case FineTunePosition::kNone:
      break;
    case FineTunePosition::kCenter: {
      gfx::RectF focus_rect(region);
      focus_rect.Outset(kFocusRingSpacingDp + kFocusRingStrokeWidthDp / 2);
      canvas->DrawRoundRect(focus_rect, kSunfishRegionFocusRingRadiusDp,
                            focus_ring_flags);
      break;
    }
    case FineTunePosition::kTopLeftVertex:
    case FineTunePosition::kTopRightVertex:
    case FineTunePosition::kBottomRightVertex:
    case FineTunePosition::kBottomLeftVertex: {
      const float radius = dsf * (kSunfishRegionCornerFocusRingRadiusDp +
                                  kFocusRingStrokeWidthDp / 2);
      region.Inset(kSunfishRegionRoundedCornerOffsetDp);
      canvas->DrawCircle(capture_mode_util::GetLocationForFineTunePosition(
                             region, focused_fine_tune_position),
                         radius, focus_ring_flags);
      break;
    }
    case FineTunePosition::kTopEdge:
    case FineTunePosition::kRightEdge:
    case FineTunePosition::kBottomEdge:
    case FineTunePosition::kLeftEdge: {
      const float radius =
          dsf * (kAffordanceCircleRadiusDp + kFocusRingSpacingDp +
                 kFocusRingStrokeWidthDp / 2);
      canvas->DrawCircle(capture_mode_util::GetLocationForFineTunePosition(
                             region, focused_fine_tune_position),
                         radius, focus_ring_flags);
      break;
    }
  }
}

void CaptureModeSession::MaybePaintCaptureRegionOverlay(
    gfx::Canvas& canvas) const {
  if (capture_region_overlay_controller_ &&
      active_behavior_->CanPaintRegionOverlay()) {
    capture_region_overlay_controller_->PaintCaptureRegionOverlay(
        canvas, /*region_bounds_in_canvas=*/controller_->user_capture_region());
  }
}

void CaptureModeSession::OnLocatedEvent(ui::LocatedEvent* event,
                                        bool is_touch) {
  if (folder_selection_dialog_controller_) {
    if (folder_selection_dialog_controller_->ShouldConsumeEvent(event))
      event->StopPropagation();
    return;
  }

  // If we're currently in countdown animation, don't further handle any
  // located events. However we should stop the event propagation here to
  // prevent other event handlers from handling this event.
  if (IsInCountDownAnimation()) {
    event->StopPropagation();
    return;
  }

  // If disclaimer dialog is visible, block all actions.
  if (disclaimer_) {
    // TODO(b/367882127): See if we can never create a cursor_setter_ instead of
    // having to reset it.
    if (cursor_setter_) {
      cursor_setter_->ResetCursor();
    }
    return;
  }

  // |ui::EventType::kMouseExited| and |ui::EventType::kMouseEntered| events
  // will be generated during moving capture mode bar to another display. We
  // should ignore them here, since they will overwrite the capture mode bar's
  // root change during keyboard tabbing in capture window mode.
  if (event->type() == ui::EventType::kMouseCaptureChanged ||
      event->type() == ui::EventType::kMouseExited ||
      event->type() == ui::EventType::kMouseEntered) {
    return;
  }

  // We should ignore synthesized events here. Otherwise, synthesized events
  // will overwrite the change by the actual event because of the
  // asynchronism (please check |WindowEventDispatcher::PostSynthesizeMouseMove|
  // for more information).
  // For example, during keyboard navigation in capture window mode, changing
  // root of capture mode bar will generate the synthesized mouse move event. It
  // will overwrite the root change since the location of the synthesized event
  // is still on the previous root.
  // For the window related synthesized events (window activation, window
  // destroy), |capture_window_observer_| can take care of them.
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  gfx::Point screen_location = event->location();
  aura::Window* event_target = static_cast<aura::Window*>(event->target());
  wm::ConvertPointToScreen(event_target, &screen_location);

  // For fullscreen/window mode, change the root window as soon as we detect the
  // cursor on a new display. For region mode, wait until the user taps down to
  // try to select a new region on the new display.
  const CaptureModeSource capture_source = controller_->source();
  const bool is_capture_region = capture_source == CaptureModeSource::kRegion;

  const bool is_press_event = event->type() == ui::EventType::kMousePressed ||
                              event->type() == ui::EventType::kTouchPressed;

  // Clear keyboard focus on presses.
  if (is_press_event && focus_cycler_->HasFocus())
    focus_cycler_->ClearFocus();

  // Do not update the root on cursor moving if the capture bar is set to be
  // anchored to the selected window. As in this case, all the widgets should be
  // anchored to the window, they should only be updated if the window was moved
  // to a different root window.
  const bool is_bar_anchored_to_window =
      controller_->source() == CaptureModeSource::kWindow &&
      capture_window_observer_->bar_anchored_to_window();
  const bool can_change_root =
      !is_bar_anchored_to_window && (!is_capture_region || is_press_event);

  if (can_change_root) {
    MaybeChangeRoot(capture_mode_util::GetPreferredRootWindow(screen_location),
                    /*root_window_will_shutdown=*/false);
  }

  // The root may have switched while pressing the mouse down. Move the capture
  // bar to the current display if that is the case and make sure it is stacked
  // at the top. The dimensions label and capture button have been moved and
  // stacked on tap down so manually stack at top instead of calling
  // RefreshStackingOrder.
  const bool is_release_event =
      event->type() == ui::EventType::kMouseReleased ||
      event->type() == ui::EventType::kTouchReleased;
  if (is_release_event && is_capture_region &&
      current_root_ !=
          capture_mode_bar_widget_->GetNativeWindow()->GetRootWindow()) {
    RefreshBarWidgetBounds();
  }

  MaybeUpdateCaptureUisOpacity(screen_location);

  // Allow all events located on the results panel (if present) to go through.
  // See `CaptureModeController::IsEventOnSearchResultsPanel()`.
  // This must be done after `MaybeUpdateCaptureUisOpacity()` which will hide
  // the panel if a drag is in progress and before running
  // `deferred_cursor_updater` to allow the panel to update the cursor type.
  if (controller_->IsEventOnSearchResultsPanel(*event, screen_location)) {
    if (cursor_setter_) {
      cursor_setter_->ResetCursor();
    }
    // If the event is a mouse or touch down, we assume the user wants to
    // interact with the panel and stop the session now.
    if (is_press_event && (event->flags() & ui::EF_RIGHT_MOUSE_BUTTON)) {
      controller_->Stop();  // Deletes `this`.
    }
    return;
  }

  // Update the value of `should_pass_located_event_to_camera_preview_` here
  // before calling `UpdateCursor` which uses
  should_pass_located_event_to_camera_preview_ =
      ShouldPassEventToCameraPreview(event);

  // From here on, no matter where the function exists, the cursor must be
  // updated at the end. Capture `this` as a WeakPtr since performing capture
  // may end up deleting `this`.
  absl::Cleanup deferred_cursor_updater =
      [session = weak_ptr_factory_.GetWeakPtr(), screen_location, is_touch] {
        if (session) {
          session->UpdateCursor(screen_location, is_touch);
        }
      };

  if (should_pass_located_event_to_camera_preview_) {
    DCHECK(!controller_->is_recording_in_progress());
    return;
  }

  // Let the capture button handle any events it can handle first.
  if (ShouldCaptureLabelHandleEvent(event_target))
    return;

  // Let the action buttons handle their events if any.
  if (capture_mode_util::IsEventTargetedOnWidget(
          *event, action_container_widget_.get())) {
    return;
  }

  // Let the recording type menu handle its events if any.
  if (capture_mode_util::IsEventTargetedOnWidget(
          *event, recording_type_menu_widget_.get())) {
    return;
  }

  // Also allow events that target the settings menu (if present) to go through.
  if (capture_mode_util::IsEventTargetedOnWidget(
          *event, capture_mode_settings_widget_.get())) {
    return;
  }

  // Here we know that the event doesn't target the settings menu, so if it's a
  // press event, we will use it to dismiss the settings menu, unless it's on
  // the settings button (since in this case, the settings button handler will
  // take care of dismissing the menu).
  const bool should_close_settings =
      is_press_event &&
      !capture_mode_bar_view_->IsEventOnSettingsButton(screen_location) &&
      capture_mode_settings_widget_;
  if (should_close_settings) {
    // All future located events up to and including a released events will be
    // consumed and ignored (i.e. won't be used to update the capture region,
    // the selected window, or perform capture ... etc.), unless it's targeting
    // the capture button.
    ignore_located_events_ = true;
    SetSettingsMenuShown(/*shown=*/false);
  }

  // Similar to the above, we want a press event that is outside the recording
  // type menu to close it, unless it is on on the drop down menu button if any.
  const bool should_close_recording_type_menu =
      is_press_event &&
      !IsPointOnRecordingTypeDropDownButton(screen_location) &&
      recording_type_menu_widget_;
  if (should_close_recording_type_menu) {
    ignore_located_events_ = true;
    SetRecordingTypeMenuShown(false);
  }

  const bool old_ignore_located_events = ignore_located_events_;
  if (ignore_located_events_) {
    if (is_release_event)
      ignore_located_events_ = false;
  }

  // Events targeting the capture bar should also go through.
  if (capture_mode_util::IsEventTargetedOnWidget(
          *event, capture_mode_bar_widget_.get())) {
    return;
  }

  event->SetHandled();
  event->StopPropagation();

  if (should_close_settings || old_ignore_located_events ||
      should_close_recording_type_menu) {
    // Note that these ignored events have already been consumed above.
    return;
  }

  const bool is_capture_fullscreen =
      capture_source == CaptureModeSource::kFullscreen;
  const bool is_capture_window = capture_source == CaptureModeSource::kWindow;

  if (is_capture_fullscreen || is_capture_window) {
    switch (event->type()) {
      case ui::EventType::kMouseMoved:
      case ui::EventType::kTouchPressed:
      case ui::EventType::kTouchMoved: {
        if (is_capture_window) {
          capture_window_observer_->UpdateSelectedWindowAtPosition(
              screen_location);
        }
        break;
      }
      case ui::EventType::kMouseReleased:
      case ui::EventType::kTouchReleased:
        if (is_capture_fullscreen ||
            IsPointOverSelectedWindow(screen_location)) {
          // Clicking anywhere in fullscreen mode, or over the selected window
          // in window mode should perform the capture operation.
          DoPerformCapture();  // `this` can be deleted after this.
        }
        break;
      default:
        break;
    }
    return;
  }

  DCHECK(is_capture_region);

  // `OnLocatedEventPressed()` and `OnLocatedEventDragged()` used root locations
  // since `CaptureModeController::user_capture_region()` is stored in root
  // coordinates.
  const gfx::Point& location_in_root = event->root_location();

  switch (event->type()) {
    case ui::EventType::kMousePressed:
    case ui::EventType::kTouchPressed:
      old_mouse_warp_status_ = SetMouseWarpEnabled(false);
      OnLocatedEventPressed(location_in_root, is_touch);
      break;
    case ui::EventType::kMouseDragged:
    case ui::EventType::kTouchMoved:
      OnLocatedEventDragged(location_in_root);
      break;
    case ui::EventType::kMouseReleased:
    case ui::EventType::kTouchReleased:
      // Reenable mouse warping.
      if (old_mouse_warp_status_)
        SetMouseWarpEnabled(*old_mouse_warp_status_);
      old_mouse_warp_status_.reset();
      OnLocatedEventReleased(location_in_root);
      break;
    default:
      break;
  }
}

FineTunePosition CaptureModeSession::GetFineTunePosition(
    const gfx::Point& location_in_screen,
    bool is_touch) const {
  // When the region is empty, this is a brand new selection rather than a fine
  // tune.
  if (controller_->user_capture_region().IsEmpty())
    return FineTunePosition::kNone;

  gfx::Rect capture_region_in_screen = controller_->user_capture_region();
  wm::ConvertRectToScreen(current_root_, &capture_region_in_screen);
  // In the case of overlapping affordances, prioritize the bottomm right
  // corner, then the rest of the corners, then the edges.
  static const std::vector<FineTunePosition> drag_positions = {
      FineTunePosition::kBottomRightVertex, FineTunePosition::kBottomLeftVertex,
      FineTunePosition::kTopLeftVertex,     FineTunePosition::kTopRightVertex,
      FineTunePosition::kBottomEdge,        FineTunePosition::kLeftEdge,
      FineTunePosition::kTopEdge,           FineTunePosition::kRightEdge};

  for (FineTunePosition position : drag_positions) {
    if (GetHitTestRectForFineTunePosition(capture_region_in_screen, position,
                                          is_touch, active_behavior_)
            .Contains(location_in_screen)) {
      return position;
    }
  }
  if (capture_region_in_screen.Contains(location_in_screen))
    return FineTunePosition::kCenter;

  return FineTunePosition::kNone;
}

void CaptureModeSession::OnLocatedEventPressed(
    const gfx::Point& location_in_root,
    bool is_touch) {
  initial_location_in_root_ = location_in_root;
  previous_location_in_root_ = location_in_root;

  // Use cursor compositing instead of the platform cursor when dragging to
  // ensure the cursor is aligned with the region.
  is_drag_in_progress_ = true;
  Shell::Get()->UpdateCursorCompositingEnabled();

  if (user_nudge_controller_)
    user_nudge_controller_->SetVisible(false);

  gfx::Point screen_location = location_in_root;
  wm::ConvertPointToScreen(current_root_, &screen_location);
  MaybeUpdateCaptureUisOpacity(screen_location);

  InvalidateImageSearch();

  // Run `MaybeUpdateCameraPreviewBounds` at the exit of this function's
  // scope since the camera preview should be hidden if user is dragging to
  // update the capture region. The reason we want to run it at the exit of this
  // function is if `is_selecting_region_` is false, we want
  // `fine_tune_position_` to be updated first since it can affect whether we
  // should hide camera preview or not. Captures `this` by WeakPtr since
  // pressing escape while dragging a capture region will end the session and
  // delete `this`.
  absl::Cleanup deferred_runner = [session = weak_ptr_factory_.GetWeakPtr()] {
    if (session) {
      session->MaybeUpdateCameraPreviewBounds();
    }
  };

  if (is_selecting_region_)
    return;

  fine_tune_position_ = GetFineTunePosition(screen_location, is_touch);

  // The capture region will be changing, so remove any existing action buttons,
  // if any, as they will no longer be applicable.
  ClearActionContainer();

  if (fine_tune_position_ == FineTunePosition::kNone) {
    // If the point is outside the capture region and not on the capture bar or
    // settings menu, restart to the select phase.
    is_selecting_region_ = true;
    UpdateCaptureRegion(gfx::Rect(), /*is_resizing=*/true, /*by_user=*/true,
                        /*root_window_will_shutdown=*/false);
    num_capture_region_adjusted_ = 0;
    return;
  }

  // In order to hide the drag affordance circles on click, we need to repaint
  // the capture region.
  if (fine_tune_position_ != FineTunePosition::kNone) {
    ++num_capture_region_adjusted_;
    RepaintRegion();
  }

  if (fine_tune_position_ != FineTunePosition::kCenter &&
      fine_tune_position_ != FineTunePosition::kNone) {
    anchor_points_ = GetAnchorPointsForPosition(fine_tune_position_);
    const gfx::Point position_location =
        capture_mode_util::GetLocationForFineTunePosition(
            controller_->user_capture_region(), fine_tune_position_);
    MaybeShowMagnifierGlassAtPoint(position_location);
  }
}

void CaptureModeSession::OnLocatedEventDragged(
    const gfx::Point& location_in_root) {
  const gfx::Point previous_location_in_root = previous_location_in_root_;
  previous_location_in_root_ = location_in_root;
  controller_->OnLocatedEventDragged();

  // For the select phase, the select region is the rectangle formed by the
  // press location and the current location.
  if (is_selecting_region_) {
    UpdateCaptureRegion(
        GetRectEnclosingPoints({initial_location_in_root_, location_in_root},
                               current_root_),
        /*is_resizing=*/true, /*by_user=*/true,
        /*root_window_will_shutdown=*/false);
    return;
  }

  if (fine_tune_position_ == FineTunePosition::kNone)
    return;

  // For a reposition, offset the old select region by the difference between
  // the current location and the previous location, but do not let the select
  // region go offscreen.
  if (fine_tune_position_ == FineTunePosition::kCenter) {
    gfx::Rect new_capture_region = controller_->user_capture_region();
    new_capture_region.Offset(location_in_root - previous_location_in_root);
    new_capture_region.AdjustToFit(current_root_->bounds());
    UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false,
                        /*by_user=*/true, /*root_window_will_shutdown=*/false);
    return;
  }

  // The new region is defined by the rectangle which encloses the anchor
  // point(s) and |resizing_point|, which is based off of |location_in_root| but
  // prevents edge drags from resizing the region in the non-desired direction.
  std::vector<gfx::Point> points = anchor_points_;
  DCHECK(!points.empty());
  gfx::Point resizing_point = location_in_root;

  // For edge dragging, there will be two anchor points with the same primary
  // axis value. Setting |resizing_point|'s secondary axis value to match either
  // one of the anchor points secondary axis value will ensure that for the
  // duration of a drag, GetRectEnclosingPoints will return a rect whose
  // secondary dimension does not change.
  if (fine_tune_position_ == FineTunePosition::kLeftEdge ||
      fine_tune_position_ == FineTunePosition::kRightEdge) {
    resizing_point.set_y(points.front().y());
  } else if (fine_tune_position_ == FineTunePosition::kTopEdge ||
             fine_tune_position_ == FineTunePosition::kBottomEdge) {
    resizing_point.set_x(points.front().x());
  }
  points.push_back(resizing_point);
  UpdateCaptureRegion(GetRectEnclosingPoints(points, current_root_),
                      /*is_resizing=*/true, /*by_user=*/true,
                      /*root_window_will_shutdown=*/false);
  MaybeShowMagnifierGlassAtPoint(location_in_root);
}

void CaptureModeSession::OnLocatedEventReleased(
    const gfx::Point& location_in_root) {
  // TODO(conniekxu): Handle the opacity of the toast widget when it's
  // overlapped with the capture region.
  if (user_nudge_controller_)
    user_nudge_controller_->SetVisible(true);

  gfx::Point screen_location = location_in_root;
  wm::ConvertPointToScreen(current_root_, &screen_location);
  EndSelection(screen_location);

  // Do a repaint to show the affordance circles.
  RepaintRegion();

  // Run `MaybeUpdateCameraPreviewBounds` when user releases the drag at
  // the exit of this function's scope to show the camera preview which may have
  // been hidden in `OnLocatedEventPressed`. The reason we want to run it at the
  // exit of this function is if `is_selecting_region_` is true, we want to wait
  // until the capture label is updated since capture label's opacity may need
  // to be updated based on if it's overlapped with camera preview or not.
  // Captures `this` by WeakPtr since pressing escape while dragging a capture
  // region will end the session and delete `this`.
  absl::Cleanup deferred_runner = [session = weak_ptr_factory_.GetWeakPtr()] {
    if (session) {
      session->MaybeUpdateCameraPreviewBounds();
    }
  };

  if (ShowDefaultActionButtonsOrPerformSearch()) {
    return;
  }

  if (!is_selecting_region_) {
    return;
  }

  // After first release event, we advance to the next phase.
  is_selecting_region_ = false;

  UpdateCaptureLabelWidget(CaptureLabelAnimation::kRegionPhaseChange);
  // Refresh the action container bounds after the capture label is updated.
  UpdateActionContainerWidget();

  A11yAlertCaptureSource(/*trigger_now=*/true);
}

void CaptureModeSession::UpdateCaptureRegion(
    const gfx::Rect& new_capture_region,
    bool is_resizing,
    bool by_user,
    bool root_window_will_shutdown) {
  const gfx::Rect old_capture_region = controller_->user_capture_region();
  if (old_capture_region == new_capture_region)
    return;

  // Calculate the region that has been damaged and repaint the layer. Add some
  // extra padding to make sure the border and affordance circles are repainted
  // and the glow animation is removed if needed.
  gfx::Rect damage_region = old_capture_region;
  damage_region.Union(new_capture_region);
  if (capture_region_overlay_controller_ &&
      capture_region_overlay_controller_->HasGlowAnimation()) {
    capture_region_overlay_controller_->RemoveGlowAnimation();
    // `kDamageWithGlowOutsetDp` is greater than `kDamageOutsetDp` so it is
    // enough to also cover the border and affordance circles.
    damage_region.Outset(kDamageWithGlowOutsetDp);
  } else {
    damage_region.Outset(active_behavior_->ShouldPaintSunfishCaptureRegion()
                             ? kSunfishRegionDamageOutsetDp
                             : kDamageOutsetDp);
  }
  layer()->SchedulePaint(damage_region);

  controller_->SetUserCaptureRegion(new_capture_region, by_user);
  InvalidateImageSearch();
  ClearActionContainer();
  UpdateDimensionsLabelWidget(is_resizing);
  UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  if (!root_window_will_shutdown) {
    // This updates the virtual views used for a11y on the affordance circles.
    // Those virtual views will be destroyed soon if `root_window_will_shutdown`
    // is true.
    focus_cycler_->OnFineTunePositionUpdated(/*notify_selection_event=*/false);
  }

  // Start a timer to request default actions or perform search after a delay.
  // This is to prevent too many requests if the user needs to repeatedly adjust
  // the capture region. Note that this delay does not apply to region
  // adjustments that end in a mouse release event, since those follow a
  // separate codepath in `OnLocatedEventReleased`.
  image_search_request_timer_.Start(
      FROM_HERE, kImageSearchRequestStartDelay,
      base::BindOnce(
          [](base::WeakPtr<CaptureModeSession> capture_mode_session) {
            if (capture_mode_session) {
              // The following may end the session and delete
              // `capture_mode_session`, but we can ignore the result here.
              std::ignore = capture_mode_session
                                ->ShowDefaultActionButtonsOrPerformSearch();
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeSession::UpdateDimensionsLabelWidget(bool is_resizing) {
  const bool should_not_show =
      !is_resizing || controller_->source() != CaptureModeSource::kRegion ||
      controller_->user_capture_region().IsEmpty();
  if (should_not_show) {
    dimensions_label_widget_.reset();
    return;
  }

  if (!dimensions_label_widget_) {
    auto* parent = GetParentContainer(current_root_);
    dimensions_label_widget_ = std::make_unique<views::Widget>();
    dimensions_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureModeDimensionsLabel"));
    // Disable the default hide animation so that the dimensions label is
    // hidden immediately when destroyed.
    dimensions_label_widget_->SetVisibilityAnimationTransition(
        views::Widget::VisibilityTransition::ANIMATE_SHOW);

    auto size_label = std::make_unique<views::Label>();
    size_label->SetEnabledColor(kColorAshTextColorPrimary);
    size_label->SetBackground(views::CreateRoundedRectBackground(
        kColorAshShieldAndBase80, kSizeLabelBorderRadius));
    size_label->SetAutoColorReadabilityEnabled(false);
    dimensions_label_widget_->SetContentsView(std::move(size_label));

    dimensions_label_widget_->Show();

    // When moving to a new display, the dimensions label gets created/moved
    // onto the new display on press, while the capture bar gets moved on
    // release. In this case, we do not have to stack the dimensions label.
    if (parent == capture_mode_bar_widget_->GetNativeWindow()->parent()) {
      parent->StackChildBelow(dimensions_label_widget_->GetNativeWindow(),
                              capture_mode_bar_widget_->GetNativeWindow());
    }
  }

  views::Label* size_label =
      static_cast<views::Label*>(dimensions_label_widget_->GetContentsView());

  const gfx::Rect capture_region = controller_->user_capture_region();
  size_label->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "%d x %d", capture_region.width(), capture_region.height())));

  UpdateDimensionsLabelBounds();
}

void CaptureModeSession::UpdateDimensionsLabelBounds() {
  DCHECK(dimensions_label_widget_ &&
         dimensions_label_widget_->GetContentsView());

  gfx::Rect bounds(
      dimensions_label_widget_->GetContentsView()->GetPreferredSize());
  const gfx::Rect capture_region = controller_->user_capture_region();
  gfx::Rect screen_region = current_root_->bounds();

  bounds.set_width(bounds.width() + 2 * kSizeLabelHorizontalPadding);
  bounds.set_x(capture_region.CenterPoint().x() - bounds.width() / 2);
  bounds.set_y(capture_region.bottom() + kSizeLabelYDistanceFromRegionDp);

  // The dimension label should always be within the screen and at the bottom of
  // the capture region. If it does not fit below the bottom edge fo the region,
  // move it above the bottom edge into the capture region.
  screen_region.Inset(
      gfx::Insets::TLBR(0, 0, kSizeLabelYDistanceFromRegionDp, 0));
  bounds.AdjustToFit(screen_region);

  wm::ConvertRectToScreen(current_root_, &bounds);
  dimensions_label_widget_->SetBounds(bounds);
}

void CaptureModeSession::MaybeShowMagnifierGlassAtPoint(
    const gfx::Point& location_in_root) {
  if (!capture_mode_util::IsCornerFineTunePosition(fine_tune_position_))
    return;
  magnifier_glass_.ShowFor(current_root_, location_in_root);
}

void CaptureModeSession::CloseMagnifierGlass() {
  magnifier_glass_.Close();
}

std::vector<gfx::Point> CaptureModeSession::GetAnchorPointsForPosition(
    FineTunePosition position) {
  std::vector<gfx::Point> anchor_points;
  // For a vertex, the anchor point is the opposite vertex on the rectangle
  // (ex. bottom left vertex -> top right vertex anchor point). For an edge, the
  // anchor points are the two vertices of the opposite edge (ex. bottom edge ->
  // top left and top right anchor points).
  const gfx::Rect rect = controller_->user_capture_region();
  switch (position) {
    case FineTunePosition::kNone:
    case FineTunePosition::kCenter:
      break;
    case FineTunePosition::kTopLeftVertex:
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopEdge:
      anchor_points.push_back(rect.bottom_left());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopRightVertex:
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kLeftEdge:
      anchor_points.push_back(rect.top_right());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kRightEdge:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kBottomLeftVertex:
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomEdge:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomRightVertex:
      anchor_points.push_back(rect.origin());
      break;
  }
  DCHECK(!anchor_points.empty());
  DCHECK_LE(anchor_points.size(), 2u);
  return anchor_points;
}

void CaptureModeSession::UpdateCaptureLabelWidget(
    CaptureLabelAnimation animation_type) {
  if (!capture_label_widget_) {
    capture_label_widget_ = std::make_unique<views::Widget>();
    auto* parent = GetParentContainer(current_root_);
    capture_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureLabel"));
    capture_label_view_ = capture_label_widget_->SetContentsView(
        std::make_unique<CaptureLabelView>(
            this,
            base::BindRepeating(&CaptureModeSession::DoPerformCapture,
                                base::Unretained(this)),
            base::BindRepeating(
                &CaptureModeSession::OnRecordingTypeDropDownButtonPressed,
                base::Unretained(this))));
    capture_label_widget_->GetNativeWindow()->SetTitle(
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_A11Y_TITLE));
    capture_label_widget_->Show();
  }

  // Note that the order here matters. The bounds of the recording type menu
  // widget is always relative to the bounds of the `capture_label_widget_`.
  // Thus, the latter must be updated before the former. Also, the menu may need
  // to close if the `label_view` becomes not interactable.
  capture_label_view_->UpdateIconAndText();
  UpdateCaptureLabelWidgetBounds(animation_type);
  MaybeUpdateRecordingTypeMenu();

  focus_cycler_->OnCaptureLabelWidgetUpdated();
}

void CaptureModeSession::UpdateCaptureLabelWidgetBounds(
    CaptureLabelAnimation animation_type) {
  DCHECK(capture_label_widget_);

  const gfx::Rect bounds = CalculateCaptureLabelWidgetBounds();
  const gfx::Rect old_bounds =
      capture_label_widget_->GetNativeWindow()->GetBoundsInScreen();
  if (old_bounds == bounds)
    return;

  if (animation_type == CaptureLabelAnimation::kNone) {
    capture_label_widget_->SetBounds(bounds);
    return;
  }

  ui::Layer* layer = capture_label_widget_->GetLayer();
  ui::LayerAnimator* animator = layer->GetAnimator();

  if (animation_type == CaptureLabelAnimation::kRegionPhaseChange) {
    capture_label_widget_->SetBounds(bounds);
    const gfx::Point center_point = bounds.CenterPoint();
    layer->SetTransform(
        gfx::GetScaleTransform(gfx::Point(center_point.x() - bounds.x(),
                                          center_point.y() - bounds.y()),
                               kLabelScaleDownOnPhaseChange));
    layer->SetOpacity(0.f);

    ui::ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(kCaptureLabelRegionPhaseChangeDuration);
    settings.SetTweenType(gfx::Tween::ACCEL_LIN_DECEL_100);
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    animator->SchedulePauseForProperties(
        kCaptureLabelRegionPhaseChangeDelay,
        ui::LayerAnimationElement::TRANSFORM |
            ui::LayerAnimationElement::OPACITY);
    layer->SetTransform(gfx::Transform());
    layer->SetOpacity(1.f);
    return;
  }

  DCHECK_EQ(CaptureLabelAnimation::kCountdownStart, animation_type);
  if (!old_bounds.IsEmpty()) {
    // This happens if there is a label or a label button showing when count
    // down starts. In this case we'll do a bounds change animation.
    ui::ScopedLayerAnimationSettings settings(animator);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(kCaptureLabelCountdownStartDuration);
    capture_label_widget_->SetBounds(bounds);
  } else {
    // This happens when no text message was showing when count down starts, in
    // this case we'll do a fade in + shrinking down animation.
    capture_label_widget_->SetBounds(bounds);
    const gfx::Point center_point = bounds.CenterPoint();
    layer->SetTransform(
        gfx::GetScaleTransform(gfx::Point(center_point.x() - bounds.x(),
                                          center_point.y() - bounds.y()),
                               kLabelScaleUpOnCountdown));

    layer->SetOpacity(0.f);

    // Fade in.
    ui::ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(kCaptureLabelCountdownStartDuration);
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(1.f);

    // Scale down from 120% -> 100%.
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer->SetTransform(gfx::Transform());
  }
}

gfx::Rect CaptureModeSession::CalculateCaptureLabelWidgetBounds() {
  DCHECK(capture_label_widget_);
  DCHECK(capture_label_view_);

  const gfx::Size preferred_size = capture_label_view_->GetPreferredSize();
  const gfx::Rect capture_bar_bounds =
      capture_mode_bar_widget_->GetNativeWindow()->bounds();
  // Ignore the action container widget bounds when it is not visible.
  // Otherwise, the capture label widget can move around unexpectedly in order
  // to avoid an invisible widget while the user is adjusting the capture
  // region.
  const gfx::Rect action_container_widget_bounds =
      action_container_widget_ && action_container_widget_->IsVisible()
          ? action_container_widget_->GetNativeWindow()->bounds()
          : gfx::Rect();

  // Calculates the bounds for when the capture label is not placed in the
  // middle of the screen.
  auto calculate_bounds = [&preferred_size, &capture_bar_bounds,
                           &action_container_widget_bounds](
                              const gfx::Rect& capture_bounds,
                              aura::Window* root) {
    // The capture_bounds must be at least the size of |preferred_size| plus
    // some padding for the capture label to be centered inside it.
    gfx::Rect label_bounds(capture_bounds);
    gfx::Size capture_bounds_min_size = preferred_size;
    capture_bounds_min_size.Enlarge(kCaptureRegionMinimumPaddingDp,
                                    kCaptureRegionMinimumPaddingDp);
    // If the label fits into |capture_bounds| with a comfortable padding, and
    // does not intersect the capture bar, we're good.
    if (label_bounds.width() > capture_bounds_min_size.width() &&
        label_bounds.height() > capture_bounds_min_size.height()) {
      label_bounds.ClampToCenteredSize(preferred_size);
      if (!label_bounds.Intersects(capture_bar_bounds))
        return label_bounds;
    }

    return CalculateRegionEdgeBounds(preferred_size, capture_bar_bounds,
                                     capture_bounds,
                                     action_container_widget_bounds, root,
                                     CaptureRegionWidgetAlignment::kCenter);
  };

  gfx::Rect bounds(current_root_->bounds());
  const gfx::Rect capture_region = controller_->user_capture_region();
  const gfx::Rect window_bounds = GetSelectedWindowTargetBounds();
  const CaptureModeSource source = controller_->source();

  // For fullscreen mode, the capture label is placed in the middle of the
  // screen. For region capture mode, if it's in select phase, the capture label
  // is also placed in the middle of the screen, and if it's in fine tune phase,
  // the capture label is ideally placed in the middle of the capture region. If
  // it cannot fit, then it will be placed slightly above or below the capture
  // region. For window capture mode, it is the same as the region capture mode
  // fine tune phase logic, in that it will first try to place the label in the
  // middle of the selected window bounds, otherwise it will be placed slightly
  // away from one of the edges of the selected window.
  if (source == CaptureModeSource::kRegion && !is_selecting_region_ &&
      !capture_region.IsEmpty()) {
    if (capture_label_view_->IsInCountDownAnimation()) {
      // If countdown starts, calculate the bounds based on the old capture
      // label's position, otherwise, since the countdown label bounds is
      // smaller than the label bounds and may fit into the capture region even
      // if the old capture label doesn't fit thus was place outside of the
      // capture region, it's possible that we see the countdown label animates
      // to inside of the capture region from outside of the capture region.
      bounds = capture_label_widget_->GetNativeWindow()->bounds();
      bounds.ClampToCenteredSize(preferred_size);
    } else {
      bounds = calculate_bounds(capture_region, current_root_);
    }
  } else if (source == CaptureModeSource::kWindow && !window_bounds.IsEmpty()) {
    bounds = calculate_bounds(window_bounds, current_root_);
  } else {
    bounds.ClampToCenteredSize(preferred_size);
  }
  // User capture bounds are in root window coordinates so convert them here.
  wm::ConvertRectToScreen(current_root_, &bounds);
  return bounds;
}

bool CaptureModeSession::ShouldCaptureLabelHandleEvent(
    aura::Window* event_target) {
  if (!capture_label_widget_ ||
      capture_label_widget_->GetNativeWindow() != event_target) {
    return false;
  }

  DCHECK(capture_label_view_);
  return capture_label_view_->ShouldHandleEvent();
}

void CaptureModeSession::UpdateRootWindowDimmers() {
  root_window_dimmers_.clear();

  // Add dimmers for all root windows except |current_root_| if needed.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    if (root_window == current_root_) {
      continue;
    }

    auto dimmer = std::make_unique<WindowDimmer>(root_window);
    dimmer->SetDimColor(capture_mode::kDimmingShieldColor);
    dimmer->window()->Show();
    root_window_dimmers_.emplace(std::move(dimmer));
  }
}

bool CaptureModeSession::IsUsingCustomCursor(CaptureModeType type) const {
  return cursor_setter_->IsUsingCustomCursor(static_cast<int>(type));
}

void CaptureModeSession::ClampCaptureRegionToRootWindowSize() {
  // Invalidate any ongoing image search before the new capture region is
  // applied, so that loading animations can be removed by scheduling a repaint
  // around the old capture bounds if needed.
  InvalidateImageSearch();

  gfx::Rect new_capture_region = controller_->user_capture_region();
  new_capture_region.AdjustToFit(current_root_->bounds());
  controller_->SetUserCaptureRegion(new_capture_region, /*by_user=*/false);
}

void CaptureModeSession::EndSelection(
    std::optional<gfx::Point> cursor_screen_location) {
  fine_tune_position_ = FineTunePosition::kNone;
  anchor_points_.clear();

  is_drag_in_progress_ = false;
  Shell::Get()->UpdateCursorCompositingEnabled();

  MaybeUpdateCaptureUisOpacity(cursor_screen_location);
  UpdateActionContainerWidget();
  UpdateDimensionsLabelWidget(/*is_resizing=*/false);
  CloseMagnifierGlass();
  InvalidateImageSearch();
}

void CaptureModeSession::RepaintRegion() {
  gfx::Rect damage_region = controller_->user_capture_region();
  damage_region.Outset(active_behavior_->ShouldPaintSunfishCaptureRegion()
                           ? kSunfishRegionDamageOutsetDp
                           : kDamageOutsetDp);
  layer()->SchedulePaint(damage_region);
}

void CaptureModeSession::SelectDefaultRegion() {
  is_selecting_region_ = false;

  // Default is centered in the root, and its width and height are
  // |kRegionDefaultRatio| size of the root.
  gfx::Rect default_capture_region = current_root_->bounds();
  default_capture_region.ClampToCenteredSize(gfx::ScaleToCeiledSize(
      default_capture_region.size(), kRegionDefaultRatio));
  UpdateCaptureRegion(default_capture_region, /*is_resizing=*/false,
                      /*by_user=*/true, /*root_window_will_shutdown=*/false);
  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_DEFAULT_REGION_SELECTED);
}

void CaptureModeSession::UpdateRegionForArrowKeys(ui::KeyboardCode key_code,
                                                  int event_flags) {
  CHECK(focus_cycler_);
  const FineTunePosition focused_fine_tune_position =
      focus_cycler_->GetFocusedFineTunePosition();
  if (focused_fine_tune_position == FineTunePosition::kNone) {
    return;
  }

  switch (key_code) {
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT:
      if (focused_fine_tune_position == FineTunePosition::kTopEdge ||
          focused_fine_tune_position == FineTunePosition::kBottomEdge) {
        return;
      }
      break;
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      if (focused_fine_tune_position == FineTunePosition::kLeftEdge ||
          focused_fine_tune_position == FineTunePosition::kRightEdge) {
        return;
      }
      break;
    default:
      NOTREACHED();
  }

  const bool horizontal =
      key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT;
  const int change = GetArrowKeyPressChange(event_flags);
  gfx::Rect new_capture_region = controller_->user_capture_region();

  if (focused_fine_tune_position == FineTunePosition::kCenter) {
    // Shift the whole capture region if we are focused on it.
    if (horizontal) {
      new_capture_region.Offset(key_code == ui::VKEY_LEFT ? -change : change,
                                0);
    } else {
      new_capture_region.Offset(0, key_code == ui::VKEY_UP ? -change : change);
    }
    new_capture_region.AdjustToFit(current_root_->bounds());
  } else {
    const gfx::Point location =
        capture_mode_util::GetLocationForFineTunePosition(
            new_capture_region, focused_fine_tune_position);

    // If an affordance circle on the left/top side of the capture region is
    // focused, left/up presses will enlarge the existing region and right/down
    // presses will shrink the existing region. If it is on the right/bottom
    // side, right/down presses will enlarge and left/up presses will shrink.
    // Does nothing if shrinking will cause the new capture region to become
    // empty.
    gfx::Insets insets;
    if (horizontal) {
      const bool affordance_on_left = location.x() == new_capture_region.x();
      const bool shrink = affordance_on_left ^ (key_code == ui::VKEY_LEFT);
      if (shrink && new_capture_region.width() < change) {
        return;
      }
      const int inset = shrink ? change : -change;
      insets = gfx::Insets::TLBR(0, affordance_on_left ? inset : 0, 0,
                                 affordance_on_left ? 0 : inset);
    } else {
      const bool affordance_on_top = location.y() == new_capture_region.y();
      const bool shrink = affordance_on_top ^ (key_code == ui::VKEY_UP);
      if (shrink && new_capture_region.height() < change) {
        return;
      }
      const int inset = shrink ? change : -change;
      insets = gfx::Insets::TLBR(affordance_on_top ? inset : 0, 0,
                                 affordance_on_top ? 0 : inset, 0);
    }

    new_capture_region.Inset(insets);
    new_capture_region.Intersect(current_root_->bounds());
  }

  UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false,
                      /*by_user=*/true, /*root_window_will_shutdown=*/false);
}

void CaptureModeSession::MaybeReparentCameraPreviewWidget() {
  if (!controller_->is_recording_in_progress())
    controller_->camera_controller()->MaybeReparentPreviewWidget();
}

void CaptureModeSession::MaybeUpdateCameraPreviewBounds() {
  if (!controller_->is_recording_in_progress()) {
    controller_->camera_controller()->MaybeUpdatePreviewWidget(
        /*animate=*/false);
  }
}

void CaptureModeSession::SetRecordingTypeMenuShown(bool shown,
                                                   bool by_key_event) {
  if (!shown) {
    recording_type_menu_widget_.reset();
    recording_type_menu_view_ = nullptr;
    return;
  }

  if (!recording_type_menu_widget_) {
    DCHECK(capture_label_widget_);
    DCHECK(capture_label_widget_->IsVisible());

    // Close the settings widget if any. Only one menu at a time can be visible.
    SetSettingsMenuShown(false);

    auto* parent = GetParentContainer(current_root_);
    recording_type_menu_widget_ = std::make_unique<views::Widget>();
    capture_toast_controller_.DismissCurrentToastIfAny();

    recording_type_menu_widget_->Init(
        CreateWidgetParams(parent,
                           RecordingTypeMenuView::GetIdealScreenBounds(
                               capture_label_widget_->GetWindowBoundsInScreen(),
                               current_root_->GetBoundsInScreen()),
                           "RecordingTypeMenuWidget"));
    recording_type_menu_view_ = recording_type_menu_widget_->SetContentsView(
        std::make_unique<RecordingTypeMenuView>(
            base::BindRepeating(&CaptureModeSession::SetRecordingTypeMenuShown,
                                weak_ptr_factory_.GetWeakPtr(), /*shown=*/false,
                                /*by_key_event=*/false)));

    auto* menu_window = recording_type_menu_widget_->GetNativeWindow();
    parent->StackChildAtTop(menu_window);

    menu_window->SetTitle(l10n_util::GetStringUTF16(
        IDS_ASH_SCREEN_CAPTURE_RECORDING_TYPE_MENU_A11Y_TITLE));
    focus_cycler_->OnMenuOpened(
        recording_type_menu_widget_.get(),
        CaptureModeSessionFocusCycler::FocusGroup::kPendingRecordingType,
        by_key_event);
  }

  recording_type_menu_widget_->Show();
}

bool CaptureModeSession::IsPointOnRecordingTypeDropDownButton(
    const gfx::Point& screen_location) const {
  if (!capture_label_widget_ || !capture_label_widget_->IsVisible())
    return false;

  DCHECK(capture_label_view_);
  return capture_label_view_->IsPointOnRecordingTypeDropDownButton(
      screen_location);
}

void CaptureModeSession::MaybeUpdateRecordingTypeMenu() {
  if (!recording_type_menu_widget_)
    return;

  // If the the drop down button becomes hidden, the recording type menu widget
  // should also hide.
  if (!capture_label_widget_ ||
      !capture_label_view_->IsRecordingTypeDropDownButtonVisible()) {
    SetRecordingTypeMenuShown(false);
    return;
  }

  recording_type_menu_widget_->SetBounds(
      RecordingTypeMenuView::GetIdealScreenBounds(
          capture_label_widget_->GetWindowBoundsInScreen(),
          current_root_->GetBoundsInScreen(),
          recording_type_menu_widget_->GetContentsView()));
}

bool CaptureModeSession::IsPointOverSelectedWindow(
    const gfx::Point& screen_point) const {
  auto* selected_window = GetSelectedWindow();
  return selected_window &&
         (capture_mode_util::GetTopMostCapturableWindowAtPoint(screen_point) ==
          selected_window);
}

// TODO(http://b/363069895): Upload strings for translation.
void CaptureModeSession::UpdateActionContainerWidget() {
  if (!ShouldShowActionContainerWidget()) {
    if (action_container_widget_ && action_container_widget_->IsVisible()) {
      // It is inefficient to destroy and recreate the widget if a drag is in
      // progress.
      action_container_view_->ClearContainer();
      action_container_widget_->Hide();
    }
    return;
  }

  if (!action_container_widget_) {
    action_container_widget_ = std::make_unique<views::Widget>();
    auto* parent = GetParentContainer(current_root_);
    action_container_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "ActionButtonsContainer"));
    action_container_widget_->widget_delegate()->SetTitle(
        active_behavior_->GetActionButtonContainerTitle());

    action_container_view_ = action_container_widget_->SetContentsView(
        std::make_unique<ActionButtonContainerView>());
  }

  if (action_container_view_->GetPreferredSize().IsEmpty()) {
    action_container_widget_->Hide();
    return;
  }

  action_container_widget_->Show();

  const gfx::Rect bounds = CalculateActionContainerWidgetBounds();
  if (bounds != action_container_widget_->GetWindowBoundsInScreen()) {
    action_container_widget_->SetBounds(bounds);
  }
}

gfx::Rect CaptureModeSession::CalculateActionContainerWidgetBounds() const {
  DCHECK(action_container_widget_);

  const gfx::Size preferred_size = action_container_view_->GetPreferredSize();
  const gfx::Rect capture_bar_bounds =
      capture_mode_bar_widget_->GetNativeWindow()->bounds();
  // If the capture label exists, take its bounds into consideration regardless
  // of whether it is currently visible or not. Otherwise there may be overlap
  // if the action container and capture label both appear at the same time.
  const gfx::Rect capture_label_widget_bounds =
      capture_label_widget_ ? capture_label_widget_->GetNativeWindow()->bounds()
                            : gfx::Rect();
  const gfx::Rect capture_region = controller_->user_capture_region();
  gfx::Rect bounds = CalculateRegionEdgeBounds(
      preferred_size, capture_bar_bounds, capture_region,
      capture_label_widget_bounds, current_root_,
      CaptureRegionWidgetAlignment::kRight);

  // User capture bounds are in root window coordinates so convert them here.
  wm::ConvertRectToScreen(current_root_, &bounds);
  return bounds;
}

void CaptureModeSession::ClearActionContainer() {
  if (action_container_widget_) {
    CHECK(action_container_view_);
    action_container_view_->ClearContainer();
    UpdateActionContainerWidget();
  }
}

[[nodiscard]] bool
CaptureModeSession::ShowDefaultActionButtonsOrPerformSearch() {
  // Cancel previous search requests if needed.
  InvalidateImageSearch();

  // Early exit if we can't show the action container, i.e. a drag is in
  // progress or capture region is empty. This will be checked again if an
  // asynchronous function invokes `AddActionButton()`.
  if (!ShouldShowActionContainerWidgetWithoutFeatureChecks()) {
    return false;
  }

  // Separate out the feature checks so metrics can be captured.
  if (!CanShowSunfishOrScannerUi()) {
    if (active_behavior_->behavior_type() == BehaviorType::kDefault) {
      RecordScannerFeatureUserState(
          ScannerFeatureUserState::
              kSmartActionsButtonNotShownDueToFeatureChecks);
    }
    return false;
  }

  // `CanShowSunfishOrScannerUi()` checks if *either* Scanner or Sunfish is
  // enabled. Check again if Sunfish specifically is enabled to show the Search
  // button.
  // TODO: crbug.com/397521940 - Replace this with a more precise Sunfish check.
  if (active_behavior_->ShouldShowDefaultActionButtonsInActionContainer() &&
      CanShowSunfishUi()) {
    if (controller_->IsNetworkConnectionOffline()) {
      ShowActionContainerError(l10n_util::GetStringUTF16(
          IDS_ASH_SCREEN_CAPTURE_MORE_ACTIONS_UNAVAILABLE_OFFLINE_ERROR));
    } else {
      RecordSearchButtonShown();
      capture_mode_util::AddActionButton(
          base::BindRepeating(&CaptureModeSession::OnSearchButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()),
          l10n_util::GetStringUTF16(
              IDS_ASH_SCREEN_CAPTURE_SUNFISH_SEARCH_BUTTON_TITLE),
          &kLensIcon,
          ActionButtonRank(ActionButtonType::kSunfish, /*weight=*/1),
          ActionButtonViewID::kSearchButton);
    }
  }
  // TODO: crbug.com/375261308 - Prevent image search when the region stays the
  // same or is within a throttling QPS after a release event.
  // Notify the behavior that the region was selected or adjusted, in case it
  // needs to do specific handling. Note `this` may be destroyed by
  // `OnRegionSelectedOrAdjusted()`.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  // `this` may be deleted after the following line.
  active_behavior_->OnRegionSelectedOrAdjustedWhenActionContainerShowing();

  return !weak_ptr;
}

bool CaptureModeSession::ShouldShowActionContainerWidgetWithoutFeatureChecks()
    const {
  // If drag for capture region is in progress, action buttons should be
  // hidden.
  if (is_drag_in_progress_) {
    return false;
  }

  // If our capture source is not `kRegion`, or the region is empty, hide
  // the action buttons.
  if (controller_->type() != CaptureModeType::kImage ||
      controller_->source() != CaptureModeSource::kRegion ||
      controller_->user_capture_region().IsEmpty()) {
    return false;
  }

  if (!active_behavior_->CanShowActionButtons()) {
    return false;
  }

  return true;
}

bool CaptureModeSession::ShouldShowActionContainerWidget() const {
  return ShouldShowActionContainerWidgetWithoutFeatureChecks() &&
         CanShowSunfishOrScannerUi();
}

void CaptureModeSession::MaybeRemoveGlowAnimation() {
  if (capture_region_overlay_controller_ &&
      capture_region_overlay_controller_->HasGlowAnimation()) {
    capture_region_overlay_controller_->RemoveGlowAnimation();
    // Schedule repaint to remove glow.
    RefreshGlowRegion();
  }
}

void CaptureModeSession::RefreshGlowRegion() {
  gfx::Rect glow_bounds(controller_->user_capture_region());
  glow_bounds.Outset(kDamageWithGlowOutsetDp);
  layer()->SchedulePaint(glow_bounds);
}

void CaptureModeSession::InvalidateImageSearch() {
  weak_token_factory_.InvalidateWeakPtrs();
  image_search_request_timer_.Stop();
  MaybeRemoveGlowAnimation();
}

void CaptureModeSession::InitInternal() {
  layer()->set_delegate(this);
  auto* parent = GetParentContainer(current_root_);
  parent_container_observer_ =
      std::make_unique<ParentContainerObserver>(parent, this);
  parent->layer()->Add(layer());
  layer()->SetBounds(parent->bounds());
  layer()->SetName("CaptureModeSession");

  // Trigger this before creating `capture_mode_bar_widget_` as we want to read
  // out this message before reading out the first view of
  // `capture_mode_bar_widget_`.
  if (!active_behavior_->NeedsDisclaimerOnInit()) {
    capture_mode_util::TriggerAccessibilityAlert(
        active_behavior_->GetCaptureModeOpenAnnouncement());
  }

  // A context menu may have input capture when entering a session. Remove
  // capture from it, otherwise subsequent mouse events will cause it to close,
  // and then we won't be able to take a screenshot of the menu. Store it so we
  // can return capture to it when exiting the session.
  // Note that some windows gets destroyed when they lose the capture (e.g. a
  // window created for capturing events while drag-drop in progress), so we
  // need to account for that.
  if (auto* capture_client = aura::client::GetCaptureClient(current_root_)) {
    input_capture_window_ = capture_client->GetCaptureWindow();
    if (input_capture_window_) {
      aura::WindowTracker tracker({input_capture_window_.get()});
      capture_client->ReleaseCapture(input_capture_window_);
      if (tracker.windows().empty()) {
        input_capture_window_ = nullptr;
      } else {
        input_capture_window_->AddObserver(this);
      }
    }
  }

  // The last region selected could have been on a larger display. Ensure that
  // the region is not larger than the current display.
  ClampCaptureRegionToRootWindowSize();

  capture_mode_bar_widget_->Init(
      CreateWidgetParams(GetParentContainer(current_root_),
                         active_behavior_->GetCaptureBarBounds(current_root_),
                         "CaptureModeBarWidget"));
  capture_mode_bar_view_ = capture_mode_bar_widget_->SetContentsView(
      active_behavior_->CreateCaptureModeBarView());
  std::u16string capture_mode_bar_a11y_title =
      active_behavior_->GetCaptureModeBarTitle();
  capture_mode_bar_widget_->GetNativeWindow()->SetTitle(
      capture_mode_bar_a11y_title);
  capture_mode_bar_widget_->widget_delegate()->SetAccessibleTitle(
      capture_mode_bar_a11y_title);
  capture_mode_bar_widget_->Show();

  // Advance focus once if spoken feedback is on so that the capture bar takes
  // spoken feedback focus.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    focus_cycler_->AdvanceFocus(/*reverse=*/false);
  }

  UpdateActionContainerWidget();
  if (ShowDefaultActionButtonsOrPerformSearch()) {
    return;
  }
  // Update the capture label widget after the action buttons, as the action
  // buttons get priority around the edges of the capture region.
  if (!active_behavior_->NeedsDisclaimerOnInit()) {
    // Don't create the capture label yet if a disclaimer needs to be shown, to
    // avoid the label being announced while the disclaimer is still visible.
    UpdateCaptureLabelWidget(CaptureLabelAnimation::kNone);
  }

  UpdateCursor(display::Screen::Get()->GetCursorScreenPoint(),
               /*is_touch=*/false);

  if (controller_->source() == CaptureModeSource::kWindow) {
    capture_window_observer_ = std::make_unique<CaptureWindowObserver>(this);
  }

  UpdateRootWindowDimmers();

  Observe(ColorUtil::GetColorProviderSourceForWindow(current_root_));

  display_observer_.emplace(this);
  // Our event handling code assumes the capture bar widget has been initialized
  // already. So we start handling events after everything has been setup.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);

  UpdateFloatingPanelBoundsIfNeeded();

  // `OnCaptureTypeChanged()` should be called after the initialization of the
  // capture bar rather than in that of the capture mode type view, since
  // `OnCaptureTypeChanged()` may trigger `ShowCaptureToast()` which has
  // dependencies on the capture bar.
  // Also please note we should call `OnCaptureTypeChanged()` in
  // `CaptureModeBarView` instead of `CaptureModeSession`, since this is during
  // the initialization of the capture session, the type change is not triggered
  // by the user.
  capture_mode_bar_view_->OnCaptureTypeChanged(controller_->type());
  MaybeCreateSunfishRegionNudge();

  if (active_behavior_->ShouldRegionOverlayBeAllowed()) {
    capture_region_overlay_controller_ =
        std::make_unique<CaptureRegionOverlayController>();
  }
}

void CaptureModeSession::ShutdownInternal() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
  capture_region_overlay_controller_.reset();
  InvalidateImageSearch();
  display_observer_.reset();
  user_nudge_controller_.reset();
  capture_window_observer_.reset();

  Observe(nullptr);

  if (input_capture_window_) {
    input_capture_window_->RemoveObserver(this);
    if (auto* client = aura::client::GetCaptureClient(
            input_capture_window_->GetRootWindow())) {
      client->SetCapture(input_capture_window_);
    }
  }

  // This may happen if we hit esc while dragging.
  if (old_mouse_warp_status_) {
    SetMouseWarpEnabled(*old_mouse_warp_status_);
  }

  // Clear all the contents view of all the widgets to avoid UAF.
  capture_mode_bar_view_ = nullptr;
  capture_mode_settings_view_ = nullptr;
  capture_label_view_ = nullptr;
  recording_type_menu_view_ = nullptr;
  action_container_view_ = nullptr;

  // Close all widgets immediately to avoid having them show up in the captured
  // screenshots or video.
  for (auto* widget : GetAvailableWidgets()) {
    widget->CloseNow();
  }

  if (a11y_alert_on_session_exit_) {
    capture_mode_util::TriggerAccessibilityAlert(
        IDS_ASH_SCREEN_CAPTURE_ALERT_CLOSE);
  }
  UpdateFloatingPanelBoundsIfNeeded();
}

void CaptureModeSession::OnSearchResultsPanelCreated(
    views::Widget* panel_widget) {
  focus_cycler_->OnSearchResultsPanelCreated(panel_widget);
}

bool CaptureModeSession::TakeFocusForSearchResultsPanel(bool reverse) {
  focus_cycler_->AdvanceFocusAfterSearchResultsPanel(reverse);
  return true;
}

void CaptureModeSession::ClearPseudoFocus() {
  focus_cycler_->ClearFocus();
}

void CaptureModeSession::SetA11yOverrideWindowToSearchResultsPanel() {
  focus_cycler_->SetA11yOverrideWindowToSearchResultsPanel();
}

}  // namespace ash
