// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_untrusted_page_handler.h"

#include <utility>

#include "ash/webui/media_app_ui/media_app_guest_ui.h"

namespace ash {

namespace {}  // namespace

MediaAppUntrustedPageHandler::MediaAppUntrustedPageHandler(
    MediaAppGuestUI& media_app_guest_ui,
    mojo::PendingReceiver<media_app_ui::mojom::UntrustedPageHandler> receiver,
    mojo::PendingRemote<media_app_ui::mojom::UntrustedPage> page)
    : media_app_guest_ui_(media_app_guest_ui) {}

MediaAppUntrustedPageHandler::~MediaAppUntrustedPageHandler() = default;

}  // namespace ash
