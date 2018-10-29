// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

StatusAreaWidgetTestApi::StatusAreaWidgetTestApi(StatusAreaWidget* widget)
    : widget_(widget) {}

StatusAreaWidgetTestApi::~StatusAreaWidgetTestApi() = default;

// static
void StatusAreaWidgetTestApi::BindRequest(
    mojom::StatusAreaWidgetTestApiRequest request) {
  StatusAreaWidget* widget =
      Shell::Get()->GetPrimaryRootWindowController()->GetStatusAreaWidget();
  mojo::MakeStrongBinding(std::make_unique<StatusAreaWidgetTestApi>(widget),
                          std::move(request));
}

void StatusAreaWidgetTestApi::TapSelectToSpeakTray(
    TapSelectToSpeakTrayCallback callback) {
  // The Select-to-Speak tray doesn't actually use the event, so construct
  // a bare bones event to perform the action.
  ui::TouchEvent event(
      ui::ET_TOUCH_PRESSED, gfx::Point(), base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH), 0);
  widget_->select_to_speak_tray_->PerformAction(event);
  std::move(callback).Run();
}

}  // namespace ash
