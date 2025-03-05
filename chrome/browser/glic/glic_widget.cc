// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_widget.h"

#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/common/chrome_features.h"
#include "ui/display/screen.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic {

void* kGlicWidgetIdentifier = &kGlicWidgetIdentifier;

GlicWidget::GlicWidget(InitParams params) : views::Widget(std::move(params)) {}

GlicWidget::~GlicWidget() = default;

std::unique_ptr<GlicWidget> GlicWidget::Create(
    Profile* profile,
    const gfx::Rect& initial_bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#if BUILDFLAG(IS_WIN)
  params.dont_show_in_taskbar = true;
  params.force_system_menu_for_frameless = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
#endif
  params.bounds = initial_bounds;
  params.sublevel = ChromeWidgetSublevel::kSublevelGlic;
  params.name = "GlicWidget";

  std::unique_ptr<GlicWidget> widget(new GlicWidget(std::move(params)));

  widget->SetContentsView(
      std::make_unique<GlicView>(profile, initial_bounds.size()));

  // Mac fullscreen uses this identifier to find this widget and reparent it to
  // the overlay widget.
  widget->SetNativeWindowProperty(views::kWidgetIdentifierKey,
                                  kGlicWidgetIdentifier);

  return widget;
}

display::Display GlicWidget::GetDisplay() {
  std::optional<display::Display> display = GetNearestDisplay();
  if (display) [[likely]] {
    return *display;
  }

  // This should not happen after Widget::Init().
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

}  // namespace glic
