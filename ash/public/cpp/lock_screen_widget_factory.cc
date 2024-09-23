// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lock_screen_widget_factory.h"

#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

std::unique_ptr<views::Widget> CreateLockScreenWidget(
    aura::Window* parent,
    std::unique_ptr<views::View> contents_view) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new views::WidgetDelegate();
  params.delegate->SetOwnedByWidget(true);
  params.delegate->SetContentsView(std::move(contents_view));
  params.delegate->SetInitiallyFocusedView(params.delegate->GetContentsView());

  params.show_state = ui::mojom::WindowShowState::kFullscreen;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.name = "LockScreenWidget";
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  return widget;
}

}  // namespace ash
