// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"

namespace ash {
namespace eche_app {

AppsLaunchInfoProvider::AppsLaunchInfoProvider(
    EcheConnectionStatusHandler* connection_handler)
    : eche_connection_status_handler_(connection_handler) {
  eche_connection_status_handler_->AddObserver(this);
}

AppsLaunchInfoProvider::~AppsLaunchInfoProvider() {
  eche_connection_status_handler_->RemoveObserver(this);
}

void AppsLaunchInfoProvider::OnConnectionStatusForUiChanged(
    mojom::ConnectionStatus connection_status) {
  last_connection_ = connection_status;
}

void AppsLaunchInfoProvider::SetEntryPoint(
    mojom::AppStreamLaunchEntryPoint entry_point) {
  entry_point_ = entry_point;
}

}  // namespace eche_app
}  // namespace ash