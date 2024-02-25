// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace app_restore {
struct AppRestoreData;
}  // namespace app_restore

namespace arc {
enum class GhostWindowType;
}

namespace ash::full_restore {

class ArcGhostWindowView;

// ArcGhostWindowShellSurface class is a shell surface which controlled its
// root surface.
class ArcGhostWindowShellSurface : public exo::ClientControlledShellSurface {
 public:
  ArcGhostWindowShellSurface(const ArcGhostWindowShellSurface&) = delete;
  ArcGhostWindowShellSurface& operator=(const ArcGhostWindowShellSurface&) =
      delete;
  ~ArcGhostWindowShellSurface() override;

  static std::unique_ptr<ArcGhostWindowShellSurface> Create(
      const std::string& app_id,
      arc::GhostWindowType type,
      int window_id,
      const gfx::Rect& bounds,
      app_restore::AppRestoreData* restore_data,
      base::RepeatingClosure close_callback);

  void OverrideInitParams(views::Widget::InitParams* params) override;

  void SetWindowType(arc::GhostWindowType window_type);

  exo::Surface* controller_surface();

 private:
  ArcGhostWindowShellSurface(std::unique_ptr<exo::Surface> surface,
                             int container,
                             const std::string& application_id);

  void InitContentOverlay(const std::string& app_id,
                          uint32_t theme_color,
                          arc::GhostWindowType type,
                          std::optional<gfx::RoundedCornersF>&& corners_radii);

  void SetAppId(const std::optional<std::string>& id);

  void SetShellAppId(ui::PropertyHandler* property_handler,
                     const std::optional<std::string>& id);

  raw_ptr<ArcGhostWindowView, DanglingUntriaged> view_observer_ = nullptr;
  std::optional<std::string> app_id_;

  std::unique_ptr<exo::Surface> controller_surface_;
  std::unique_ptr<exo::Buffer> buffer_;
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_SHELL_SURFACE_H_
