// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/rounded_label_widget.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// The contents of RoundedLabelWidget. It is a rounded background with a label
// containing text we want to display.
class RoundedLabelView : public views::Label {
 public:
  RoundedLabelView(int horizontal_padding,
                   int vertical_padding,
                   SkColor background_color,
                   SkColor foreground_color,
                   int rounding_dp,
                   int preferred_height,
                   int message_id)
      : views::Label(l10n_util::GetStringUTF16(message_id),
                     views::style::CONTEXT_LABEL),
        preferred_height_(preferred_height) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(vertical_padding, horizontal_padding)));
    SetBackground(views::CreateSolidBackground(background_color));

    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetEnabledColor(foreground_color);
    SetBackgroundColor(background_color);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    const gfx::RoundedCornersF radii(rounding_dp);
    layer()->SetRoundedCornerRadius(radii);
    layer()->SetIsFastRoundedCorner(true);
  }
  RoundedLabelView(const RoundedLabelView&) = delete;
  RoundedLabelView& operator=(const RoundedLabelView&) = delete;
  ~RoundedLabelView() override = default;

  // views::Label:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(views::Label::CalculatePreferredSize().width(),
                     preferred_height_);
  }

 private:
  const int preferred_height_;
};

}  // namespace

RoundedLabelWidget::InitParams::InitParams() = default;

RoundedLabelWidget::InitParams::InitParams(InitParams&& other) = default;

RoundedLabelWidget::RoundedLabelWidget() = default;

RoundedLabelWidget::~RoundedLabelWidget() = default;

void RoundedLabelWidget::Init(InitParams params) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_POPUP);
  widget_params.name = params.name;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_params.layer_type = ui::LAYER_NOT_DRAWN;
  widget_params.accept_events = false;
  widget_params.parent = params.parent;
  set_focus_on_creation(false);
  if (params.hide_in_mini_view) {
    widget_params.init_properties_container.SetProperty(kHideInDeskMiniViewKey,
                                                        true);
  }
  views::Widget::Init(std::move(widget_params));

  SetContentsView(std::make_unique<RoundedLabelView>(
      params.horizontal_padding, params.vertical_padding,
      params.background_color, params.foreground_color, params.rounding_dp,
      params.preferred_height, params.message_id));
  Show();
}

gfx::Rect RoundedLabelWidget::GetBoundsCenteredIn(const gfx::Rect& bounds) {
  DCHECK(GetContentsView());
  RoundedLabelView* contents_view =
      static_cast<RoundedLabelView*>(GetContentsView());
  gfx::Rect widget_bounds = bounds;
  widget_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());
  return widget_bounds;
}

void RoundedLabelWidget::SetBoundsCenteredIn(const gfx::Rect& bounds,
                                             bool animate) {
  auto* window = GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings{
      animate ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW
              : OVERVIEW_ANIMATION_NONE,
      window};
  window->SetBounds(GetBoundsCenteredIn(bounds));
}

}  // namespace ash
