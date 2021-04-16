// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_

#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/wm_helper.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace exo {
class ClientControlledShellSurface;
}

namespace chromeos {
namespace full_restore {

// Returns true if the ARC supports ghost window.
bool IsArcGhostWindowEnabled();

// Converts window bounds from Chrome DP to ARC pixels units, and adjust
// window position on display.
apps::mojom::WindowInfoPtr ConvertToArcBounds(
    int64_t display_id,
    apps::mojom::WindowInfoPtr window_info);

// The ArcWindowHandler class provides control for ARC ghost window.
class ArcWindowHandler {
  // Map from window_session_id to exo::ClientControlledShellSurface.
  using ShellSurfaceMap =
      std::map<int, std::unique_ptr<exo::ClientControlledShellSurface>>;

  // This class populates the exo::ShellSurfaceBase to PropertyHandler by
  // the corresponding window session id.
  class WindowSessionResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    explicit WindowSessionResolver(ShellSurfaceMap* session_id_map);
    WindowSessionResolver(const WindowSessionResolver&) = delete;
    WindowSessionResolver& operator=(const WindowSessionResolver&) = delete;
    ~WindowSessionResolver() override = default;

    // exo::WMHelper::AppPropertyResolver:
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override;

   private:
    ShellSurfaceMap* session_id_map_;
  };

 public:
  ArcWindowHandler();
  ArcWindowHandler(const ArcWindowHandler&) = delete;
  ArcWindowHandler& operator=(const ArcWindowHandler&) = delete;
  ~ArcWindowHandler();

  void OnAppInstanceConnected();

 private:
  // The ghost windows would not send window info to app instance if the
  // instance had not connected yet.
  bool app_instance_connected_ = false;

  // Map window session id to ClientControlledShellSurface.
  ShellSurfaceMap session_id_to_shell_surface_;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
