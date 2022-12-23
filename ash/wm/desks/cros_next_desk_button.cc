// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/cros_next_desk_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/check_op.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kIconButtonCornerRadius = 18;

constexpr int kDefaultButtonCornerRadius = 14;

constexpr int kZeroStateButtonHeight = 28;

constexpr int kZeroStateButtonWidth = 28;

constexpr int kExpandedStateButtonWidth = 44;

constexpr int kDefaultButtonHorizontalPadding = 16;

constexpr int kDefaultDeskButtonMinWidth = 56;

}  // namespace

// -----------------------------------------------------------------------------
// CrOSNextDefaultDeskButton:

CrOSNextDefaultDeskButton::CrOSNextDefaultDeskButton(DesksBarView* bar_view)
    : CrOSNextDeskButtonBase(
          DesksController::Get()->desks()[0]->name(),
          /*set_text=*/true,
          base::BindRepeating(&CrOSNextDefaultDeskButton::OnButtonPressed,
                              base::Unretained(this))),
      bar_view_(bar_view) {
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kDefaultButtonCornerRadius));
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                 DesksController::Get()->desks()[0]->name()));

  SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemOnBase));
}

gfx::Size CrOSNextDefaultDeskButton::CalculatePreferredSize() const {
  auto* root_window =
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  const int preview_width = DeskMiniView::GetPreviewWidth(
      root_window->bounds().size(), DeskPreviewView::GetHeight(root_window));
  int label_width = 0, label_height = 0;
  gfx::Canvas::SizeStringInt(DesksController::Get()->desks()[0]->name(),
                             gfx::FontList(), &label_width, &label_height, 0,
                             gfx::Canvas::NO_ELLIPSIS);

  // `preview_width` is supposed to be larger than
  // `kZeroStateDefaultDeskButtonMinWidth`, but it might be not the truth for
  // tests with extreme abnormal size of display.
  const int min_width = std::min(preview_width, kDefaultDeskButtonMinWidth);
  const int max_width = std::max(preview_width, kDefaultDeskButtonMinWidth);
  const int width = base::clamp(
      label_width + 2 * kDefaultButtonHorizontalPadding, min_width, max_width);
  return gfx::Size(width, kZeroStateButtonHeight);
}

void CrOSNextDefaultDeskButton::UpdateLabelText() {
  SetText(gfx::ElideText(
      DesksController::Get()->desks()[0]->name(), gfx::FontList(),
      bounds().width() - 2 * kDefaultButtonHorizontalPadding, gfx::ELIDE_TAIL));
}

void CrOSNextDefaultDeskButton::OnButtonPressed() {
  bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                /*expanding_bar_view=*/true);
  bar_view_->NudgeDeskName(/*desk_index=*/0);
}

BEGIN_METADATA(CrOSNextDefaultDeskButton, CrOSNextDeskButtonBase)
END_METADATA

// -----------------------------------------------------------------------------
// CrOSNextDeskIconButton:

CrOSNextDeskIconButton::CrOSNextDeskIconButton(
    DesksBarView* bar_view,
    const gfx::VectorIcon* button_icon,
    const std::u16string& text,
    ui::ColorId icon_color_id,
    ui::ColorId background_color_id,
    base::RepeatingClosure callback)
    : CrOSNextDeskButtonBase(text, /*set_text=*/false, callback),
      bar_view_(bar_view),
      state_(bar_view_->IsZeroState() ? State::kZero : State::kExpanded) {
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kIconButtonCornerRadius));
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*button_icon, icon_color_id));
  SetBackground(views::CreateThemedSolidBackground(background_color_id));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset), kIconButtonCornerRadius);
  views::FocusRing::Get(this)->SetHasFocusPredicate([&](views::View* view) {
    return IsViewHighlighted() ||
           ((bar_view_->dragged_item_over_bar() &&
             IsPointOnButton(bar_view_->last_dragged_item_screen_location())) ||
            paint_as_active_);
  });
}

CrOSNextDeskIconButton::~CrOSNextDeskIconButton() = default;

bool CrOSNextDeskIconButton::IsPointOnButton(
    const gfx::Point& screen_location) const {
  gfx::Point point_in_view = screen_location;
  ConvertPointFromScreen(this, &point_in_view);
  return HitTestPoint(point_in_view);
}

gfx::Size CrOSNextDeskIconButton::CalculatePreferredSize() const {
  if (state_ == State::kZero) {
    return gfx::Size(kZeroStateButtonWidth, kZeroStateButtonHeight);
  }

  gfx::Rect desk_preview_bounds = DeskMiniView::GetDeskPreviewBounds(
      GetWidget()->GetNativeWindow()->GetRootWindow());
  if (state_ == State::kExpanded) {
    return gfx::Size(kExpandedStateButtonWidth, desk_preview_bounds.height());
  }

  DCHECK_EQ(state_, State::kDragAndDrop);
  return gfx::Size(desk_preview_bounds.width(), desk_preview_bounds.height());
}

void CrOSNextDeskIconButton::UpdateFocusState() {
  absl::optional<ui::ColorId> new_focus_color_id;

  if (IsViewHighlighted() ||
      (bar_view_->dragged_item_over_bar() &&
       IsPointOnButton(bar_view_->last_dragged_item_screen_location()))) {
    new_focus_color_id = ui::kColorAshFocusRing;
  } else if (paint_as_active_) {
    new_focus_color_id = kColorAshCurrentDeskColor;
  } else {
    new_focus_color_id = absl::nullopt;
  }

  if (focus_color_id_ == new_focus_color_id) {
    return;
  }

  focus_color_id_ = new_focus_color_id;

  // Only repaint the focus ring if the color gets updated.
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(new_focus_color_id);
  focus_ring->SchedulePaint();
}

BEGIN_METADATA(CrOSNextDeskIconButton, CrOSNextDeskButtonBase)
END_METADATA

}  // namespace ash
