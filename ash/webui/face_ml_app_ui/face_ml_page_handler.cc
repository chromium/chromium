// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/face_ml_app_ui/face_ml_page_handler.h"

#include <utility>

namespace ash {
FaceMLPageHandler::FaceMLPageHandler() = default;
FaceMLPageHandler::~FaceMLPageHandler() = default;

void FaceMLPageHandler::BindInterface(
    mojo::PendingReceiver<mojom::face_ml_app::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::face_ml_app::Page> pending_page) {
  receiver_.Bind(std::move(pending_receiver));
  page_.Bind(std::move(pending_page));
}

}  // namespace ash
