// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/expanded_state_new_desk_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kNewDeskButtonAndNameSpacing = 8;

constexpr int kBorderCornerRadius = 6;

constexpr int kCornerRadius = 4;

// The button belongs to ExpandedStateNewDeskButton.
class ASH_EXPORT InnerNewDeskButton : public DeskButtonBase {
 public:
  InnerNewDeskButton(ExpandedStateNewDeskButton* outer_button,
                     DesksBarView* bar_view)
      : DeskButtonBase(std::u16string(), kBorderCornerRadius, kCornerRadius),
        outer_button_(outer_button),
        bar_view_(bar_view) {
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
      bar_view_->set_should_name_nudge(true);
      controller->NewDesk(DesksCreationRemovalSource::kButton);
      UpdateButtonState();
    }
  }

  // Update the button's enable/disable state based on current desks state.
  void UpdateButtonState() override {
    outer_button_->UpdateLabelColor();
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

    views::InkDrop::Get(this)->SetVisibleOpacity(
        color_provider->GetRippleAttributes(background_color_).inkdrop_opacity);
    SchedulePaint();
  }

 private:
  ExpandedStateNewDeskButton* outer_button_;
  DesksBarView* bar_view_;
};

}  // namespace

ExpandedStateNewDeskButton::ExpandedStateNewDeskButton(DesksBarView* bar_view)
    : bar_view_(bar_view),
      new_desk_button_(
          AddChildView(std::make_unique<InnerNewDeskButton>(this, bar_view))),
      label_(AddChildView(std::make_unique<views::Label>())) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetBackgroundColor(AshColorProvider::Get()->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield80));
  UpdateLabelColor();
}

void ExpandedStateNewDeskButton::Layout() {
  // Layout the button until |mini_views_| have been created. This button only
  // needs to be laid out in the expanded desks bar where the |mini_views_| is
  // always not empty.
  if (bar_view_->mini_views().empty())
    return;

  const gfx::Rect new_desk_button_bounds = DeskMiniView::GetDeskPreviewBounds(
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow());
  new_desk_button_->SetBoundsRect(new_desk_button_bounds);
  auto* desk_mini_view = bar_view_->mini_views()[0];
  auto* desk_name_view = desk_mini_view->desk_name_view();
  // 'New desk' string might exceed the maximum width in different languages.
  // Elide the string 'New desk' if it exceeds the width limit after been
  // translated into a different language.
  label_->SetText(gfx::ElideText(
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON), gfx::FontList(),
      new_desk_button_bounds.width() - desk_name_view->GetInsets().width(),
      gfx::ELIDE_TAIL));
  const gfx::Size label_size = label_->GetPreferredSize();
  // Set the label to have the same height as the DeskNameView to keep them at
  // the same horizotal level. Note, don't get the label's width from
  // DeskNameView since desk's name is changeable, but this label here is not.
  const int label_height = desk_name_view->GetPreferredSize().height();
  label_->SetBoundsRect(gfx::Rect(
      gfx::Point((new_desk_button_bounds.width() - label_size.width()) / 2,
                 new_desk_button_bounds.bottom() -
                     desk_mini_view->GetPreviewBorderInsets().bottom() +
                     kNewDeskButtonAndNameSpacing),
      gfx::Size(label_size.width(), label_height)));
}

void ExpandedStateNewDeskButton::UpdateButtonState() {
  new_desk_button_->UpdateButtonState();
}

void ExpandedStateNewDeskButton::UpdateLabelColor() {
  const SkColor label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label_->SetEnabledColor(
      DesksController::Get()->CanCreateDesks()
          ? label_color
          : AshColorProvider::Get()->GetDisabledColor(label_color));
}

}  // namespace ash
