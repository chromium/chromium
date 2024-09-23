// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_label_widget.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/style/rounded_label.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

namespace ash {

RoundedLabelWidget::InitParams::InitParams() = default;

RoundedLabelWidget::InitParams::InitParams(InitParams&& other) = default;

RoundedLabelWidget::InitParams::~InitParams() = default;

RoundedLabelWidget::RoundedLabelWidget() = default;

RoundedLabelWidget::~RoundedLabelWidget() = default;

void RoundedLabelWidget::Init(InitParams params) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  widget_params.name = params.name;
  widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_params.layer_type = ui::LAYER_NOT_DRAWN;
  widget_params.accept_events = false;
  widget_params.parent = params.parent;
  widget_params.init_properties_container.SetProperty(kHideInDeskMiniViewKey,
                                                      true);
  widget_params.init_properties_container.SetProperty(kOverviewUiKey, true);
  set_focus_on_creation(false);
  views::Widget::Init(std::move(widget_params));

  if (params.disable_default_visibility_animation) {
    SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  }

  SetContentsView(std::make_unique<RoundedLabel>(
      params.horizontal_padding, params.vertical_padding, params.rounding_dp,
      params.preferred_height,
      absl::holds_alternative<std::u16string>(params.message)
          ? absl::get<std::u16string>(params.message)
          : l10n_util::GetStringUTF16(absl::get<int>(params.message))));
  Show();
}

gfx::Rect RoundedLabelWidget::GetBoundsCenteredIn(const gfx::Rect& bounds) {
  views::View* contents_view = GetContentsView();
  DCHECK(contents_view);
  gfx::Rect widget_bounds = bounds;
  widget_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());
  return widget_bounds;
}

void RoundedLabelWidget::SetBoundsCenteredIn(const gfx::Rect& bounds_in_screen,
                                             bool animate) {
  auto* window = GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings{
      animate ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW
              : OVERVIEW_ANIMATION_NONE,
      window};
  window->SetBoundsInScreen(
      GetBoundsCenteredIn(bounds_in_screen),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

}  // namespace ash
