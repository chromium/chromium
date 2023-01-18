// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"

#include <cmath>

#include "base/debug/stack_trace.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"

namespace arc::input_overlay {

namespace {

constexpr int kTouchPointShadowElevation = 5;

constexpr int kDotCenterDiameter = 14;
constexpr int kDotInsideStrokeThickness = 1;
constexpr int kDotOutsideStrokeThickness = 3;

constexpr int kCrossCenterLength = 78;
constexpr int kCrossInsideStrokeThickness = 1;
constexpr int kCrossOutsideStrokeThickness = 4;
constexpr int kCrossCornerRadius = 6;

// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -6;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;
constexpr SkColor kFocusRingColor = gfx::kGoogleBlue300;

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

SkPath DrawCrossCenter(gfx::Canvas* canvas) {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength),
      /*mid_length=*/SkIntToScalar(kCrossCenterLength / 3),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/SkIntToScalar(0));
}

SkPath DrawCrossInsideStroke(gfx::Canvas* canvas) {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength),
      /*mid_length=*/SkIntToScalar(kCrossCenterLength / 3),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/SkIntToScalar(kCrossInsideStrokeThickness));
}

SkPath DrawCrossOutsideStroke(gfx::Canvas* canvas) {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength +
                                       2 * kCrossInsideStrokeThickness),
      /*mid_length=*/
      SkIntToScalar(kCrossCenterLength / 3 + 2 * kCrossInsideStrokeThickness),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/SkIntToScalar(kCrossOutsideStrokeThickness));
}

class Background : public views::Background {
 public:
  explicit Background(SkColor color) { SetNativeControlColor(color); }
  ~Background() override = default;
};

class CrossCenterBackground : public Background {
 public:
  explicit CrossCenterBackground(SkColor color) : Background(color) {}
  ~CrossCenterBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawPath(DrawCrossCenter(canvas), flags);
  }
};

class DotCenterBackground : public Background {
 public:
  explicit DotCenterBackground(SkColor color) : Background(color) {}
  ~DotCenterBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    int radius = kDotCenterDiameter / 2;
    canvas->DrawCircle(gfx::Point(radius, radius), radius, flags);
  }
};

class CrossInsideStrokeBackground : public Background {
 public:
  explicit CrossInsideStrokeBackground(SkColor color) : Background(color) {}
  ~CrossInsideStrokeBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kCrossInsideStrokeThickness);
    flags.setColor(get_color());
    canvas->DrawPath(DrawCrossInsideStroke(canvas), flags);
  }
};

class DotInsideStrokeBackground : public Background {
 public:
  explicit DotInsideStrokeBackground(SkColor color) : Background(color) {}
  ~DotInsideStrokeBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kDotInsideStrokeThickness);
    flags.setColor(get_color());
    int radius = kDotCenterDiameter / 2;
    int center = radius + kDotInsideStrokeThickness;
    canvas->DrawCircle(gfx::Point(center, center), radius, flags);
  }
};

class CrossOutsideStrokeBackground : public Background {
 public:
  explicit CrossOutsideStrokeBackground(SkColor color) : Background(color) {}
  ~CrossOutsideStrokeBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kCrossOutsideStrokeThickness);
    flags.setColor(kOutsideStrokeColor);
    canvas->DrawPath(DrawCrossOutsideStroke(canvas), flags);
  }
};

class DotOutsideStrokeBackground : public Background {
 public:
  explicit DotOutsideStrokeBackground(SkColor color) : Background(color) {}
  ~DotOutsideStrokeBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kDotOutsideStrokeThickness);
    flags.setColor(get_color());
    int radius = kDotCenterDiameter / 2 + kDotInsideStrokeThickness;
    int center = radius + kDotOutsideStrokeThickness;
    canvas->DrawCircle(gfx::Point(center, center), radius, flags);
  }
};

class CrossCenter : public TouchPointElement {
 public:
  CrossCenter() {
    SetToDefault();
    int temp = kCrossInsideStrokeThickness + kCrossOutsideStrokeThickness;
    SetPosition(gfx::Point(temp, temp));
    SetSize(gfx::Size(kCrossCenterLength, kCrossCenterLength));
  }
  ~CrossCenter() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(std::make_unique<CrossCenterBackground>(kCenterColor));
  }

  void SetToHover() override {
    SetBackground(std::make_unique<CrossCenterBackground>(
        color_utils::GetResultingPaintColor(kCenterColorHover20White,
                                            kCenterColor)));
  }

  void SetToDrag() override {
    SetBackground(std::make_unique<CrossCenterBackground>(
        color_utils::GetResultingPaintColor(kCenterColorDrag30White,
                                            kCenterColor)));
  }
};

class DotCenter : public TouchPointElement {
 public:
  DotCenter() {
    SetToDefault();
    int temp = kDotOutsideStrokeThickness + kDotInsideStrokeThickness;
    SetPosition(gfx::Point(temp, temp));
    SetSize(gfx::Size(kDotCenterDiameter, kDotCenterDiameter));
  }
  ~DotCenter() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(std::make_unique<DotCenterBackground>(kCenterColor));
  }

  void SetToHover() override {
    SetBackground(std::make_unique<DotCenterBackground>(
        color_utils::GetResultingPaintColor(kCenterColorHover20White,
                                            kCenterColor)));
  }

  void SetToDrag() override {
    SetBackground(std::make_unique<DotCenterBackground>(
        color_utils::GetResultingPaintColor(kCenterColorDrag30White,
                                            kCenterColor)));
  }
};

// Because inside stroke is on top of the touch point. Use the inside stroke to
// forward the mouse and gesture events to its parent which is touch point.
class InsideStroke : public TouchPointElement {
 public:
  InsideStroke() = default;
  ~InsideStroke() override = default;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    static_cast<TouchPoint*>(parent())->ApplyMouseEntered(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    static_cast<TouchPoint*>(parent())->ApplyMouseExited(event);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    return static_cast<TouchPoint*>(parent())->ApplyMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    return static_cast<TouchPoint*>(parent())->ApplyMouseDragged(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    static_cast<TouchPoint*>(parent())->ApplyMouseReleased(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    return static_cast<TouchPoint*>(parent())->ApplyGestureEvent(event);
  }
};

class CrossInsideStroke : public InsideStroke {
 public:
  CrossInsideStroke() {
    SetToDefault();
    int temp = kCrossOutsideStrokeThickness;
    SetPosition(gfx::Point(temp, temp));
    temp = kCrossCenterLength + 2 * kCrossInsideStrokeThickness;
    SetSize(gfx::Size(temp, temp));
  }
  ~CrossInsideStroke() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(
        std::make_unique<CrossInsideStrokeBackground>(kInsideStrokeColor));
  }

  void SetToHover() override {
    SetBackground(
        std::make_unique<CrossInsideStrokeBackground>(kInsideStrokeColorHover));
  }

  void SetToDrag() override {
    SetBackground(
        std::make_unique<CrossInsideStrokeBackground>(kInsideStrokeColorDrag));
  }
};

class DotInsideStroke : public InsideStroke {
 public:
  DotInsideStroke() {
    SetToDefault();
    int temp = kDotOutsideStrokeThickness;
    SetPosition(gfx::Point(temp, temp));
    temp = kDotCenterDiameter + 2 * kDotInsideStrokeThickness;
    SetSize(gfx::Size(temp, temp));
  }
  ~DotInsideStroke() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(
        std::make_unique<DotInsideStrokeBackground>(kInsideStrokeColor));
  }

  void SetToHover() override {
    SetBackground(
        std::make_unique<DotInsideStrokeBackground>(kInsideStrokeColorHover));
  }

  void SetToDrag() override {
    SetBackground(
        std::make_unique<DotInsideStrokeBackground>(kInsideStrokeColorDrag));
  }
};

class CrossOutsideStroke : public TouchPointElement {
 public:
  CrossOutsideStroke() {
    SetToDefault();
    SetPosition(gfx::Point(0, 0));
    int temp = kCrossCenterLength + 2 * kCrossInsideStrokeThickness +
               2 * kCrossOutsideStrokeThickness;
    SetSize(gfx::Size(temp, temp));
  }
  ~CrossOutsideStroke() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(
        std::make_unique<CrossOutsideStrokeBackground>(kOutsideStrokeColor));
  }

  void SetToHover() override {
    SetBackground(std::make_unique<CrossOutsideStrokeBackground>(
        kOutsideStrokeColorHover));
  }

  void SetToDrag() override {
    SetBackground(std::make_unique<CrossOutsideStrokeBackground>(
        kOutsideStrokeColorDrag));
  }
};

class DotOutsideStroke : public TouchPointElement {
 public:
  DotOutsideStroke() {
    SetToDefault();
    SetPosition(gfx::Point(0, 0));
    int temp = kDotCenterDiameter + 2 * kDotInsideStrokeThickness +
               2 * kDotOutsideStrokeThickness;
    SetSize(gfx::Size(temp, temp));
  }
  ~DotOutsideStroke() override = default;

  // TouchPointElement:
  void SetToDefault() override {
    SetBackground(
        std::make_unique<DotOutsideStrokeBackground>(kOutsideStrokeColor));
  }

  void SetToHover() override {
    SetBackground(
        std::make_unique<DotOutsideStrokeBackground>(kOutsideStrokeColorHover));
  }

  void SetToDrag() override {
    SetBackground(
        std::make_unique<DotOutsideStrokeBackground>(kOutsideStrokeColorDrag));
  }
};

class CrossTouchPoint : public TouchPoint {
 public:
  explicit CrossTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~CrossTouchPoint() override = default;

  void Init() override {
    touch_outside_stroke_ =
        AddChildView(std::make_unique<CrossOutsideStroke>());
    touch_center_ = AddChildView(std::make_unique<CrossCenter>());
    // Put the inside stroke on top purposely because the thickness is only 1
    // and it doesn't show up obviously probably due to the round issue.
    touch_inside_stroke_ = AddChildView(std::make_unique<CrossInsideStroke>());
    TouchPoint::Init();
  }
};

class DotTouchPoint : public TouchPoint {
 public:
  explicit DotTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~DotTouchPoint() override = default;

  void Init() override {
    touch_outside_stroke_ = AddChildView(std::make_unique<DotOutsideStroke>());
    touch_center_ = AddChildView(std::make_unique<DotCenter>());
    // Put the inside stroke on top purposely because the thickness is only 1
    // and it doesn't show up obviously probably due to the round issue.
    touch_inside_stroke_ = AddChildView(std::make_unique<DotInsideStroke>());
    TouchPoint::Init();
  }
};

}  // namespace

TouchPointElement::TouchPointElement() = default;
TouchPointElement::~TouchPointElement() = default;

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

TouchPoint::TouchPoint(const gfx::Point& center_pos)
    : center_pos_(center_pos) {}

TouchPoint::~TouchPoint() = default;

void TouchPoint::Init() {
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGroup);
  // TODO(b/260868602): Update the name.
  GetViewAccessibility().OverrideName(u"touch point");

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(kFocusRingColor);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);

  auto size = touch_outside_stroke_->size();
  SetSize(size);
  SetPosition(gfx::Point(std::max(0, center_pos_.x() - size.width() / 2),
                         std::max(0, center_pos_.y() - size.height() / 2)));

  std::make_unique<ash::ViewShadow>(this, kTouchPointShadowElevation);
}

void TouchPoint::SetToDefault() {
  touch_outside_stroke_->SetToDefault();
  touch_inside_stroke_->SetToDefault();
  touch_center_->SetToDefault();
}

void TouchPoint::SetToHover() {
  touch_outside_stroke_->SetToHover();
  touch_inside_stroke_->SetToHover();
  touch_center_->SetToHover();
}

void TouchPoint::SetToDrag() {
  touch_outside_stroke_->SetToDrag();
  touch_inside_stroke_->SetToDrag();
  touch_center_->SetToDrag();
}

void TouchPoint::ApplyMouseEntered(const ui::MouseEvent& event) {
  SetToHover();
}

void TouchPoint::ApplyMouseExited(const ui::MouseEvent& event) {
  SetToDefault();
}

bool TouchPoint::ApplyMousePressed(const ui::MouseEvent& event) {
  return static_cast<ActionView*>(parent())->ApplyMousePressed(event);
}

bool TouchPoint::ApplyMouseDragged(const ui::MouseEvent& event) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget)
    widget->SetCursor(ui::mojom::CursorType::kGrabbing);
  SetToDrag();
  return static_cast<ActionView*>(parent())->ApplyMouseDragged(event);
}

void TouchPoint::ApplyMouseReleased(const ui::MouseEvent& event) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget)
    widget->SetCursor(ui::mojom::CursorType::kGrab);
  SetToHover();
  static_cast<ActionView*>(parent())->ApplyMouseReleased(event);
}

void TouchPoint::ApplyGestureEvent(ui::GestureEvent* event) {
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

}  // namespace arc::input_overlay
