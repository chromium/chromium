// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"

#include <cmath>

#include "base/debug/stack_trace.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"

namespace arc::input_overlay {

namespace {

constexpr int kDotCenterDiameter = 14;
constexpr int kDotInsideStrokeThickness = 1;
constexpr int kDotOutsideStrokeThickness = 3;

constexpr int kCrossCenterLength = 78;
constexpr int kCrossCenterMiddleLength = 30;
constexpr int kCrossInsideStrokeThickness = 1;
constexpr int kCrossOutsideStrokeThickness = 4;
constexpr int kCrossCornerRadius = 6;

// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -6;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;

constexpr SkColor kOutsideStrokeColor =
    SkColorSetA(SK_ColorWHITE, 0xCC /*80%*/);
constexpr SkColor kOutsideStrokeColorHover =
    SkColorSetA(SK_ColorWHITE, 0xCC /*80%*/);
constexpr SkColor kOutsideStrokeColorDrag = gfx::kGoogleBlue200;

constexpr SkColor kInsideStrokeColor = SkColorSetA(SK_ColorBLACK, 0x33 /*20%*/);
constexpr SkColor kInsideStrokeColorHover =
    SkColorSetA(SK_ColorBLACK, 0x33 /*20%*/);
constexpr SkColor kInsideStrokeColorDrag =
    SkColorSetA(SK_ColorBLACK, 0x66 /*40%*/);
constexpr SkColor kCenterColor = SkColorSetRGB(0x12, 0x6D, 0xFF);
constexpr SkColor kCenterColorHover20White =
    SkColorSetA(SK_ColorWHITE, 0x33 /*20%*/);
constexpr SkColor kCenterColorDrag30White =
    SkColorSetA(SK_ColorWHITE, 0x4D /*30%*/);

// Draw the cross shape path with round corner. It starts from bottom to up on
// line #0 and draws clock-wisely.
// |overall_length| is the total length of one side excluding the stroke
// thickness. |mid_length| is the length of the middle part which is close to
// the one third of |overall_length|.
//      __
//   _0^  |__
//  |__    __|
//     |__|
//
SkPath DrawCrossPath(SkScalar overall_length,
                     SkScalar mid_length,
                     SkScalar corner_radius,
                     SkScalar out_stroke_thickness) {
  SkPath path;
  SkScalar short_length = (overall_length - mid_length) / 2;
  path.moveTo(short_length + out_stroke_thickness,
              short_length + out_stroke_thickness);
  // #0
  path.rLineTo(0, -(short_length - corner_radius));
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, -corner_radius);
  // #1
  path.rLineTo(mid_length - 2 * corner_radius, 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, corner_radius);
  // #2
  path.rLineTo(0, short_length - corner_radius);
  // #3
  path.rLineTo(short_length - corner_radius, 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, corner_radius);
  // #4
  path.rLineTo(0, mid_length - 2 * corner_radius);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, +corner_radius);
  // #5
  path.rLineTo(-(short_length - corner_radius), 0);
  // #6
  path.rLineTo(0, short_length - corner_radius);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, corner_radius);
  // #7
  path.rLineTo(-(mid_length - 2 * corner_radius), 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, -corner_radius);
  // #8
  path.rLineTo(0, -(short_length - corner_radius));
  // #9
  path.rLineTo(-(short_length - corner_radius), 0);
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -corner_radius, -corner_radius);
  // #10
  path.rLineTo(0, -(mid_length - 2 * corner_radius));
  path.rArcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, corner_radius, -corner_radius);
  // #11
  path.close();
  return path;
}

SkPath DrawCrossCenter() {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength),
      /*mid_length=*/SkIntToScalar(kCrossCenterMiddleLength),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/
      SkIntToScalar(kCrossOutsideStrokeThickness +
                    kCrossInsideStrokeThickness));
}

SkPath DrawCrossOutsideStroke() {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength +
                                       2 * kCrossInsideStrokeThickness),
      /*mid_length=*/
      SkIntToScalar(kCrossCenterMiddleLength + 2 * kCrossInsideStrokeThickness),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/SkIntToScalar(kCrossOutsideStrokeThickness));
}

class CrossTouchPoint : public TouchPoint {
 public:
  explicit CrossTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~CrossTouchPoint() override = default;

  // TouchPoint:
  void Init() override {
    SetAccessibilityProperties(
        ax::mojom::Role::kGroup,
        l10n_util::GetStringUTF16(
            IDS_INPUT_OVERLAY_KEYMAPPING_TOUCH_POINT_CROSS));

    TouchPoint::Init();
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    int size = kCrossCenterLength + 2 * kCrossInsideStrokeThickness +
               2 * kCrossOutsideStrokeThickness;
    return gfx::Size(size, size);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    // Draw outside stroke.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kCrossOutsideStrokeThickness);
    flags.setColor(GetOutsideStrokeColor());
    canvas->DrawPath(DrawCrossOutsideStroke(), flags);
    // Draw center.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(GetCenterColor());
    canvas->DrawPath(DrawCrossCenter(), flags);
    // Draw inside stroke.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kCrossInsideStrokeThickness);
    flags.setColor(GetInsideStrokeColor());
    canvas->DrawPath(DrawCrossCenter(), flags);
  }
};

class DotTouchPoint : public TouchPoint {
 public:
  explicit DotTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~DotTouchPoint() override = default;

  // TouchPoint:
  void Init() override {
    SetAccessibilityProperties(
        ax::mojom::Role::kGroup,
        l10n_util::GetStringUTF16(
            IDS_INPUT_OVERLAY_KEYMAPPING_TOUCH_POINT_DOT));

    TouchPoint::Init();
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    int size = kDotCenterDiameter + 2 * kDotInsideStrokeThickness +
               2 * kDotOutsideStrokeThickness;
    return gfx::Size(size, size);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    int radius = kDotCenterDiameter / 2;
    int center =
        radius + kDotInsideStrokeThickness + kDotOutsideStrokeThickness;

    // Draw outside stroke.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kDotOutsideStrokeThickness);
    flags.setColor(GetOutsideStrokeColor());
    canvas->DrawCircle(gfx::Point(center, center),
                       radius + kDotInsideStrokeThickness, flags);
    // Draw center.
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(GetCenterColor());
    canvas->DrawCircle(gfx::Point(center, center), radius, flags);
    // Draw inside stroke.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kDotInsideStrokeThickness);
    flags.setColor(GetInsideStrokeColor());
    canvas->DrawCircle(gfx::Point(center, center), radius, flags);
  }
};

}  // namespace

// static
TouchPoint* TouchPoint::Show(views::View* parent,
                             ActionType action_type,
                             const gfx::Point& center_pos) {
  std::unique_ptr<TouchPoint> touch_point;
  switch (action_type) {
    case ActionType::TAP:
      touch_point = std::make_unique<DotTouchPoint>(center_pos);
      break;
    case ActionType::MOVE:
      touch_point = std::make_unique<CrossTouchPoint>(center_pos);
      break;
    default:
      NOTREACHED();
  }

  auto* touch_point_ptr =
      parent->AddChildViewAt(std::move(touch_point), /*index=*/0);
  touch_point_ptr->Init();
  return touch_point_ptr;
}

gfx::Size TouchPoint::GetSize(ActionType action_type) {
  int size = 0;
  switch (action_type) {
    case ActionType::TAP:
      size = kDotCenterDiameter + kDotInsideStrokeThickness * 2 +
             kDotOutsideStrokeThickness * 2;
      break;
    case ActionType::MOVE:
      size = kCrossCenterLength + kCrossInsideStrokeThickness * 2 +
             kCrossOutsideStrokeThickness * 2;
      break;
    default:
      NOTREACHED();
  }
  return gfx::Size(size, size);
}

TouchPoint::TouchPoint(const gfx::Point& center_pos)
    : center_pos_(center_pos) {}

TouchPoint::~TouchPoint() = default;

void TouchPoint::Init() {
  SizeToPreferredSize();
  SetPosition(gfx::Point(std::max(0, center_pos_.x() - size().width() / 2),
                         std::max(0, center_pos_.y() - size().height() / 2)));

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshInputOverlayFocusRing);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
}

void TouchPoint::OnCenterPositionChanged(const gfx::Point& point) {
  center_pos_ = point;
  SetPosition(gfx::Point(std::max(0, center_pos_.x() - size().width() / 2),
                         std::max(0, center_pos_.y() - size().height() / 2)));
}

ui::Cursor TouchPoint::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kHand;
}

void TouchPoint::OnMouseEntered(const ui::MouseEvent& event) {
  SetToHover();
}

void TouchPoint::OnMouseExited(const ui::MouseEvent& event) {
  SetToDefault();
}

bool TouchPoint::OnMousePressed(const ui::MouseEvent& event) {
  static_cast<ActionView*>(parent())->ApplyMousePressed(event);
  return true;
}

bool TouchPoint::OnMouseDragged(const ui::MouseEvent& event) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget) {
    widget->SetCursor(ui::mojom::CursorType::kGrabbing);
  }
  SetToDrag();
  static_cast<ActionView*>(parent())->ApplyMouseDragged(event);
  return true;
}

void TouchPoint::OnMouseReleased(const ui::MouseEvent& event) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget) {
    widget->SetCursor(ui::mojom::CursorType::kGrab);
  }
  SetToHover();
  static_cast<ActionView*>(parent())->ApplyMouseReleased(event);
}

void TouchPoint::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      SetToDrag();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      SetToDefault();
      event->SetHandled();
      break;
    default:
      break;
  }

  static_cast<ActionView*>(parent())->ApplyGestureEvent(event);
}

bool TouchPoint::OnKeyPressed(const ui::KeyEvent& event) {
  return static_cast<ActionView*>(parent())->ApplyKeyPressed(event);
}

bool TouchPoint::OnKeyReleased(const ui::KeyEvent& event) {
  return static_cast<ActionView*>(parent())->ApplyKeyReleased(event);
}

void TouchPoint::OnFocus() {
  static_cast<ActionView*>(parent())->ShowFocusInfoMsg(
      l10n_util::GetStringUTF8(
          IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS_TOUCH_POINT_FOCUS),
      this);
}

void TouchPoint::OnBlur() {
  static_cast<ActionView*>(parent())->RemoveMessage();
}

SkColor TouchPoint::GetCenterColor() {
  switch (ui_state_) {
    case UIState::kDefault:
      return kCenterColor;
    case UIState::kHover:
      return color_utils::GetResultingPaintColor(kCenterColorHover20White,
                                                 kCenterColor);
    case UIState::kDrag:
      return color_utils::GetResultingPaintColor(kCenterColorDrag30White,
                                                 kCenterColor);
    default:
      NOTREACHED();
  }
}

SkColor TouchPoint::GetInsideStrokeColor() {
  switch (ui_state_) {
    case UIState::kDefault:
      return kInsideStrokeColor;
    case UIState::kHover:
      return kInsideStrokeColorHover;
    case UIState::kDrag:
      return kInsideStrokeColorDrag;
    default:
      NOTREACHED();
  }
}

SkColor TouchPoint::GetOutsideStrokeColor() {
  switch (ui_state_) {
    case UIState::kDefault:
      return kOutsideStrokeColor;
    case UIState::kHover:
      return kOutsideStrokeColorHover;
    case UIState::kDrag:
      return kOutsideStrokeColorDrag;
    default:
      NOTREACHED();
  }
}

void TouchPoint::SetToDefault() {
  if (ui_state_ == UIState::kDefault) {
    return;
  }
  ui_state_ = UIState::kDefault;
  SchedulePaint();
}

void TouchPoint::SetToHover() {
  if (ui_state_ == UIState::kHover) {
    return;
  }
  ui_state_ = UIState::kHover;
  SchedulePaint();
}

void TouchPoint::SetToDrag() {
  if (ui_state_ == UIState::kDrag) {
    return;
  }
  ui_state_ = UIState::kDrag;
  SchedulePaint();
}

BEGIN_METADATA(TouchPoint, views::View)
END_METADATA

}  // namespace arc::input_overlay
