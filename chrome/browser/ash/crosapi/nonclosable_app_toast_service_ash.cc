// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/nonclosable_app_toast_service_ash.h"

#include <string>

#include "chromeos/ash/components/nonclosable_app_ui/nonclosable_app_ui_utils.h"

namespace crosapi {

NonclosableAppToastServiceAsh::NonclosableAppToastServiceAsh() = default;
NonclosableAppToastServiceAsh::~NonclosableAppToastServiceAsh() = default;

void NonclosableAppToastServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NonclosableAppToastService> receiver) {
  nonclosable_app_toast_service_receiver_set_.Add(this, std::move(receiver));
}

void NonclosableAppToastServiceAsh::OnUserAttemptedClose(
    const std::string& app_id,
    const std::string& app_name) {
  ash::ShowNonclosableAppToast(app_id, app_name);
}

}  // namespace crosapi
