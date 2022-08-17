// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_controller.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

GlanceablesController::GlanceablesController() = default;

GlanceablesController::~GlanceablesController() = default;

void GlanceablesController::CreateUi() {
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.delegate = new views::WidgetDelegate;  // Takes ownership.
  params.delegate->SetOwnedByWidget(true);
  // Allow maximize so the glanceable container's FillLayoutManager can fill the
  // screen with the widget. This is required even for fullscreen widgets.
  params.delegate->SetCanMaximize(true);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "GlanceablesWidget";
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  // Create the glanceables widget on the primary display.
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_GlanceablesContainer);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_->Init(std::move(params));

  view_ = widget_->SetContentsView(std::make_unique<GlanceablesView>());

  widget_->Show();
}

void GlanceablesController::DestroyUi() {
  widget_.reset();
  view_ = nullptr;
}

void GlanceablesController::FetchData() {
  // GlanceablesWeatherView observes the weather model for updates.
  Shell::Get()
      ->ambient_controller()
      ->ambient_weather_controller()
      ->FetchWeather();
}

}  // namespace ash
