// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button_base.h"

#include "ash/constants/ash_features.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/wm_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kFocusRingRadius = 16;

}  // namespace

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               bool set_text,
                               DeskBarViewBase* bar_view,
                               base::RepeatingClosure pressed_callback)
    : LabelButton(pressed_callback),
      bar_view_(bar_view),
      pressed_callback_(pressed_callback) {
  DCHECK(!text.empty());
  if (set_text) {
    SetText(text);
  }

  // Call `SetPaintToLayer` explicitly here since we need to do the layer
  // animations on `this`.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  GetViewAccessibility().SetName(text);
  SetTooltipText(text);

  // Create an empty border, otherwise in `LabelButton` a default border with
  // non-empty insets will be created.
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kWindowMiniViewFocusRingHaloInset), kFocusRingRadius);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview &&
      !features::IsOverviewNewFocusEnabled()) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<DeskButtonBase>(view);
          CHECK(v);
          return v->is_focused();
        }));
  }
}

DeskButtonBase::~DeskButtonBase() = default;

void DeskButtonBase::OnFocus() {
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    MoveFocusToView(this);
  }
  UpdateFocusState();
  View::OnFocus();
}

void DeskButtonBase::OnBlur() {
  UpdateFocusState();
  View::OnBlur();
}

views::View* DeskButtonBase::GetView() {
  return this;
}

void DeskButtonBase::MaybeActivateFocusedView() {
  pressed_callback_.Run();
}

void DeskButtonBase::MaybeCloseFocusedView(bool primary_action) {}

void DeskButtonBase::MaybeSwapFocusedView(bool right) {}

void DeskButtonBase::OnFocusableViewFocused() {
  UpdateFocusState();
  bar_view_->ScrollToShowViewIfNecessary(this);
}

void DeskButtonBase::OnFocusableViewBlurred() {
  UpdateFocusState();
}

void DeskButtonBase::UpdateFocusState() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(DeskButtonBase)
END_METADATA

}  // namespace ash
