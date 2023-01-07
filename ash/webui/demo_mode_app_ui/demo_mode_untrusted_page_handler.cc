// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_untrusted_page_handler.h"

namespace ash {

DemoModeUntrustedPageHandler::DemoModeUntrustedPageHandler(
    mojo::PendingReceiver<mojom::demo_mode::UntrustedPageHandler>
        pending_receiver,
    views::Widget* widget)
    : receiver_(this, std::move(pending_receiver)), widget_(widget) {}

DemoModeUntrustedPageHandler::~DemoModeUntrustedPageHandler() = default;

void DemoModeUntrustedPageHandler::ToggleFullscreen() {
  widget_->SetFullscreen(!widget_->IsFullscreen());
}

}  // namespace ash
