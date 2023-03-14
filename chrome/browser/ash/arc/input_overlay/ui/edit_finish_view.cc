// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_finish_view.h"

#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace arc::input_overlay {

namespace {
// About the whole view.
constexpr int kViewHeight = 144;
constexpr int kSideMargin = 24;
constexpr int kViewMargin = 8;
constexpr int kViewCornerRadius = 16;
constexpr int kViewBackgroundColor = SkColorSetA(SK_ColorBLACK, 0xCC /*80%*/);
constexpr int kParentPadding = 16;
// Space between children.
constexpr int kSpaceRow = 4;
// Alpha view features.
constexpr int kSpaceRowAlpha = 8;
constexpr int kViewHeightAlpha = 184;

// About button.
constexpr int kButtonHeight = 40;
constexpr int kButtonSideInset = 24;
constexpr int kButtonCornerRadius = 12;
constexpr SkColor kButtonTextColor = gfx::kGoogleGrey200;
constexpr SkColor kSaveButtonBackgroundColor = gfx::kGoogleBlue300;
constexpr SkColor kSaveButtonTextColor = gfx::kGoogleGrey900;
constexpr char kFontStyle[] = "Google Sans";
constexpr int kFontSize = 13;
// Alpha button features.
constexpr int kButtonCornerRadiusAlpha = 16;
constexpr int kFontSizeAlpha = 16;
constexpr int kButtonSideInsetAlpha = 20;
constexpr int kButtonHeightAlpha = 56;
// This color is same as the background color of input_mapping_view in kEdit
// mode and is used for buttons to decide what ink drop color should be. If the
// dark background color is set, then it will show the light ink drop color.
// Since we only need the dark mode for the kEdit mode, so dark background is
// passed for setting up the ink drop.
constexpr SkColor kEditBackgroundColor = SkColorSetA(SK_ColorBLACK, 0x99);
constexpr SkColor kInkDropBaseColor = SK_ColorWHITE;
constexpr float kInkDropOpacity = 0.08f;

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -6;
// Thickness of focus ring.
constexpr float kHaloThickness = 4;

std::unique_ptr<views::InkDrop> CreateInkDrop(views::Button* view) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(view), /*highlight_on_hover=*/true,
      /*highlight_on_focus=*/true);
}

std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
    const views::Button* view) {
  return std::make_unique<views::FloodFillInkDropRipple>(
      const_cast<views::InkDropHost*>(views::InkDrop::Get(view)), view->size(),
      gfx::Insets(),
      views::InkDrop::Get(view)->GetInkDropCenterBasedOnLastEvent(),
      kInkDropBaseColor, kInkDropOpacity);
}

std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
    views::Button* view) {
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(view->size()), kInkDropBaseColor);
  highlight->set_visible_opacity(kInkDropOpacity);
  return highlight;
}

}  // namespace

class EditFinishView::ChildButton : public views::LabelButton {
 public:
  using OnMousePressedCallback =
      base::RepeatingCallback<bool(const ui::MouseEvent& event)>;
  using OnMouseDraggedCallback =
      base::RepeatingCallback<bool(const ui::MouseEvent& event)>;
  using OnMouseReleasedCallback =
      base::RepeatingCallback<void(const ui::MouseEvent& event)>;

  ChildButton(PressedCallback callback,
              int text_source_id,
              SkColor background_color,
              SkColor text_color,
              OnMousePressedCallback on_mouse_pressed_callback,
              OnMouseDraggedCallback on_mouse_dragged_callback,
              OnMouseReleasedCallback on_mouse_released_callback)
      : LabelButton(callback, l10n_util::GetStringUTF16(text_source_id)),
        on_mouse_pressed_callback_(on_mouse_pressed_callback),
        on_mouse_dragged_callback_(on_mouse_dragged_callback),
        on_mouse_released_callback_(on_mouse_released_callback) {
    label()->SetFontList(
        gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                      AllowReposition() ? kFontSize : kFontSizeAlpha,
                      gfx::Font::Weight::MEDIUM));
    SetEnabledTextColors(text_color);
    SetAccessibleName(l10n_util::GetStringUTF16(text_source_id));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
        0, AllowReposition() ? kButtonSideInset : kButtonSideInsetAlpha)));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetMinSize(gfx::Size(
        /*width=*/0, AllowReposition() ? kButtonHeight : kButtonHeightAlpha));
    SetBackground(views::CreateRoundedRectBackground(
        background_color,
        AllowReposition() ? kButtonCornerRadius : kButtonCornerRadiusAlpha));

    // Set states.
    views::InstallRoundRectHighlightPathGenerator(
        this, gfx::Insets(),
        AllowReposition() ? kButtonCornerRadius : kButtonCornerRadiusAlpha);
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHaloInset(kHaloInset);
    focus_ring->SetHaloThickness(kHaloThickness);
    focus_ring->SetColorId(ui::kColorAshInputOverlayFocusRing);
    if (AllowReposition()) {
      SetUpInkDropForButton();
    } else {
      ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                            /*highlight_on_hover=*/true,
                                            /*highlight_on_focus=*/true,
                                            kEditBackgroundColor);
    }
  }
  ~ChildButton() override = default;

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (AllowReposition()) {
      on_mouse_pressed_callback_.Run(event);
    }
    return LabelButton::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!AllowReposition()) {
      return LabelButton::OnMouseDragged(event);
    }

    is_dragging_ = true;
    on_mouse_dragged_callback_.Run(event);
    views::InkDrop::Get(this)->GetInkDrop()->SetHovered(false);
    views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN,
                                              /*event=*/nullptr);
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!AllowReposition() || !is_dragging_) {
      LabelButton::OnMouseReleased(event);
      return;
    }
    // Don't trigger button pressed event when it is dragged.
    is_dragging_ = false;
    on_mouse_released_callback_.Run(event);
  }

 private:
  // Set inkdrop without theme.
  void SetUpInkDropForButton() {
    auto* ink_drop = views::InkDrop::Get(this);
    ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    ink_drop->SetCreateInkDropCallback(
        base::BindRepeating(&CreateInkDrop, this));
    ink_drop->SetCreateRippleCallback(
        base::BindRepeating(&CreateInkDropRipple, this));
    ink_drop->SetCreateHighlightCallback(
        base::BindRepeating(&CreateInkDropHighlight, this));
  }

  bool is_dragging_ = false;

  // Callbacks for dragging.
  OnMousePressedCallback on_mouse_pressed_callback_;
  OnMouseDraggedCallback on_mouse_dragged_callback_;
  OnMouseReleasedCallback on_mouse_released_callback_;
};

// static
EditFinishView* EditFinishView::BuildView(
    DisplayOverlayController* display_overlay_controller,
    views::View* parent) {
  auto* menu_view_ptr = parent->AddChildView(
      std::make_unique<EditFinishView>(display_overlay_controller));
  menu_view_ptr->Init(parent->size());
  return menu_view_ptr;
}

EditFinishView::EditFinishView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {}

EditFinishView::~EditFinishView() {}

void EditFinishView::Init(const gfx::Size& parent_size) {
  DCHECK(display_overlay_controller_);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      AllowReposition() ? gfx::Insets().TLBR(kViewMargin, kViewMargin,
                                             kViewMargin, kViewMargin)
                        : gfx::Insets(),
      AllowReposition() ? kSpaceRow : kSpaceRowAlpha));
  SetBackground(AllowReposition()
                    ? views::CreateRoundedRectBackground(kViewBackgroundColor,
                                                         kViewCornerRadius)
                    : views::CreateSolidBackground(SK_ColorTRANSPARENT));

  if (AllowReposition()) {
    SetFocusRing();
  }

  auto on_mouse_pressed_callback = base::BindRepeating(
      &EditFinishView::OnMousePressed, base::Unretained(this));
  auto on_mouse_dragged_callback = base::BindRepeating(
      &EditFinishView::OnMouseDragged, base::Unretained(this));
  auto on_mouse_released_callback = base::BindRepeating(
      &EditFinishView::OnMouseReleased, base::Unretained(this));

  reset_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnResetButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_RESET, SK_ColorTRANSPARENT, kButtonTextColor,
      on_mouse_pressed_callback, on_mouse_dragged_callback,
      on_mouse_released_callback));

  save_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnSaveButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_SAVE, kSaveButtonBackgroundColor,
      kSaveButtonTextColor, on_mouse_pressed_callback,
      on_mouse_dragged_callback, on_mouse_released_callback));

  cancel_button_ = AddChildView(std::make_unique<ChildButton>(
      base::BindRepeating(&EditFinishView::OnCancelButtonPressed,
                          base::Unretained(this)),
      IDS_INPUT_OVERLAY_EDIT_MODE_CANCEL, SK_ColorTRANSPARENT, kButtonTextColor,
      on_mouse_pressed_callback, on_mouse_dragged_callback,
      on_mouse_released_callback));

  const int width = CalculateWidth();
  const int height = AllowReposition() ? kViewHeight : kViewHeightAlpha;
  SetSize(
      gfx::Size(AllowReposition() ? width + 2 * kViewMargin : width, height));
  SetPosition(gfx::Point(std::max(0, parent_size.width() - width - kSideMargin),
                         std::max(0, parent_size.height() / 3 - height / 2)));
}

void EditFinishView::SetFocusRing() {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_LAYOUT_ACCTIONS_MENU));
  GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MENU_FOCUS));
  views::FocusRing::Install(this);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
  focus_ring->SetColorId(ui::kColorAshInputOverlayFocusRing);
}

int EditFinishView::CalculateWidth() {
  int width = std::max(reset_button_->GetPreferredSize().width(),
                       save_button_->GetPreferredSize().width());
  width = std::max(width, cancel_button_->GetPreferredSize().width());
  return width;
}

bool EditFinishView::OnMousePressed(const ui::MouseEvent& event) {
  if (AllowReposition()) {
    OnDragStart(event);
  }
  return true;
}

bool EditFinishView::OnMouseDragged(const ui::MouseEvent& event) {
  if (AllowReposition()) {
    SetCursor(ui::mojom::CursorType::kGrabbing);
    OnDragUpdate(event);
  }
  return true;
}

void EditFinishView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!AllowReposition() || !is_dragging_) {
    views::View::OnMouseReleased(event);
    return;
  }
  SetCursor(ui::mojom::CursorType::kGrab);
  OnDragEnd();
  RecordInputOverlayButtonGroupReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kMouseDragRepostion,
      display_overlay_controller_->GetWindowStateType());
}

ui::Cursor EditFinishView::GetCursor(const ui::MouseEvent& event) {
  if (AllowReposition()) {
    return ui::mojom::CursorType::kHand;
  }
  return views::View::GetCursor(event);
}

void EditFinishView::OnGestureEvent(ui::GestureEvent* event) {
  if (!AllowReposition()) {
    views::View::OnGestureEvent(event);
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
      RecordInputOverlayButtonGroupReposition(
          display_overlay_controller_->GetPackageName(),
          RepositionType::kTouchscreenDragRepostion,
          display_overlay_controller_->GetWindowStateType());
      break;
    default:
      views::View::OnGestureEvent(event);
      break;
  }
}

bool EditFinishView::OnKeyPressed(const ui::KeyEvent& event) {
  auto target_position = origin();
  if (!AllowReposition() ||
      !UpdatePositionByArrowKey(event.key_code(), target_position) ||
      !HasFocus()) {
    return views::View::OnKeyPressed(event);
  }

  ClampPosition(target_position, size(), parent()->size(), kParentPadding);
  SetPosition(target_position);
  return true;
}

bool EditFinishView::OnKeyReleased(const ui::KeyEvent& event) {
  if (!AllowReposition() || !ash::IsArrowKeyEvent(event) || !HasFocus()) {
    return views::View::OnKeyReleased(event);
  }

  RecordInputOverlayButtonGroupReposition(
      display_overlay_controller_->GetPackageName(),
      RepositionType::kKeyboardArrowKeyReposition,
      display_overlay_controller_->GetWindowStateType());
  return true;
}

void EditFinishView::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  start_drag_view_pos_ = origin();
}

void EditFinishView::OnDragUpdate(const ui::LocatedEvent& event) {
  is_dragging_ = true;

  auto new_location = event.location();
  auto target_position = origin() + (new_location - start_drag_event_pos_);
  ClampPosition(target_position, size(), parent()->size(), kParentPadding);
  SetPosition(target_position);
}

void EditFinishView::OnDragEnd() {
  is_dragging_ = false;
}

void EditFinishView::SetCursor(ui::mojom::CursorType cursor_type) {
  auto* widget = GetWidget();
  // widget is null for test.
  if (widget) {
    widget->SetCursor(cursor_type);
  }
}

void EditFinishView::OnResetButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }
  display_overlay_controller_->OnCustomizeRestore();
  if (reset_button_->HasFocus() || !parent()) {
    return;
  }
  ResetFocusTo(parent());
}

void EditFinishView::OnSaveButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }
  display_overlay_controller_->OnCustomizeSave();
}

void EditFinishView::OnCancelButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }
  display_overlay_controller_->OnCustomizeCancel();
}

}  // namespace arc::input_overlay
