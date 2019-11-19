// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/shared_display_edge_indicator.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class IndicatorView : public views::View {
 public:
  IndicatorView() = default;
  ~IndicatorView() override = default;

  void SetColor(SkColor color) {
    color_ = color;
    SchedulePaint();
  }

  // views::Views overrides:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(bounds().size()), color_);
  }

 private:
  SkColor color_ = SK_ColorTRANSPARENT;
  DISALLOW_COPY_AND_ASSIGN(IndicatorView);
};

views::Widget* CreateWidget(const gfx::Rect& bounds,
                            views::View* contents_view) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.context = Shell::GetRootWindowControllerWithDisplayId(display.id())
                       ->GetRootWindow();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  widget->set_focus_on_creation(false);
  widget->Init(std::move(params));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->GetNativeWindow()->SetName("SharedEdgeIndicator");
  widget->SetContentsView(contents_view);
  aura::Window* window = widget->GetNativeWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  screen_position_client->SetBounds(window, bounds, display);
  widget->Show();
  return widget;
}

}  // namespace

SharedDisplayEdgeIndicator::SharedDisplayEdgeIndicator()
    : src_indicator_(NULL), dst_indicator_(NULL) {}

SharedDisplayEdgeIndicator::~SharedDisplayEdgeIndicator() {
  Hide();
}

void SharedDisplayEdgeIndicator::Show(const gfx::Rect& src_bounds,
                                      const gfx::Rect& dst_bounds) {
  DCHECK(!src_indicator_);
  DCHECK(!dst_indicator_);
  src_indicator_ = new IndicatorView;
  dst_indicator_ = new IndicatorView;
  CreateWidget(src_bounds, src_indicator_);
  CreateWidget(dst_bounds, dst_indicator_);
  animation_.reset(new gfx::ThrobAnimation(this));
  animation_->SetThrobDuration(base::TimeDelta::FromMilliseconds(1000));
  animation_->StartThrobbing(-1 /* infinite */);
}

void SharedDisplayEdgeIndicator::Hide() {
  if (src_indicator_)
    src_indicator_->GetWidget()->Close();
  src_indicator_ = NULL;
  if (dst_indicator_)
    dst_indicator_->GetWidget()->Close();
  dst_indicator_ = NULL;
}

void SharedDisplayEdgeIndicator::AnimationProgressed(
    const gfx::Animation* animation) {
  int value = animation->CurrentValueBetween(0, 255);
  SkColor color = SkColorSetARGB(0xFF, value, value, value);
  if (src_indicator_)
    static_cast<IndicatorView*>(src_indicator_)->SetColor(color);
  if (dst_indicator_)
    static_cast<IndicatorView*>(dst_indicator_)->SetColor(color);
}

}  // namespace ash
