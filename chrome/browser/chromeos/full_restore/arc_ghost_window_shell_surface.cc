// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_ghost_window_shell_surface.h"

#include "ash/wm/desks/desks_util.h"
#include "chrome/browser/chromeos/full_restore/arc_ghost_window_delegate.h"
#include "chrome/browser/chromeos/full_restore/arc_ghost_window_view.h"
#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/exo/buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/views/window/caption_button_types.h"

namespace {

constexpr int kDiameter = 24;

}  // namespace

namespace chromeos {
namespace full_restore {

std::unique_ptr<exo::ClientControlledShellSurface> InitArcGhostWindow(
    ArcWindowHandler* window_handler,
    const std::string& app_id,
    int window_id,
    int64_t display_id,
    gfx::Rect bounds,
    absl::optional<gfx::Size> maximum_size,
    absl::optional<gfx::Size> minimum_size,
    absl::optional<uint32_t> color,
    base::RepeatingClosure close_callback) {
  absl::optional<double> scale_factor = GetDisplayScaleFactor(display_id);
  DCHECK(scale_factor.has_value());

  // TODO(sstan): Fallback to system default color or other topic color, if
  // the task hasn't valid theme color.
  uint32_t theme_color = color.has_value() && IsValidThemeColor(color.value())
                             ? color.value()
                             : SK_ColorWHITE;

  // TODO(sstan): Handle the desk container from full_restore data.
  int container = ash::desks_util::GetActiveDeskContainerId();

  auto surface = std::make_unique<exo::Surface>();
  auto shell_surface = std::make_unique<ArcGhostWindowShellSurface>(
      std::move(surface), container, scale_factor.value());

  // TODO(sstan): Add set_surface_destroyed_callback.
  shell_surface->set_delegate(std::make_unique<ArcGhostWindowDelegate>(
      shell_surface.get(), window_handler, window_id, display_id, bounds));
  shell_surface->set_close_callback(std::move(close_callback));

  shell_surface->SetApplicationId(app_id.c_str());
  shell_surface->SetBounds(display_id, bounds);

  if (maximum_size.has_value())
    shell_surface->SetMaximumSize(maximum_size.value());

  if (minimum_size.has_value())
    shell_surface->SetMinimumSize(minimum_size.value());

  // Set frame buttons.
  constexpr uint32_t kAllButtonMask =
      1 << views::CAPTION_BUTTON_ICON_MINIMIZE |
      1 << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE |
      1 << views::CAPTION_BUTTON_ICON_CLOSE |
      1 << views::CAPTION_BUTTON_ICON_BACK |
      1 << views::CAPTION_BUTTON_ICON_MENU;
  shell_surface->SetFrameButtons(kAllButtonMask, kAllButtonMask);
  shell_surface->OnSetFrameColors(theme_color, theme_color);

  shell_surface->InitContentOverlay(app_id, theme_color);

  // Relayout overlay.
  shell_surface->controller_surface()->Commit();
  shell_surface->GetWidget()->LayoutRootViewIfNecessary();

  return shell_surface;
}

ArcGhostWindowShellSurface::ArcGhostWindowShellSurface(
    std::unique_ptr<exo::Surface> surface,
    int container,
    double scale_factor)
    : ClientControlledShellSurface(surface.get(),
                                   /*can_minimize=*/true,
                                   container,
                                   /*default_scale_cancellation=*/true) {
  controller_surface_ = std::move(surface);
  buffer_ = std::make_unique<exo::Buffer>(
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBuffer({1, 1}, gfx::BufferFormat::RGBA_8888,
                                  gfx::BufferUsage::GPU_READ,
                                  gpu::kNullSurfaceHandle, nullptr));
  controller_surface_->Attach(buffer_.get());
  controller_surface_->SetFrame(exo::SurfaceFrameType::NORMAL);
  controller_surface_->Commit();
  SetScale(scale_factor);
  CommitPendingScale();
}

ArcGhostWindowShellSurface::~ArcGhostWindowShellSurface() {
  controller_surface_.reset();
  buffer_.reset();
}

exo::Surface* ArcGhostWindowShellSurface::controller_surface() {
  return controller_surface_.get();
}

void ArcGhostWindowShellSurface::InitContentOverlay(const std::string& app_id,
                                                    uint32_t theme_color) {
  exo::ShellSurfaceBase::OverlayParams overlay_params(
      std::make_unique<ArcGhostWindowView>(kDiameter, app_id, theme_color));
  overlay_params.translucent = true;
  overlay_params.overlaps_frame = false;
  AddOverlay(std::move(overlay_params));
}

}  // namespace full_restore
}  // namespace chromeos
