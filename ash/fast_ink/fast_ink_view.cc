// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_view.h"

#include <memory>

#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

FastInkView::FastInkView() = default;

FastInkView::~FastInkView() = default;

// static
views::UniqueWidgetPtr FastInkView::CreateWidgetWithContents(
    std::unique_ptr<FastInkView> fast_ink_view,
    aura::Window* container) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "FastInkOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = container;
  params.layer_type = ui::LAYER_SOLID_COLOR;

  aura::Window* root_window = container->GetRootWindow();
  gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(std::move(params)));
  FastInkView* fast_ink_view_ptr =
      widget->SetContentsView(std::move(fast_ink_view));
  widget->SetBounds(screen_bounds);

  auto fast_ink_host = std::make_unique<FastInkHost>();
  fast_ink_host->Init(widget->GetNativeWindow());

  // PresentationCallback must to be set after `fast_ink_host` is initialized.
  fast_ink_host->SetPresentationCallback(
      fast_ink_view_ptr->GetPresentationCallback());
  fast_ink_view_ptr->SetFastInkHost(std::move(fast_ink_host));

  widget->Show();
  return widget;
}

void FastInkView::UpdateSurface(const gfx::Rect& content_rect,
                                const gfx::Rect& damage_rect,
                                bool auto_refresh) {
  if (auto_refresh) {
    host_->AutoUpdateSurface(content_rect, damage_rect);
  } else {
    host_->UpdateSurface(content_rect, damage_rect, /*synchronous_draw=*/false);
  }
}

std::unique_ptr<FastInkHost::ScopedPaint> FastInkView::GetScopedPaint(
    const gfx::Rect& damage_rect_in_window) const {
  return host_->CreateScopedPaint(damage_rect_in_window);
}

FastInkHost::PresentationCallback FastInkView::GetPresentationCallback() {
  return FastInkHost::PresentationCallback();
}

void FastInkView::SetFastInkHost(std::unique_ptr<FastInkHost> host) {
  host_ = std::move(host);
}

}  // namespace ash
