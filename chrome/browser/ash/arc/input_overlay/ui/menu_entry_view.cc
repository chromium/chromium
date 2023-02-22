// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include "ash/app_list/app_list_util.h"
#include "ash/style/style_util.h"
#include "base/cxx17_backports.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc::input_overlay {

namespace {
constexpr SkColor kDefaultColor = SkColorSetA(SK_ColorWHITE, 0x99 /*60%*/);
constexpr SkColor kDragColor100White = SK_ColorWHITE;
constexpr SkColor kDragColor60Blue =
    SkColorSetA(gfx::kGoogleBlue300, 0x99 /*60%*/);
constexpr SkColor kHoverColor = SkColorSetA(gfx::kGoogleBlue600, 0x66 /*40%*/);
constexpr SkColor kBorderColor = SkColorSetA(SK_ColorBLACK, 0x33 /*20%*/);

constexpr int kParentPadding = 16;
constexpr int kMenuEntrySize = 48;
constexpr int kMenuEntryIconSize = 24;
constexpr int kMenuEntryCornerRadius = 8;
constexpr int kMenuEntryBorderThickness = 1;
// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -5;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;

// GIO Alpha specs:
// Menu entry size for alpha.
constexpr int kMenuEntrySizeAlpha = 56;
// Gap between focus ring outer edge to label.
constexpr float kHaloInsetAlpha = -4;
// Thickness of focus ring.
constexpr float kHaloThicknessAlpha = 2;

}  // namespace

MenuEntryView::MenuEntryView(
    PressedCallback pressed_callback,
    OnPositionChangedCallback on_position_changed_callback)
    : views::ImageButton(std::move(pressed_callback)),
      on_position_changed_callback_(on_position_changed_callback) {
  auto game_icon = ui::ImageModel::FromVectorIcon(
      allow_reposition_ ? kGameControlsGamepadIcon
                        : vector_icons::kVideogameAssetOutlineIcon,
      SK_ColorBLACK, kMenuEntryIconSize);
  SetImageModel(views::Button::STATE_NORMAL, game_icon);
  SetBackground(views::CreateRoundedRectBackground(kDefaultColor,
                                                   kMenuEntryCornerRadius));

  if (allow_reposition_) {
    SetBorder(views::CreateRoundedRectBorder(
        kMenuEntryBorderThickness, kMenuEntryCornerRadius, kBorderColor));
  }

  SetSize(allow_reposition_
              ? gfx::Size(kMenuEntrySize, kMenuEntrySize)
              : gfx::Size(kMenuEntrySizeAlpha, kMenuEntrySizeAlpha));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  // Set up focus ring for |menu_entry_|.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kMenuEntryCornerRadius);
  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/true);
  auto* focus_ring = views::FocusRing::Get(this);
  if (allow_reposition_) {
    focus_ring->SetHaloInset(kHaloInset);
    focus_ring->SetHaloThickness(kHaloThickness);
    focus_ring->SetColorId(ui::kColorAshInputOverlayFocusRing);
  } else {
    focus_ring->SetHaloInset(kHaloInsetAlpha);
    focus_ring->SetHaloThickness(kHaloThicknessAlpha);
    focus_ring->SetColorId(ui::kColorAshFocusRing);
  }
}

MenuEntryView::~MenuEntryView() = default;

void MenuEntryView::ChangeHoverState(bool is_hovered) {
  if (!allow_reposition_ || is_hovered == hover_state_)
    return;

  SetBackground(views::CreateRoundedRectBackground(
      is_hovered
          ? color_utils::GetResultingPaintColor(kHoverColor, kDefaultColor)
          : kDefaultColor,
      kMenuEntryCornerRadius));
  hover_state_ = is_hovered;
}

bool MenuEntryView::OnMousePressed(const ui::MouseEvent& event) {
  if (allow_reposition_)
    OnDragStart(event);
  return views::Button::OnMousePressed(event);
}

bool MenuEntryView::OnMouseDragged(const ui::MouseEvent& event) {
  if (allow_reposition_) {
    SetCursor(ui::mojom::CursorType::kGrabbing);
    OnDragUpdate(event);
  }
  return views::Button::OnMouseDragged(event);
}

void MenuEntryView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!allow_reposition_ || !is_dragging_) {
    views::Button::OnMouseReleased(event);
    MayCancelLocatedEvent(event);
  } else {
    SetCursor(ui::mojom::CursorType::kGrab);
    OnDragEnd();
    RecordInputOverlayMenuEntryReposition(RepositionType::kMouseDragRepostion);
  }
}

void MenuEntryView::OnGestureEvent(ui::GestureEvent* event) {
  if (!allow_reposition_) {
    views::Button::OnGestureEvent(event);
    MayCancelLocatedEvent(*event);
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnDragUpdate(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      OnDragEnd();
      event->SetHandled();
      RecordInputOverlayMenuEntryReposition(
          RepositionType::kTouchscreenDragRepostion);
      break;
    default:
      views::Button::OnGestureEvent(event);
      break;
  }
}

bool MenuEntryView::OnKeyPressed(const ui::KeyEvent& event) {
  auto target_position = origin();
  if (!allow_reposition_ ||
      !UpdatePositionByArrowKey(event.key_code(), target_position)) {
    return views::ImageButton::OnKeyPressed(event);
  }
  ClampPosition(target_position, size(), parent()->size(), kParentPadding);
  SetPosition(target_position);
  return true;
}

bool MenuEntryView::OnKeyReleased(const ui::KeyEvent& event) {
  if (!allow_reposition_ || !ash::IsArrowKeyEvent(event))
    return views::ImageButton::OnKeyReleased(event);

  on_position_changed_callback_.Run(/*leave_focus=*/false,
                                    absl::make_optional(origin()));
  RecordInputOverlayMenuEntryReposition(
      RepositionType::kKeyboardArrowKeyReposition);
  return true;
}

void MenuEntryView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  start_drag_view_pos_ = origin();
}

void MenuEntryView::OnDragUpdate(const ui::LocatedEvent& event) {
  if (!is_dragging_) {
    is_dragging_ = true;
    ChangeMenuEntryOnDrag(/*is_dragging=*/true);
  }
  auto new_location = event.location();
  auto target_position = origin() + (new_location - start_drag_event_pos_);
  ClampPosition(target_position, size(), parent()->size(), kParentPadding);
  SetPosition(target_position);
}

void MenuEntryView::OnDragEnd() {
  is_dragging_ = false;
  ChangeMenuEntryOnDrag(is_dragging_);
  // When menu entry is in dragging, input events target at overlay layer. When
  // finishing drag, input events should target on the app content layer
  // underneath the overlay. So it needs to leave focus to make event target
  // leave from the overlay layer.
  on_position_changed_callback_.Run(/*leave_focus=*/true,
                                    origin() != start_drag_view_pos_
                                        ? absl::make_optional(origin())
                                        : absl::nullopt);
}

void MenuEntryView::MayCancelLocatedEvent(const ui::LocatedEvent& event) {
  if ((event.IsMouseEvent() && !HitTestPoint(event.location())) ||
      (event.IsGestureEvent() && event.type() == ui::ET_GESTURE_TAP_CANCEL)) {
    on_position_changed_callback_.Run(/*leave_focus=*/true,
                                      /*location=*/absl::nullopt);
  }
}

void MenuEntryView::ChangeMenuEntryOnDrag(bool is_dragging) {
  if (is_dragging) {
    SetBackground(views::CreateRoundedRectBackground(
        color_utils::GetResultingPaintColor(kDragColor60Blue,
                                            kDragColor100White),
        kMenuEntryCornerRadius));
    SetBorder(views::CreateRoundedRectBorder(
        kMenuEntryBorderThickness, kMenuEntryCornerRadius, kHoverColor));
  } else {
    SetBackground(views::CreateRoundedRectBackground(kDefaultColor,
                                                     kMenuEntryCornerRadius));
    SetBorder(views::CreateRoundedRectBorder(
        kMenuEntryBorderThickness, kMenuEntryCornerRadius, kBorderColor));
  }
}

void MenuEntryView::SetCursor(ui::mojom::CursorType cursor_type) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget)
    widget->SetCursor(cursor_type);
}

}  // namespace arc::input_overlay
