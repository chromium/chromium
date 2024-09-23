// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/wm/desks/desks_util.h"
#include "base/check_op.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_delegate.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "ui/aura/env.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash::full_restore {

ArcGhostWindowShellSurface::ArcGhostWindowShellSurface(
    std::unique_ptr<exo::Surface> surface,
    int container,
    const std::string& application_id)
    : ClientControlledShellSurface(surface.get(),
                                   /*can_minimize=*/true,
                                   container,
                                   /*default_scale_cancellation=*/true,
                                   /*supports_floated_state=*/false) {
  controller_surface_ = std::move(surface);
  buffer_ = exo::Buffer::CreateBuffer(
      gfx::Size(1, 1), gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::GPU_READ,
      "ArcGhostWindowShellSurface", gpu::kNullSurfaceHandle,
      /*shutdown_event=*/nullptr);
  SetApplicationId(application_id.c_str());
  controller_surface_->Attach(buffer_.get());
  controller_surface_->SetFrame(exo::SurfaceFrameType::NORMAL);
}

ArcGhostWindowShellSurface::~ArcGhostWindowShellSurface() {
  controller_surface_.reset();
  buffer_.reset();
}

// static
std::unique_ptr<ArcGhostWindowShellSurface> ArcGhostWindowShellSurface::Create(
    const std::string& app_id,
    arc::GhostWindowType type,
    int window_id,
    const gfx::Rect& bounds,
    app_restore::AppRestoreData* restore_data,
    base::RepeatingClosure close_callback) {
  // ArcGhostWindowShellSurface need a valid display id, or it cannot be
  // created.
  int64_t display_id_value =
      restore_data->display_id.value_or(display::kInvalidDisplayId);

  const chromeos::WindowStateType window_state =
      restore_data->window_info.window_state_type.value_or(
          chromeos::WindowStateType::kDefault);

  gfx::Rect local_bounds = bounds;
  // If the window is maximize / minimized, the initial bounds will be
  // unnecessary. Here set it as display size to ensure the content render is
  // correct.
  if (local_bounds.IsEmpty()) {
    DCHECK(chromeos::IsMaximizedOrFullscreenWindowStateType(window_state) ||
           chromeos::IsMinimizedWindowStateType(window_state));
    display::Display disp;
    display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_value,
                                                          &disp);
    local_bounds = disp.work_area();
  }

  // TODO(sstan): Fallback to system default color or other topic color, if
  // the task hasn't valid theme color.
  uint32_t theme_color =
      restore_data->status_bar_color.has_value() &&
              IsValidThemeColor(restore_data->status_bar_color.value())
          ? restore_data->status_bar_color.value()
          : SK_ColorWHITE;

  // TODO(sstan): Handle the desk container from full_restore data.
  int container = desks_util::GetActiveDeskContainerId();

  auto surface = std::make_unique<exo::Surface>();
  std::unique_ptr<ArcGhostWindowShellSurface> shell_surface(
      new ArcGhostWindowShellSurface(std::move(surface), container,
                                     WrapSessionAppIdFromWindowId(window_id)));

  // TODO(sstan): Add set_surface_destroyed_callback.
  shell_surface->set_delegate(std::make_unique<ArcGhostWindowDelegate>(
      shell_surface.get(), window_id, app_id, display_id_value, local_bounds,
      window_state));
  shell_surface->set_close_callback(std::move(close_callback));

  shell_surface->SetAppId(app_id);
  shell_surface->SetBounds(display_id_value, local_bounds);

  const std::optional<app_restore::WindowInfo::ArcExtraInfo>& arc_info =
      restore_data->window_info.arc_extra_info;
  if (arc_info) {
    if (arc_info->maximum_size.has_value()) {
      shell_surface->SetMaximumSize(*arc_info->maximum_size);
    }
    if (arc_info->minimum_size.has_value()) {
      shell_surface->SetMinimumSize(*arc_info->minimum_size);
    }
  }

  if (restore_data->window_info.app_title.has_value()) {
    shell_surface->SetTitle(*restore_data->window_info.app_title);
  }

  // Set frame buttons.
  constexpr uint32_t kVisibleButtonMask =
      1 << views::CAPTION_BUTTON_ICON_MINIMIZE |
      1 << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE |
      1 << views::CAPTION_BUTTON_ICON_CLOSE |
      1 << views::CAPTION_BUTTON_ICON_BACK;
  shell_surface->SetFrameButtons(kVisibleButtonMask, kVisibleButtonMask);
  shell_surface->OnSetFrameColors(theme_color, theme_color);

  std::optional<gfx::RoundedCornersF> overlay_corners_radii;
  if (chromeos::features::IsRoundedWindowsEnabled()) {
    DCHECK_NE(window_state, chromeos::WindowStateType::kPip);

    const int window_corner_radius =
        chromeos::ShouldWindowStateHaveRoundedCorners(window_state)
            ? chromeos::features::RoundedWindowsRadius()
            : 0;

    gfx::RoundedCornersF window_radii(window_corner_radius);
    shell_surface->SetWindowCornersRadii(window_radii);

    // Ghost surface shadow radii must match the window radii.
    shell_surface->SetShadowCornersRadii(window_radii);

    // Ghost surface is an overlay widget, so its corners must be rounded. The
    // bottom two corners of the ghost window overlay overlap with the window,
    // so we need to round them.
    overlay_corners_radii =
        gfx::RoundedCornersF(0, 0, window_corner_radius, window_corner_radius);
  }

  shell_surface->controller_surface()->Commit();

  shell_surface->InitContentOverlay(app_id, theme_color, type,
                                    std::move(overlay_corners_radii));

  // Relayout overlay.
  shell_surface->GetWidget()->LayoutRootViewIfNecessary();

  // Change the window state at the last operation, since we need create the
  // window entity first.
  if (chromeos::IsMaximizedOrFullscreenWindowStateType(window_state)) {
    shell_surface->SetMaximized();
    shell_surface->controller_surface()->Commit();
  } else if (chromeos::IsMinimizedWindowStateType(window_state)) {
    shell_surface->SetMinimized();
    shell_surface->controller_surface()->Commit();
  } else {
    // Reset the same bounds, to make sure white background can be located in
    // correct place. Without this operation, the white background will not
    // at the same location with window bounds.
    shell_surface->SetBounds(display_id_value, local_bounds);
  }

  return shell_surface;
}

void ArcGhostWindowShellSurface::OverrideInitParams(
    views::Widget::InitParams* params) {
  ClientControlledShellSurface::OverrideInitParams(params);
  SetShellAppId(&params->init_properties_container, app_id_);
}

exo::Surface* ArcGhostWindowShellSurface::controller_surface() {
  return controller_surface_.get();
}

void ArcGhostWindowShellSurface::InitContentOverlay(
    const std::string& app_id,
    uint32_t theme_color,
    arc::GhostWindowType type,
    std::optional<gfx::RoundedCornersF>&& corners_radii) {
  std::string app_name;
  // TODO(sstan): Move this part out of shell surface.
  // In test env, ArcAppListPrefs or App maybe null.
  auto* pref = ArcAppListPrefs::Get(ProfileManager::GetPrimaryUserProfile());
  if (pref) {
    auto app_info = pref->GetApp(app_id);
    if (app_info)
      app_name = app_info->name;
  }
  auto view = std::make_unique<ArcGhostWindowView>(this, app_name);
  view_observer_ = view.get();
  view->LoadIcon(app_id);

  view->SetThemeColor(theme_color);
  view->SetGhostWindowViewType(type);

  exo::ShellSurfaceBase::OverlayParams overlay_params(std::move(view));
  overlay_params.translucent = true;
  overlay_params.overlaps_frame = false;
  overlay_params.corners_radii = std::move(corners_radii);
  AddOverlay(std::move(overlay_params));
}

void ArcGhostWindowShellSurface::SetAppId(
    const std::optional<std::string>& id) {
  app_id_ = id;
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    SetShellAppId(GetWidget()->GetNativeWindow(), app_id_);
  }
}

void ArcGhostWindowShellSurface::SetShellAppId(
    ui::PropertyHandler* property_handler,
    const std::optional<std::string>& id) {
  if (id)
    property_handler->SetProperty(app_restore::kAppIdKey, *id);
  else
    property_handler->ClearProperty(app_restore::kAppIdKey);
}

void ArcGhostWindowShellSurface::SetWindowType(
    arc::GhostWindowType window_type) {
  DCHECK(view_observer_);
  view_observer_->SetGhostWindowViewType(window_type);
}

}  // namespace ash::full_restore
