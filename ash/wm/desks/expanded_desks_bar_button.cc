// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/expanded_desks_bar_button.h"

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

// The button belongs to ExpandedDesksBarButton.
class ASH_EXPORT InnerExpandedDesksBarButton : public DeskButtonBase {
 public:
  METADATA_HEADER(InnerExpandedDesksBarButton);

  // TODO(sophiewen): Move callback to DeskButtonBase constructor parameter. We
  // also want to eventually use views::Button::Callback.
  InnerExpandedDesksBarButton(ExpandedDesksBarButton* outer_button,
                              base::RepeatingClosure callback)
      : DeskButtonBase(std::u16string(), kBorderCornerRadius, kCornerRadius),
        outer_button_(outer_button),
        button_callback_(callback) {
    paint_contents_only_ = true;
  }
  InnerExpandedDesksBarButton(const InnerExpandedDesksBarButton&) = delete;
  InnerExpandedDesksBarButton operator=(const InnerExpandedDesksBarButton&) =
      delete;
  ~InnerExpandedDesksBarButton() override = default;

  void OnThemeChanged() override {
    DeskButtonBase::OnThemeChanged();
    const SkColor enabled_icon_color =
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kButtonIconColor);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*outer_button_->button_icon(),
                                   enabled_icon_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *outer_button_->button_icon(),
                 AshColorProvider::GetDisabledColor(enabled_icon_color)));
    UpdateButtonState();
  }

  void OnButtonPressed() override { button_callback_.Run(); }

  // Update the button's enable/disable state based on current desks state.
  // TODO(sophiewen): This disables all expanded button types when the max # of
  // desks is created, but this logic should be separated for New Desk creation
  // and Desks Templates.
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
  ExpandedDesksBarButton* outer_button_;
  // Defines the button behavior and is called in OnButtonPressed.
  base::RepeatingClosure button_callback_;
};

BEGIN_METADATA(InnerExpandedDesksBarButton, views::LabelButton)
END_METADATA

}  // namespace

ExpandedDesksBarButton::ExpandedDesksBarButton(
    DesksBarView* bar_view,
    const gfx::VectorIcon* button_icon,
    const std::u16string& button_label,
    base::RepeatingClosure callback)
    : bar_view_(bar_view),
      button_icon_(button_icon),
      button_label_(button_label),
      inner_button_(AddChildView(
          std::make_unique<InnerExpandedDesksBarButton>(this, callback))),
      label_(AddChildView(std::make_unique<views::Label>())) {
  DCHECK(button_icon_);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetBackgroundColor(AshColorProvider::Get()->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield80));
  UpdateLabelColor();
}

void ExpandedDesksBarButton::UpdateButtonState() {
  inner_button_->UpdateButtonState();
}

void ExpandedDesksBarButton::UpdateLabelColor() {
  const SkColor label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label_->SetEnabledColor(
      DesksController::Get()->CanCreateDesks()
          ? label_color
          : AshColorProvider::Get()->GetDisabledColor(label_color));
}

bool ExpandedDesksBarButton::IsPointOnButton(
    const gfx::Point& screen_location) const {
  gfx::Point point_in_view = screen_location;
  ConvertPointFromScreen(this, &point_in_view);
  return HitTestPoint(point_in_view);
}

void ExpandedDesksBarButton::UpdateBorderColor() const {
  DCHECK(inner_button_);
  const bool focused =
      bar_view_->dragged_item_over_bar() &&
      IsPointOnButton(bar_view_->last_dragged_item_screen_location());
  if (inner_button_->border_ptr()->SetFocused(focused))
    inner_button_->SchedulePaint();
}

void ExpandedDesksBarButton::Layout() {
  // Layout the button until |mini_views_| have been created. This button only
  // needs to be laid out in the expanded desks bar where the |mini_views_| is
  // always not empty.
  if (bar_view_->mini_views().empty())
    return;
  const gfx::Rect inner_button_bounds = DeskMiniView::GetDeskPreviewBounds(
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow());
  inner_button_->SetBoundsRect(inner_button_bounds);
  auto* desk_mini_view = bar_view_->mini_views()[0];
  auto* desk_name_view = desk_mini_view->desk_name_view();
  // `button_label_` string might exceed the maximum width in different
  // languages. Elide the string `button_label_` if it exceeds the width limit
  // after been translated into a different language.
  label_->SetText(gfx::ElideText(
      button_label_, gfx::FontList(),
      inner_button_bounds.width() - desk_name_view->GetInsets().width(),
      gfx::ELIDE_TAIL));
  const gfx::Size label_size = label_->GetPreferredSize();
  // Set the label to have the same height as the DeskNameView to keep them at
  // the same horizotal level. Note, don't get the label's width from
  // DeskNameView since desk's name is changeable, but this label here is not.
  const int label_height = desk_name_view->GetPreferredSize().height();
  label_->SetBoundsRect(gfx::Rect(
      gfx::Point((inner_button_bounds.width() - label_size.width()) / 2,
                 inner_button_bounds.bottom() -
                     desk_mini_view->GetPreviewBorderInsets().bottom() +
                     kNewDeskButtonAndNameSpacing),
      gfx::Size(label_size.width(), label_height)));
}

BEGIN_METADATA(ExpandedDesksBarButton, views::View)
END_METADATA

}  // namespace ash
