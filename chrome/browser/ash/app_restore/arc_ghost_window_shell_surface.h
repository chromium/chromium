// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_

#include <utility>

#include "components/exo/client_controlled_shell_surface.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace full_restore {

class ArcWindowHandler;

std::unique_ptr<exo::ClientControlledShellSurface> InitArcGhostWindow(
    ArcWindowHandler* window_handler,
    const std::string& app_id,
    int window_id,
    absl::optional<int64_t> display_id,
    gfx::Rect bounds,
    absl::optional<chromeos::WindowStateType> window_state,
    absl::optional<gfx::Size> maximum_size,
    absl::optional<gfx::Size> minimum_size,
    absl::optional<std::u16string> title,
    absl::optional<uint32_t> color,
    base::RepeatingClosure close_callback);

// ArcGhostWindowShellSurface class is a shell surface which controlled its
// root surface.
class ArcGhostWindowShellSurface : public exo::ClientControlledShellSurface {
 public:
  ArcGhostWindowShellSurface(std::unique_ptr<exo::Surface> surface,
                             int container,
                             double scale_factor,
                             const std::string& application_id);
  ArcGhostWindowShellSurface(const ArcGhostWindowShellSurface&) = delete;
  ArcGhostWindowShellSurface& operator=(const ArcGhostWindowShellSurface&) =
      delete;
  ~ArcGhostWindowShellSurface() override;

  void OverrideInitParams(views::Widget::InitParams* params) override;

  void InitContentOverlay(const std::string& app_id, uint32_t theme_color);
  void SetAppId(const absl::optional<std::string>& id);

  exo::Surface* controller_surface();

 private:
  void SetShellAppId(ui::PropertyHandler* property_handler,
                     const absl::optional<std::string>& id);

  absl::optional<std::string> app_id_;

  std::unique_ptr<exo::Surface> controller_surface_;
  std::unique_ptr<exo::Buffer> buffer_;
};

}  // namespace full_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
