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
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

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
                              DeskBarViewBase* bar_view,
                              base::RepeatingClosure callback,
                              const std::u16string& text)
      : DeskButtonBase(text,
                       /*set_text=*/false,
                       bar_view,
                       std::move(callback),
                       kCornerRadius),
        outer_button_(outer_button) {}
  InnerExpandedDesksBarButton(const InnerExpandedDesksBarButton&) = delete;
  InnerExpandedDesksBarButton operator=(const InnerExpandedDesksBarButton&) =
      delete;
  ~InnerExpandedDesksBarButton() override = default;

  absl::optional<ui::ColorId> focus_color_id() { return focus_color_id_; }
  void set_focus_color_id(absl::optional<ui::ColorId> focus_color_id) {
    focus_color_id_ = focus_color_id;
  }

  // views::View:
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

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return DeskMiniView::GetDeskPreviewBounds(bar_view_->root()).size();
  }

  void SetButtonState(bool enabled) {
    outer_button_->UpdateLabelColor(enabled);
    // Notify the overview highlight if we are about to be disabled.
    // TODO(b/277988182): Add highlight/chromevoxing support for bento button
    // desk bar outside of overview.
    if (!enabled && bar_view_->type() == DeskBarViewBase::Type::kOverview) {
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

  void UpdateFocusState() override { outer_button_->UpdateFocusColor(); }

 private:
  raw_ptr<ExpandedDesksBarButton, ExperimentalAsh> outer_button_;
  absl::optional<ui::ColorId> focus_color_id_;
};

BEGIN_METADATA(InnerExpandedDesksBarButton, views::LabelButton)
END_METADATA

// -----------------------------------------------------------------------------
// ExpandedDesksBarButton:

ExpandedDesksBarButton::ExpandedDesksBarButton(
    DeskBarViewBase* bar_view,
    const gfx::VectorIcon* button_icon,
    const std::u16string& button_label,
    bool initially_enabled,
    base::RepeatingClosure callback)
    : bar_view_(bar_view),
      button_icon_(button_icon),
      button_label_(button_label),
      inner_button_(AddChildView(
          std::make_unique<InnerExpandedDesksBarButton>(this,
                                                        bar_view,
                                                        callback,
                                                        button_label))),
      label_(AddChildView(std::make_unique<views::Label>())) {
  DCHECK(button_icon_);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetButtonState(initially_enabled);

  views::InstallRoundRectHighlightPathGenerator(
      inner_button_, gfx::Insets(kFocusRingHaloInset), kBorderCornerRadius);
  auto* focus_ring = views::FocusRing::Get(inner_button_);
  focus_ring->SetHasFocusPredicate(base::BindRepeating(
      [](const ExpandedDesksBarButton* desks_bar_button,
         const views::View* view) {
        const auto* inner_button =
            views::AsViewClass<InnerExpandedDesksBarButton>(view);
        CHECK(inner_button);
        return inner_button->IsViewHighlighted() ||
               ((desks_bar_button->bar_view_->dragged_item_over_bar() &&
                 desks_bar_button->IsPointOnButton(
                     desks_bar_button->bar_view_
                         ->last_dragged_item_screen_location())) ||
                desks_bar_button->active_);
      },
      base::Unretained(this)));
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

void ExpandedDesksBarButton::UpdateFocusColor() const {
  DCHECK(inner_button_);
  auto* inner_button_focus_ring = views::FocusRing::Get(inner_button_);
  absl::optional<ui::ColorId> new_focus_color_id;

  if (inner_button_->IsViewHighlighted() ||
      (bar_view_->dragged_item_over_bar() &&
       IsPointOnButton(bar_view_->last_dragged_item_screen_location()))) {
    new_focus_color_id = ui::kColorAshFocusRing;
  } else if (active_) {
    new_focus_color_id = kColorAshCurrentDeskColor;
  } else {
    new_focus_color_id = absl::nullopt;
  }

  if (inner_button_->focus_color_id() == new_focus_color_id)
    return;

  // Only repaint the focus ring if the color gets updated.
  inner_button_->set_focus_color_id(new_focus_color_id);
  inner_button_focus_ring->SetColorId(new_focus_color_id);

  inner_button_focus_ring->SchedulePaint();
}

void ExpandedDesksBarButton::Layout() {
  // Layout the button until |mini_views_| have been created. This button only
  // needs to be laid out in the expanded desks bar where the |mini_views_| is
  // always not empty.
  if (bar_view_->mini_views().empty())
    return;
  const gfx::Rect inner_button_bounds = {{0, 0}, CalculatePreferredSize()};
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
  UpdateFocusColor();
}

gfx::Size ExpandedDesksBarButton::CalculatePreferredSize() const {
  return inner_button_->GetPreferredSize();
}

absl::optional<ui::ColorId>
ExpandedDesksBarButton::GetFocusColorIdForTesting() {
  return inner_button_->focus_color_id();
}

BEGIN_METADATA(ExpandedDesksBarButton, views::View)
END_METADATA

}  // namespace ash
