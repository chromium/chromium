// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"

#include "ash/wm/desks/desks_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_delegate.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/views/window/caption_button_types.h"

namespace ash::full_restore {

namespace {

bool IsMaximizedState(
    const absl::optional<chromeos::WindowStateType>& window_state) {
  return window_state.has_value() &&
         (window_state.value() == chromeos::WindowStateType::kMaximized ||
          window_state.value() == chromeos::WindowStateType::kFullscreen);
}

bool IsMinimizedState(
    const absl::optional<chromeos::WindowStateType>& window_state) {
  return window_state.has_value() &&
         window_state.value() == chromeos::WindowStateType::kMinimized;
}

}  // namespace

// Explicitly identifies ARC ghost surface.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kArcGhostSurface, false)

ArcGhostWindowShellSurface::ArcGhostWindowShellSurface(
    std::unique_ptr<exo::Surface> surface,
    int container,
    double scale_factor,
    const std::string& application_id)
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
  SetApplicationId(application_id.c_str());
  controller_surface_->Attach(buffer_.get());
  controller_surface_->SetFrame(exo::SurfaceFrameType::NORMAL);
  SetScale(scale_factor);
  CommitPendingScale();
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
  absl::optional<double> scale_factor = GetDisplayScaleFactor(display_id_value);
  if (!scale_factor.has_value())
    return nullptr;

  const auto& window_state = restore_data->window_state_type;
  gfx::Rect local_bounds = bounds;
  // If the window is maximize / minimized, the initial bounds will be
  // unnecessary. Here set it as display size to ensure the content render is
  // correct.
  if (local_bounds.IsEmpty()) {
    DCHECK(IsMaximizedState(window_state) || IsMinimizedState(window_state));
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
                                     scale_factor.value(),
                                     WrapSessionAppIdFromWindowId(window_id)));

  // TODO(sstan): Add set_surface_destroyed_callback.
  shell_surface->set_delegate(std::make_unique<ArcGhostWindowDelegate>(
      shell_surface.get(), window_id, app_id, display_id_value, local_bounds,
      window_state.value_or(chromeos::WindowStateType::kDefault)));
  shell_surface->set_close_callback(std::move(close_callback));

  shell_surface->SetAppId(app_id);
  shell_surface->SetBounds(display_id_value, local_bounds);

  if (restore_data->maximum_size.has_value())
    shell_surface->SetMaximumSize(restore_data->maximum_size.value());

  if (restore_data->minimum_size.has_value())
    shell_surface->SetMinimumSize(restore_data->minimum_size.value());

  if (restore_data->title.has_value())
    shell_surface->SetTitle(restore_data->title.value());

  // Set frame buttons.
  constexpr uint32_t kAllButtonMask =
      1 << views::CAPTION_BUTTON_ICON_MINIMIZE |
      1 << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE |
      1 << views::CAPTION_BUTTON_ICON_CLOSE |
      1 << views::CAPTION_BUTTON_ICON_BACK |
      1 << views::CAPTION_BUTTON_ICON_MENU;
  shell_surface->SetFrameButtons(kAllButtonMask, kAllButtonMask);
  shell_surface->OnSetFrameColors(theme_color, theme_color);

  shell_surface->controller_surface()->Commit();

  shell_surface->InitContentOverlay(app_id, theme_color, type);

  // Relayout overlay.
  shell_surface->GetWidget()->LayoutRootViewIfNecessary();

  // Change the window state at the last operation, since we need create the
  // window entity first.
  if (IsMaximizedState(window_state)) {
    shell_surface->SetMaximized();
    shell_surface->controller_surface()->Commit();
  } else if (IsMinimizedState(window_state)) {
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
  params->init_properties_container.SetProperty(kArcGhostSurface, true);
}

exo::Surface* ArcGhostWindowShellSurface::controller_surface() {
  return controller_surface_.get();
}

void ArcGhostWindowShellSurface::InitContentOverlay(const std::string& app_id,
                                                    uint32_t theme_color,
                                                    arc::GhostWindowType type) {
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
  AddOverlay(std::move(overlay_params));
}

void ArcGhostWindowShellSurface::SetAppId(
    const absl::optional<std::string>& id) {
  app_id_ = id;
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    SetShellAppId(GetWidget()->GetNativeWindow(), app_id_);
  }
}

void ArcGhostWindowShellSurface::SetShellAppId(
    ui::PropertyHandler* property_handler,
    const absl::optional<std::string>& id) {
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
