// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_page_handler.h"

namespace ash {

DemoModePageHandler::DemoModePageHandler(
    mojo::PendingReceiver<mojom::demo_mode::PageHandler> pending_receiver,
    views::Widget* widget)
    : receiver_(this, std::move(pending_receiver)), widget_(widget) {}

DemoModePageHandler::~DemoModePageHandler() = default;

void DemoModePageHandler::ToggleFullscreen() {
  widget_->SetFullscreen(!widget_->IsFullscreen());
}

}  // namespace ash
