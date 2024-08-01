// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button_base.h"

#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/wm_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

constexpr int kFocusRingRadius = 16;

}  // namespace

DeskButtonBase::DeskButtonBase(const std::u16string& text,
                               bool set_text,
                               DeskBarViewBase* bar_view,
                               base::RepeatingClosure pressed_callback)
    : LabelButton(pressed_callback), bar_view_(bar_view) {
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
}

DeskButtonBase::~DeskButtonBase() = default;

void DeskButtonBase::OnFocus() {
  bar_view_->ScrollToShowViewIfNecessary(this);
  View::OnFocus();
}

BEGIN_METADATA(DeskButtonBase)
END_METADATA

}  // namespace ash
