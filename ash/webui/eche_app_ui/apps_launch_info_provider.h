// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_LAUNCH_INFO_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_LAUNCH_INFO_PROVIDER_H_

#include <cstdint>
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"

namespace ash {
namespace eche_app {

// A class to store app stream entry point and last connection status.
class AppsLaunchInfoProvider {
 public:
  explicit AppsLaunchInfoProvider(EcheConnectionStatusHandler*);
  ~AppsLaunchInfoProvider() = default;

  AppsLaunchInfoProvider(const AppsLaunchInfoProvider&) = delete;
  AppsLaunchInfoProvider& operator=(const AppsLaunchInfoProvider&) = delete;

  void SetAppLaunchInfo(mojom::AppStreamLaunchEntryPoint entry_point);

  mojom::ConnectionStatus GetConnectionStatusFromLastAttempt() {
    return last_connection_;
  }

  mojom::AppStreamLaunchEntryPoint entry_point() { return entry_point_; }

 private:
  raw_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_;
  mojom::AppStreamLaunchEntryPoint entry_point_ =
      mojom::AppStreamLaunchEntryPoint::UNKNOWN;
  mojom::ConnectionStatus last_connection_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_LAUNCH_INFO_PROVIDER_H_
