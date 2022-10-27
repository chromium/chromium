// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/expanded_desks_bar_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk_button_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kDeskBarButtonAndNameSpacing = 8;

constexpr int kBorderCornerRadius = 6;

constexpr int kCornerRadius = 4;

}  // namespace

// -----------------------------------------------------------------------------
// InnerExpandedDesksBarButton:

class ASH_EXPORT InnerExpandedDesksBarButton : public DeskButtonBase {
 public:
  METADATA_HEADER(InnerExpandedDesksBarButton);

  InnerExpandedDesksBarButton(ExpandedDesksBarButton* outer_button,
                              base::RepeatingClosure callback,
                              const std::u16string& text)
      : DeskButtonBase(text,
                       /*set_text=*/false,
                       std::move(callback),
                       kBorderCornerRadius,
                       kCornerRadius),
        outer_button_(outer_button) {
    set_paint_contents_only(true);
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
    SetImage(
        views::Button::STATE_DISABLED,
        gfx::CreateVectorIcon(*outer_button_->button_icon(),
                              ColorUtil::GetDisabledColor(enabled_icon_color)));
    SetButtonState(GetEnabled());
  }

  void SetButtonState(bool enabled) {
    outer_button_->UpdateLabelColor(enabled);
    // Notify the overview highlight if we are about to be disabled.
    if (!enabled) {
      OverviewSession* overview_session =
          Shell::Get()->overview_controller()->overview_session();
      DCHECK(overview_session);
      overview_session->highlight_controller()->OnViewDestroyingOrDisabling(
          this);
    }
    SetEnabled(enabled);
    UpdateBackgroundColor();
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
    SchedulePaint();
  }

  void UpdateBorderState() override { outer_button_->UpdateBorderColor(); }

 private:
  ExpandedDesksBarButton* outer_button_;
};

BEGIN_METADATA(InnerExpandedDesksBarButton, views::LabelButton)
END_METADATA

// -----------------------------------------------------------------------------
// ExpandedDesksBarButton:

ExpandedDesksBarButton::ExpandedDesksBarButton(
    DesksBarView* bar_view,
    const gfx::VectorIcon* button_icon,
    const std::u16string& button_label,
    bool initially_enabled,
    base::RepeatingClosure callback)
    : bar_view_(bar_view),
      button_icon_(button_icon),
      button_label_(button_label),
      inner_button_(AddChildView(
          std::make_unique<InnerExpandedDesksBarButton>(this,
                                                        callback,
                                                        button_label))),
      label_(AddChildView(std::make_unique<views::Label>())) {
  DCHECK(button_icon_);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetButtonState(initially_enabled);
}

DeskButtonBase* ExpandedDesksBarButton::GetInnerButton() {
  return static_cast<DeskButtonBase*>(inner_button_);
}

void ExpandedDesksBarButton::SetButtonState(bool enabled) {
  inner_button_->SetButtonState(enabled);
}

void ExpandedDesksBarButton::UpdateLabelColor(bool enabled) {
  const SkColor label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label_->SetEnabledColor(enabled ? label_color
                                  : ColorUtil::GetDisabledColor(label_color));
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
      inner_button_->IsViewHighlighted() ||
      (bar_view_->dragged_item_over_bar() &&
       IsPointOnButton(bar_view_->last_dragged_item_screen_location()));
  bool should_paint = inner_button_->GetBorderPtr()->SetFocused(focused);
  // Focus takes priority.
  if (!focused) {
    inner_button_->GetBorderPtr()->set_color(
        active_ ? AshColorProvider::Get()->GetContentLayerColor(
                      AshColorProvider::ContentLayerType::kCurrentDeskColor)
                : SK_ColorTRANSPARENT);
    should_paint = true;
  }

  if (should_paint)
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
  // the same horizontal level. Note, don't get the label's width from
  // DeskNameView since desk's name is changeable, but this label here is not.
  const int label_height = desk_name_view->GetPreferredSize().height();
  label_->SetBoundsRect(gfx::Rect(
      gfx::Point((inner_button_bounds.width() - label_size.width()) / 2,
                 inner_button_bounds.bottom() -
                     desk_mini_view->GetPreviewBorderInsets().bottom() +
                     kDeskBarButtonAndNameSpacing),
      gfx::Size(label_size.width(), label_height)));
}

void ExpandedDesksBarButton::OnThemeChanged() {
  views::View::OnThemeChanged();
  label_->SetBackgroundColor(
      GetColorProvider()->GetColor(kColorAshShieldAndBase80));
  UpdateBorderColor();
}

BEGIN_METADATA(ExpandedDesksBarButton, views::View)
END_METADATA

}  // namespace ash
