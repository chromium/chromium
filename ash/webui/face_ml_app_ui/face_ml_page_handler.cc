// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/face_ml_app_ui/face_ml_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"

namespace ash {

// static
void FaceMLPageHandler::Create(
    FaceMLAppUI* face_ml_app_ui,
    mojo::PendingReceiver<mojom::face_ml_app::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::face_ml_app::Page> pending_page) {
  auto page_handler = base::WrapUnique(new FaceMLPageHandler(
      face_ml_app_ui, std::move(pending_receiver), std::move(pending_page)));
  content::SaveWebUIManagedInterfaceInDocument(face_ml_app_ui,
                                               std::move(page_handler));
}

FaceMLPageHandler::FaceMLPageHandler(
    FaceMLAppUI* face_ml_app_ui,
    mojo::PendingReceiver<mojom::face_ml_app::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::face_ml_app::Page> pending_page)
    : receiver_(this, std::move(pending_receiver)),
      page_(std::move(pending_page)),
      face_ml_app_ui_(raw_ref<FaceMLAppUI>::from_ptr(face_ml_app_ui)) {}

FaceMLPageHandler::~FaceMLPageHandler() = default;

void FaceMLPageHandler::GetCurrentUserInformation(
    GetCurrentUserInformationCallback callback) {
  mojom::face_ml_app::UserInformation user_info =
      face_ml_app_ui_->GetUserProvider()->GetCurrentUserInformation();
  std::move(callback).Run(user_info.Clone());
}
}  // namespace ash
