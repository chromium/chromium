// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_

#include <utility>

#include "components/exo/client_controlled_shell_surface.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace full_restore {

class ArcWindowHandler;

std::unique_ptr<exo::ClientControlledShellSurface> InitArcGhostWindow(
    ArcWindowHandler* window_handler,
    const std::string& app_id,
    int window_id,
    int64_t display_id,
    gfx::Rect bounds,
    absl::optional<gfx::Size> maximum_size,
    absl::optional<gfx::Size> minimum_size,
    absl::optional<uint32_t> color,
    base::RepeatingClosure close_callback);

// ArcGhostWindowShellSurface class is a shell surface which controlled its
// root surface.
class ArcGhostWindowShellSurface : public exo::ClientControlledShellSurface {
 public:
  ArcGhostWindowShellSurface(std::unique_ptr<exo::Surface> surface,
                             int container,
                             double scale_factor);
  ArcGhostWindowShellSurface(const ArcGhostWindowShellSurface&) = delete;
  ArcGhostWindowShellSurface& operator=(const ArcGhostWindowShellSurface&) =
      delete;
  ~ArcGhostWindowShellSurface() override;

  void InitContentOverlay(const std::string& app_id, uint32_t theme_color);

  exo::Surface* controller_surface();

 private:
  std::unique_ptr<exo::Surface> controller_surface_;
  std::unique_ptr<exo::Buffer> buffer_;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
