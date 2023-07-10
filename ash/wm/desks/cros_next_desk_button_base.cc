// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/cros_next_desk_button_base.h"

#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kFocusRingRadius = 16;

}  // namespace

CrOSNextDeskButtonBase::CrOSNextDeskButtonBase(
    const std::u16string& text,
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

  SetAccessibleName(text);
  SetTooltipText(text);

  // Create an empty border, otherwise in `LabelButton` a default border with
  // non-empty insets will be created.
  SetBorder(views::CreateEmptyBorder(gfx::Insets()));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset), kFocusRingRadius);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<CrOSNextDeskButtonBase>(view);
          CHECK(v);
          return v->IsViewHighlighted();
        }));
  }
}

CrOSNextDeskButtonBase::~CrOSNextDeskButtonBase() = default;

void CrOSNextDeskButtonBase::OnFocus() {
  if (bar_view_->type() == DeskBarViewBase::Type::kOverview) {
    UpdateOverviewHighlightForFocus(this);
  }
  UpdateFocusState();
  View::OnFocus();
}

void CrOSNextDeskButtonBase::OnBlur() {
  UpdateFocusState();
  View::OnBlur();
}

views::View* CrOSNextDeskButtonBase::GetView() {
  return this;
}

void CrOSNextDeskButtonBase::MaybeActivateHighlightedView() {
  pressed_callback_.Run();
}

void CrOSNextDeskButtonBase::MaybeCloseHighlightedView(bool primary_action) {}

void CrOSNextDeskButtonBase::MaybeSwapHighlightedView(bool right) {}

void CrOSNextDeskButtonBase::OnViewHighlighted() {
  UpdateFocusState();
  bar_view_->ScrollToShowViewIfNecessary(this);
}

void CrOSNextDeskButtonBase::OnViewUnhighlighted() {
  UpdateFocusState();
}

void CrOSNextDeskButtonBase::UpdateFocusState() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(CrOSNextDeskButtonBase, views::LabelButton)
END_METADATA

}  // namespace ash
