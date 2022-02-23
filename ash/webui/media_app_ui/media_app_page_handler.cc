// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_page_handler.h"

#include <utility>

#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui_delegate.h"

namespace ash {

MediaAppPageHandler::MediaAppPageHandler(
    MediaAppUI* media_app_ui,
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), media_app_ui_(media_app_ui) {}

MediaAppPageHandler::~MediaAppPageHandler() = default;

void MediaAppPageHandler::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  auto error_message = media_app_ui_->delegate()->OpenFeedbackDialog();
  std::move(callback).Run(std::move(error_message));
}

void MediaAppPageHandler::ToggleBrowserFullscreenMode(
    ToggleBrowserFullscreenModeCallback callback) {
  media_app_ui_->delegate()->ToggleBrowserFullscreenMode();
  std::move(callback).Run();
}

}  // namespace ash
