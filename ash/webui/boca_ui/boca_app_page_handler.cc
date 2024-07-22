// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include "ash/webui/boca_ui/boca_ui.h"

namespace ash {

BocaAppHandler::BocaAppHandler(
    BocaUI* boca_ui,
    mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
    mojo::PendingRemote<boca::mojom::Page> remote)
    : receiver_(this, std::move(receiver)),
      remote_(std::move(remote)),
      boca_ui_(boca_ui) {}

BocaAppHandler::~BocaAppHandler() = default;

}  // namespace ash
