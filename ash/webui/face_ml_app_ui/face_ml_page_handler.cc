// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/face_ml_app_ui/face_ml_page_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/task/thread_pool.h"

namespace ash {
FaceMLPageHandler::FaceMLPageHandler(FaceMLAppUI* face_ml_app_ui)
    : face_ml_app_ui_(*face_ml_app_ui) {}
FaceMLPageHandler::~FaceMLPageHandler() = default;

void FaceMLPageHandler::BindInterface(
    mojo::PendingReceiver<mojom::face_ml_app::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::face_ml_app::Page> pending_page) {
  receiver_.Bind(std::move(pending_receiver));
  page_.Bind(std::move(pending_page));
}

void FaceMLPageHandler::GetCurrentUserInformation(
    GetCurrentUserInformationCallback callback) {
  mojom::face_ml_app::UserInformation user_info =
      face_ml_app_ui_->GetUserProvider()->GetCurrentUserInformation();
  std::move(callback).Run(user_info.Clone());
}
}  // namespace ash
