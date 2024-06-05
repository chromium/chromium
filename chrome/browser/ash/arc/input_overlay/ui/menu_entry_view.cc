// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_util.h"
#include "ash/style/style_util.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr int kMenuEntrySideMargin = 24;
// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -5;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;

}  // namespace

// static
MenuEntryView* MenuEntryView::Show(
    PressedCallback pressed_callback,
    OnPositionChangedCallback on_position_changed_callback,
    DisplayOverlayController* display_overlay_controller) {
  auto* menu_entry =
      display_overlay_controller->GetOverlayWidgetContentsView()->AddChildView(
          std::make_unique<MenuEntryView>(
              std::move(pressed_callback),
              std::move(on_position_changed_callback),
              display_overlay_controller));
  menu_entry->Init();
  return menu_entry;
}

MenuEntryView::MenuEntryView(
    PressedCallback pressed_callback,
    OnPositionChangedCallback on_position_changed_callback,
    DisplayOverlayController* display_overlay_controller)
    : views::ImageButton(std::move(pressed_callback)),
      on_position_changed_callback_(on_position_changed_callback),
      display_overlay_controller_(display_overlay_controller) {}

MenuEntryView::~MenuEntryView() = default;

void MenuEntryView::ChangeHoverState(bool is_hovered) {
  if (is_hovered == hover_state_) {
    return;
  }

  SetBackground(views::CreateRoundedRectBackground(
      is_hovered
          ? color_utils::GetResultingPaintColor(kHoverColor, kDefaultColor)
          : kDefaultColor,
      kMenuEntryCornerRadius));
  hover_state_ = is_hovered;
}

void MenuEntryView::OnFirstDraggingCallback() {
  ChangeMenuEntryOnDrag(/*is_dragging=*/true);
}

void MenuEntryView::OnMouseDragEndCallback() {
  ChangeMenuEntryOnDrag(/*is_dragging=*/false);
  // When menu entry is in dragging, input events target at overlay layer. When
  // finishing drag, input events should target on the app content layer
  // underneath the overlay. So it needs to leave focus to make event target
  // leave from the overlay layer.
  on_position_changed_callback_.Run(/*leave_focus=*/true,
                                    std::make_optional(origin()));
  RecordInputOverlayMenuEntryReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kMouseDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void MenuEntryView::OnGestureDragEndCallback() {
  ChangeMenuEntryOnDrag(/*is_dragging=*/false);
  on_position_changed_callback_.Run(/*leave_focus=*/true,
                                    std::make_optional(origin()));
  RecordInputOverlayMenuEntryReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kTouchscreenDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

void MenuEntryView::OnKeyReleasedCallback() {
  on_position_changed_callback_.Run(/*leave_focus=*/false,
                                    std::make_optional(origin()));
  RecordInputOverlayMenuEntryReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kKeyboardArrowKeyReposition,
      display_overlay_controller_->GetWindowStateType());
}

void MenuEntryView::AddedToWidget() {
  SetRepositionController();
}

bool MenuEntryView::OnMousePressed(const ui::MouseEvent& event) {
  reposition_controller_->OnMousePressed(event);
  return views::ImageButton::OnMousePressed(event);
}

bool MenuEntryView::OnMouseDragged(const ui::MouseEvent& event) {
  SetCursor(ui::mojom::CursorType::kGrabbing);
  reposition_controller_->OnMouseDragged(event);
  return true;
}

void MenuEntryView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!reposition_controller_->OnMouseReleased(event)) {
    views::ImageButton::OnMouseReleased(event);
  } else {
    SetCursor(ui::mojom::CursorType::kGrab);
  }
}

void MenuEntryView::Init() {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA));
  SetBackground(views::CreateRoundedRectBackground(kDefaultColor,
                                                   kMenuEntryCornerRadius));
  SetBorder(views::CreateRoundedRectBorder(
      kMenuEntryBorderThickness, kMenuEntryCornerRadius, kBorderColor));

  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(chromeos::kGameDashboardGamepadIcon,
                                     SK_ColorBLACK, kMenuEntryIconSize));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  // Set up focus ring for `menu_entry_`.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kMenuEntryCornerRadius);
  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/true);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
  focus_ring->SetColorId(ui::kColorAshInputOverlayFocusRing);

  const auto position = CalculatePosition();
  SetBounds(position.x(), position.y(), kMenuEntrySize, kMenuEntrySize);
}

gfx::Point MenuEntryView::CalculatePosition() const {
  const auto* touch_injector = display_overlay_controller_->touch_injector();
  if (auto normalized_location = touch_injector->menu_entry_location()) {
    const auto content_bounds = touch_injector->content_bounds_f();
    return gfx::Point(static_cast<int>(std::round(normalized_location->x() *
                                                  content_bounds.width())),
                      static_cast<int>(std::round(normalized_location->y() *
                                                  content_bounds.height())));
  } else {
    const auto* parent_view =
        display_overlay_controller_->GetOverlayWidgetContentsView();
    if (!parent_view || parent_view->bounds().IsEmpty()) {
      return gfx::Point();
    }

    return gfx::Point(
        std::max(0,
                 parent_view->width() - kMenuEntrySize - kMenuEntrySideMargin),
        std::max(0, parent_view->height() / 2 - kMenuEntrySize / 2));
  }
}

void MenuEntryView::OnGestureEvent(ui::GestureEvent* event) {
  if (!reposition_controller_->OnGestureEvent(event)) {
    return views::ImageButton::OnGestureEvent(event);
  }
}

bool MenuEntryView::OnKeyPressed(const ui::KeyEvent& event) {
  return reposition_controller_->OnKeyPressed(event)
             ? true
             : views::ImageButton::OnKeyPressed(event);
}

bool MenuEntryView::OnKeyReleased(const ui::KeyEvent& event) {
  return reposition_controller_->OnKeyReleased(event)
             ? true
             : views::ImageButton::OnKeyReleased(event);
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
  // widget is null for test.
  if (auto* widget = GetWidget()) {
    widget->SetCursor(cursor_type);
  }
}

void MenuEntryView::SetRepositionController() {
  if (reposition_controller_) {
    return;
  }
  reposition_controller_ =
      std::make_unique<RepositionController>(this, kParentPadding);
  reposition_controller_->set_first_dragging_callback(base::BindRepeating(
      &MenuEntryView::OnFirstDraggingCallback, base::Unretained(this)));
  reposition_controller_->set_mouse_drag_end_callback(base::BindRepeating(
      &MenuEntryView::OnMouseDragEndCallback, base::Unretained(this)));
  reposition_controller_->set_gesture_drag_end_callback(base::BindRepeating(
      &MenuEntryView::OnGestureDragEndCallback, base::Unretained(this)));
  reposition_controller_->set_key_released_callback(base::BindRepeating(
      &MenuEntryView::OnKeyReleasedCallback, base::Unretained(this)));
}

BEGIN_METADATA(MenuEntryView)
END_METADATA

}  // namespace arc::input_overlay
