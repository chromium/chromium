// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/graduation/graduation_ui_handler.h"

#include <string>

#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash::graduation {

GraduationUiHandler::GraduationUiHandler(
    mojo::PendingReceiver<graduation_ui::mojom::GraduationUiHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

GraduationUiHandler::~GraduationUiHandler() = default;

void GraduationUiHandler::GetProfileInfo(GetProfileInfoCallback callback) {
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  CHECK(active_user);

  const gfx::ImageSkia icon = active_user->GetImage();

  std::move(callback).Run(graduation_ui::mojom::ProfileInfo::New(
      active_user->GetDisplayEmail(),
      webui::GetBitmapDataUrl(icon.GetRepresentation(1.0f).GetBitmap())));
}

}  // namespace ash::graduation
