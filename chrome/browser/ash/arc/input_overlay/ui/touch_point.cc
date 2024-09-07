// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"

#include <cmath>

#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {

// `TouchPoint` consists of outside stroke, inside stroke and center.
constexpr size_t kTouchPointComponentSize = 3u;

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
// `overall_length` is the total length of one side excluding the stroke
// thickness. `mid_length` is the length of the middle part which is close to
// the one third of `overall_length`.
//      __
//   _0^  |__
//  |__    __|
//     |__|
//
SkPath DrawCrossPath(SkScalar overall_length,
                     SkScalar mid_length,
                     SkScalar corner_radius,
                     SkScalar out_stroke_thickness,
                     SkPoint center) {
  SkPath path;
  SkScalar short_length = (overall_length - mid_length) / 2;
  path.moveTo(center.x() - mid_length / 2, center.y() - mid_length / 2);
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

SkPath DrawCrossCenter(const gfx::Point& center) {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength),
      /*mid_length=*/SkIntToScalar(kCrossCenterMiddleLength),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/
      SkIntToScalar(kCrossOutsideStrokeThickness + kCrossInsideStrokeThickness),
      /*center=*/SkPoint::Make(center.x(), center.y()));
}

SkPath DrawCrossOutsideStroke(const gfx::Point& center) {
  return DrawCrossPath(
      /*overall_length=*/SkIntToScalar(kCrossCenterLength +
                                       2 * kCrossInsideStrokeThickness),
      /*mid_length=*/
      SkIntToScalar(kCrossCenterMiddleLength + 2 * kCrossInsideStrokeThickness),
      /*corner_radius=*/SkIntToScalar(kCrossCornerRadius),
      /*out_stroke_thickness=*/SkIntToScalar(kCrossOutsideStrokeThickness),
      /*center=*/SkPoint::Make(center.x(), center.y()));
}

SkColor GetOutsideStrokeColor(const ui::ColorProvider* color_provider,
                              UIState ui_state) {
  switch (ui_state) {
    case UIState::kDefault:
      return IsBeta() ? SkColorSetA(SK_ColorWHITE, GetAlpha(/*percent=*/0.8f))
                      : kOutsideStrokeColor;
    case UIState::kHover:
      return IsBeta() ? SkColorSetA(SK_ColorWHITE, GetAlpha(/*percent=*/0.8f))
                      : kOutsideStrokeColorHover;
    case UIState::kDrag:
      return IsBeta() ? color_provider->GetColor(
                            cros_tokens::kCrosSysGamingControlButtonBorderHover)
                      : kOutsideStrokeColorDrag;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

SkColor GetInsideStrokeColor(const ui::ColorProvider* color_provider,
                             UIState ui_state) {
  switch (ui_state) {
    case UIState::kDefault:
      return IsBeta() ? SkColorSetA(SK_ColorBLACK, GetAlpha(/*percent=*/0.2f))
                      : kInsideStrokeColor;
    case UIState::kHover:
      return IsBeta() ? SkColorSetA(SK_ColorBLACK, GetAlpha(/*percent=*/0.2f))
                      : kInsideStrokeColorHover;
    case UIState::kDrag:
      return IsBeta() ? SkColorSetA(SK_ColorBLACK, GetAlpha(/*percent=*/0.4f))
                      : kInsideStrokeColorDrag;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

SkColor GetCenterColor(const ui::ColorProvider* color_provider,
                       UIState ui_state) {
  switch (ui_state) {
    case UIState::kDefault:
      return IsBeta() ? color_provider->GetColor(
                            cros_tokens::kCrosSysGamingControlButtonDefault)
                      : kCenterColor;
    case UIState::kHover:
      return IsBeta() ? color_provider->GetColor(
                            cros_tokens::kCrosSysGamingControlButtonHover)
                      : color_utils::GetResultingPaintColor(
                            kCenterColorHover20White, kCenterColor);
    case UIState::kDrag:
      return IsBeta() ? color_provider->GetColor(
                            cros_tokens::kCrosSysGamingControlButtonHover)
                      : color_utils::GetResultingPaintColor(
                            kCenterColorDrag30White, kCenterColor);
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

std::array<SkColor, kTouchPointComponentSize> GetColors(
    const ui::ColorProvider* color_provider,
    UIState ui_state) {
  return {GetOutsideStrokeColor(color_provider, ui_state),
          GetInsideStrokeColor(color_provider, ui_state),
          GetCenterColor(color_provider, ui_state)};
}

class CrossTouchPoint : public TouchPoint {
 public:
  explicit CrossTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~CrossTouchPoint() override = default;

  // TouchPoint:
  void Init() override {
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_INPUT_OVERLAY_KEYMAPPING_TOUCH_POINT_CROSS));

    TouchPoint::Init();
  }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return GetSize(ActionType::MOVE);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    PaintBackground(canvas, ActionType::MOVE);
  }
};

class DotTouchPoint : public TouchPoint {
 public:
  explicit DotTouchPoint(const gfx::Point& center_pos)
      : TouchPoint(center_pos) {}
  ~DotTouchPoint() override = default;

  // TouchPoint:
  void Init() override {
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_INPUT_OVERLAY_KEYMAPPING_TOUCH_POINT_DOT));

    TouchPoint::Init();
  }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return GetSize(ActionType::TAP);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    PaintBackground(canvas, ActionType::TAP);
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
      NOTREACHED_IN_MIGRATION();
  }

  auto* touch_point_ptr =
      parent->AddChildViewAt(std::move(touch_point), /*index=*/0);
  touch_point_ptr->Init();
  return touch_point_ptr;
}

// static
int TouchPoint::GetEdgeLength(ActionType action_type) {
  int length = 0;
  switch (action_type) {
    case ActionType::TAP:
      length = kDotCenterDiameter + kDotInsideStrokeThickness * 2 +
               kDotOutsideStrokeThickness * 2;
      break;
    case ActionType::MOVE:
      length = kCrossCenterLength + kCrossInsideStrokeThickness * 2 +
               kCrossOutsideStrokeThickness * 2;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return length;
}

// static
gfx::Size TouchPoint::GetSize(ActionType action_type) {
  const int edge_length = GetEdgeLength(action_type);
  return gfx::Size(edge_length, edge_length);
}

// static
void TouchPoint::DrawTouchPoint(gfx::Canvas* canvas,
                                const ui::ColorProvider* color_provider,
                                ActionType action_type,
                                UIState ui_state,
                                const gfx::Point& center) {
  DCHECK(canvas);
  DCHECK(color_provider);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const auto colors = GetColors(color_provider, ui_state);
  DCHECK_EQ(colors.size(), kTouchPointComponentSize);

  switch (action_type) {
    case ActionType::TAP: {
      const int radius = kDotCenterDiameter / 2;
      const int center_x = center.x();
      const int center_y = center.y();

      // Draw outside stroke.
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kDotOutsideStrokeThickness);
      flags.setColor(colors[0]);
      canvas->DrawCircle(gfx::Point(center_x, center_y),
                         radius + kDotInsideStrokeThickness, flags);
      // Draw center.
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(colors[2]);
      canvas->DrawCircle(gfx::Point(center_x, center_y), radius, flags);
      // Draw inside stroke.
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kDotInsideStrokeThickness);
      flags.setColor(colors[1]);
      canvas->DrawCircle(gfx::Point(center_x, center_y), radius, flags);
    } break;

    case ActionType::MOVE:
      // Draw outside stroke.
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kCrossOutsideStrokeThickness);
      flags.setColor(colors[0]);
      canvas->DrawPath(DrawCrossOutsideStroke(center), flags);
      // Draw center.
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(colors[2]);
      canvas->DrawPath(DrawCrossCenter(center), flags);
      // Draw inside stroke.
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kCrossInsideStrokeThickness);
      flags.setColor(colors[1]);
      canvas->DrawPath(DrawCrossCenter(center), flags);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
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
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    parent_view->ApplyMousePressed(event);
  }
  return true;
}

bool TouchPoint::OnMouseDragged(const ui::MouseEvent& event) {
  // widget is null for test.
  if (auto* widget = GetWidget()) {
    widget->SetCursor(ui::mojom::CursorType::kGrabbing);
  }
  SetToDrag();
  views::AsViewClass<ActionView>(parent())->ApplyMouseDragged(event);
  return true;
}

void TouchPoint::OnMouseReleased(const ui::MouseEvent& event) {
  // widget is null for test.
  if (auto* widget = GetWidget()) {
    widget->SetCursor(ui::mojom::CursorType::kGrab);
  }
  SetToHover();
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    parent_view->ApplyMouseReleased(event);
  }
}

void TouchPoint::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      SetToDrag();
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollEnd:
    case ui::EventType::kScrollFlingStart:
      SetToDefault();
      event->SetHandled();
      break;
    default:
      break;
  }

  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    parent_view->ApplyGestureEvent(event);
  }
}

bool TouchPoint::OnKeyPressed(const ui::KeyEvent& event) {
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    return parent_view->ApplyKeyPressed(event);
  }
  return false;
}

bool TouchPoint::OnKeyReleased(const ui::KeyEvent& event) {
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    return parent_view->ApplyKeyReleased(event);
  }
  return false;
}

void TouchPoint::OnFocus() {
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    parent_view->ShowFocusInfoMsg(
        l10n_util::GetStringUTF8(
            IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS_TOUCH_POINT_FOCUS),
        this);
  }
}

void TouchPoint::OnBlur() {
  if (auto* parent_view = views::AsViewClass<ActionView>(parent())) {
    parent_view->RemoveMessage();
  }
}

void TouchPoint::PaintBackground(gfx::Canvas* canvas, ActionType action_type) {
  const auto size = GetSize(action_type);
  DrawTouchPoint(canvas, GetColorProvider(), action_type, ui_state_,
                 /*center=*/gfx::Point(size.width() / 2, size.height() / 2));
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

BEGIN_METADATA(TouchPoint)
END_METADATA

}  // namespace arc::input_overlay
