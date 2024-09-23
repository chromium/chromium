// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/scroll_arrow_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "base/functional/bind.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {
base::TimeDelta g_scroll_time_interval = base::Seconds(1);
}

ScrollArrowButton::ScrollArrowButton(base::RepeatingClosure on_scroll,
                                     bool is_left_arrow,
                                     DeskBarViewBase* bar_view)
    : on_scroll_(std::move(on_scroll)),
      state_change_subscription_(AddStateChangedCallback(
          base::BindRepeating(&ScrollArrowButton::OnStateChanged,
                              base::Unretained(this)))),
      is_left_arrow_(is_left_arrow),
      bar_view_(bar_view) {
  GetViewAccessibility().SetName(base::UTF8ToUTF16(GetClassName()));
}

ScrollArrowButton::~ScrollArrowButton() = default;

void ScrollArrowButton::PaintButtonContents(gfx::Canvas* canvas) {
  const bool show_left_arrow = is_left_arrow_ ^ base::i18n::IsRTL();
  gfx::ImageSkia img = CreateVectorIcon(
      show_left_arrow ? kOverflowShelfLeftIcon : kOverflowShelfRightIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

  DCHECK(!bar_view_->mini_views().empty());
  const auto* mini_view = bar_view_->mini_views()[0].get();
  canvas->DrawImageInt(
      img, (width() - img.width()) / 2,
      mini_view->bounds().y() +
          (mini_view->desk_preview()->bounds().height() - img.height()) / 2);
}

void ScrollArrowButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  SchedulePaint();
}

void ScrollArrowButton::OnDeskHoverStart() {
  // Don't start the timer again, if it's already running.
  if (timer_.IsRunning())
    return;

  timer_.Start(FROM_HERE, g_scroll_time_interval, on_scroll_);
  on_scroll_.Run();
}

void ScrollArrowButton::OnDeskHoverEnd() {
  timer_.Stop();
}

void ScrollArrowButton::OnStateChanged() {
  if (GetState() == ButtonState::STATE_PRESSED) {
    // Please note that the order of the function calls matters. If call
    // |on_scroll_| first, when the scroll view moves to the end, the visibility
    // of the scroll arrow button will be set to |FALSE|, at the same time, the
    // state of the button will be set to |STATE_NORMAL|. In this case, stopping
    // timer will be called before starting timer.
    timer_.Start(FROM_HERE, g_scroll_time_interval, on_scroll_);
    on_scroll_.Run();
  } else {
    timer_.Stop();
  }
}

// static
base::AutoReset<base::TimeDelta>
ScrollArrowButton::SetScrollTimeIntervalForTest(base::TimeDelta interval) {
  return {&g_scroll_time_interval, interval};
}

BEGIN_METADATA(ScrollArrowButton)
END_METADATA

}  // namespace ash
