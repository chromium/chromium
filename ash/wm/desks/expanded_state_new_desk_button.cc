// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/expanded_state_new_desk_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kNewDeskButtonAndNameSpacing = 8;

constexpr int kBorderCornerRadius = 6;

constexpr int kCornerRadius = 4;

// The new desk button in expand desks bar in Bento has the same size as the
// desk preview, which is proportional to the size of the display on which it
// resides.
gfx::Rect GetExpandedStateNewDeskButtonBounds(aura::Window* root_window) {
  const int preview_height =
      DeskPreviewView::GetHeight(root_window, /*compact=*/false);
  const auto root_size = root_window->bounds().size();
  return gfx::Rect(preview_height * root_size.width() / root_size.height(),
                   preview_height);
}

// The button belongs to ExpandedStateNewDeskButton.
class ASH_EXPORT InnerNewDeskButton : public DeskButtonBase {
 public:
  InnerNewDeskButton()
      : DeskButtonBase(base::string16(), kBorderCornerRadius, kCornerRadius) {
    paint_contents_only_ = true;
  }
  InnerNewDeskButton(const InnerNewDeskButton&) = delete;
  InnerNewDeskButton operator=(const InnerNewDeskButton&) = delete;
  ~InnerNewDeskButton() override = default;

  // DeskButtonBase:
  const char* GetClassName() const override { return "InnerNewDeskButton"; }

  void OnThemeChanged() override {
    DeskButtonBase::OnThemeChanged();
    AshColorProvider::Get()->DecoratePillButton(this, &kDesksNewDeskButtonIcon);
    UpdateButtonState();
  }

  void OnButtonPressed() override {
    auto* controller = DesksController::Get();
    if (controller->CanCreateDesks()) {
      controller->NewDesk(DesksCreationRemovalSource::kButton);
      UpdateButtonState();
    }
  }

  // Update the button's enable/disable state based on current desks state.
  void UpdateButtonState() override {
    const bool enabled = DesksController::Get()->CanCreateDesks();

    // Notify the overview highlight if we are about to be disabled.
    if (!enabled) {
      OverviewSession* overview_session =
          Shell::Get()->overview_controller()->overview_session();
      DCHECK(overview_session);
      overview_session->highlight_controller()->OnViewDestroyingOrDisabling(
          this);
    }
    SetEnabled(enabled);

    const auto* color_provider = AshColorProvider::Get();
    background_color_ = color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
    if (!enabled)
      background_color_ = AshColorProvider::GetDisabledColor(background_color_);

    SetInkDropVisibleOpacity(
        color_provider->GetRippleAttributes(background_color_).inkdrop_opacity);
    SchedulePaint();
  }
};

}  // namespace

ExpandedStateNewDeskButton::ExpandedStateNewDeskButton(DesksBarView* bar_view)
    : bar_view_(bar_view),
      new_desk_button_(AddChildView(std::make_unique<InnerNewDeskButton>())),
      label_(AddChildView(std::make_unique<views::Label>())) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  label_->SetText(l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON));
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

void ExpandedStateNewDeskButton::Layout() {
  const gfx::Rect new_desk_button_bounds = GetExpandedStateNewDeskButtonBounds(
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow());
  new_desk_button_->SetBoundsRect(new_desk_button_bounds);

  const gfx::Size label_size = label_->GetPreferredSize();
  label_->SetBoundsRect(gfx::Rect(
      gfx::Point(
          (new_desk_button_bounds.width() - label_size.width()) / 2,
          new_desk_button_bounds.bottom() + kNewDeskButtonAndNameSpacing),
      label_size));
}

void ExpandedStateNewDeskButton::UpdateButtonState() {
  new_desk_button_->UpdateButtonState();
}

}  // namespace ash
