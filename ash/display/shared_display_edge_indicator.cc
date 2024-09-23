// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/shared_display_edge_indicator.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

std::unique_ptr<views::Widget> CreateWidget(const gfx::Rect& bounds) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.context = Shell::GetRootWindowControllerWithDisplayId(display.id())
                       ->GetRootWindow();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "SharedDisplayEdgeIndicator";
  widget->set_focus_on_creation(false);
  widget->Init(std::move(params));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  aura::Window* window = widget->GetNativeWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(window->GetRootWindow());
  screen_position_client->SetBounds(window, bounds, display);
  widget->Show();
  return widget;
}

}  // namespace

SharedDisplayEdgeIndicator::SharedDisplayEdgeIndicator() = default;

SharedDisplayEdgeIndicator::~SharedDisplayEdgeIndicator() = default;

void SharedDisplayEdgeIndicator::Show(const gfx::Rect& src_bounds,
                                      const gfx::Rect& dst_bounds) {
  DCHECK(!src_widget_);
  DCHECK(!dst_widget_);
  src_widget_ = CreateWidget(src_bounds);
  dst_widget_ = CreateWidget(dst_bounds);

  animation_ = std::make_unique<gfx::ThrobAnimation>(this);
  gfx::AnimationContainer* container = new gfx::AnimationContainer();
  container->SetAnimationRunner(
      std::make_unique<views::CompositorAnimationRunner>(src_widget_.get(),
                                                         FROM_HERE));
  animation_->SetContainer(container);
  animation_->SetThrobDuration(base::Milliseconds(1000));
  animation_->StartThrobbing(/*infinite=*/-1);
}

void SharedDisplayEdgeIndicator::AnimationProgressed(
    const gfx::Animation* animation) {
  int value = animation->CurrentValueBetween(0, 255);
  SkColor color = SkColorSetARGB(0xFF, value, value, value);
  src_widget_->GetLayer()->SetColor(color);
  dst_widget_->GetLayer()->SetColor(color);
}

}  // namespace ash
