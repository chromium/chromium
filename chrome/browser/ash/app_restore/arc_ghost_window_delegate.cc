// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_delegate.h"

#include "base/notreached.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash::full_restore {

namespace {

const int kNullWindowState = -1;

}  // namespace

ArcGhostWindowDelegate::ArcGhostWindowDelegate(
    exo::ClientControlledShellSurface* shell_surface,
    int window_id,
    const std::string& app_id,
    int64_t display_id,
    const gfx::Rect& bounds,
    chromeos::WindowStateType window_state)
    : window_id_(window_id),
      app_id_(app_id),
      bounds_(gfx::Rect(bounds)),
      pending_close_(false),
      window_state_(window_state),
      shell_surface_(shell_surface) {
  DCHECK(shell_surface);

  observation_.Observe(ArcGhostWindowHandler::Get());
  SetDisplayId(display_id);
}
ArcGhostWindowDelegate::~ArcGhostWindowDelegate() = default;

void ArcGhostWindowDelegate::OnGeometryChanged(const gfx::Rect& geometry) {}

void ArcGhostWindowDelegate::OnStateChanged(
    chromeos::WindowStateType old_state_type,
    chromeos::WindowStateType new_state) {
  if (old_state_type == new_state)
    return;

  auto* window_state =
      WindowState::Get(shell_surface_->GetWidget()->GetNativeWindow());

  if (!window_state || !shell_surface_->host_window()->GetRootWindow())
    return;

  display::Display display;
  const display::Screen* screen = display::Screen::GetScreen();
  auto display_existed = screen->GetDisplayWithDisplayId(display_id_, &display);
  DCHECK(display_existed);

  switch (new_state) {
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kDefault:
      // Reset geometry for previous bounds.
      shell_surface_->SetBounds(display_id_, bounds_);
      shell_surface_->SetRestored();
      break;
    case chromeos::WindowStateType::kMinimized:
      shell_surface_->SetMinimized();
      break;
    case chromeos::WindowStateType::kMaximized:
      shell_surface_->SetMaximized();
      // Update geometry for showing overlay as maximized bounds.
      shell_surface_->SetBounds(display_id_, display.bounds());
      break;
    case chromeos::WindowStateType::kFullscreen:
      // TODO(sstan): Adjust bounds like maximized state.
      shell_surface_->SetFullscreen(true, display::kInvalidDisplayId);
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
  shell_surface_->OnSurfaceCommit();
  window_state_ = new_state;
}

void ArcGhostWindowDelegate::OnBoundsChanged(
    chromeos::WindowStateType current_state,
    chromeos::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds_in_screen,
    bool is_resize,
    int bounds_change,
    bool is_adjusted_bounds) {
  auto* window_state =
      WindowState::Get(shell_surface_->GetWidget()->GetNativeWindow());

  if (!window_state || !shell_surface_->host_window()->GetRootWindow())
    return;

  display::Display target_display;
  const display::Screen* screen = display::Screen::GetScreen();

  if (!screen->GetDisplayWithDisplayId(display_id, &target_display))
    return;

  if (display_id_ != display_id) {
    if (!SetDisplayId(display_id))
      return;
  }

  // Don't change the bounds in maximize/fullscreen/pinned state.
  if (window_state->IsMaximizedOrFullscreenOrPinned() &&
      requested_state == window_state->GetStateType()) {
    return;
  }

  switch (requested_state) {
    case chromeos::WindowStateType::kPrimarySnapped:
      // TODO(b/279530665): Maybe sync to ARC.
      shell_surface_->SetSnapPrimary(chromeos::kDefaultSnapRatio);
      break;
    case chromeos::WindowStateType::kSecondarySnapped:
      // TODO(b/279530665): Maybe sync to ARC.
      shell_surface_->SetSnapSecondary(chromeos::kDefaultSnapRatio);
      break;
    case chromeos::WindowStateType::kFloated:
      // Ignore the unsupported request.
      return;
    default:
      if (requested_state != window_state->GetStateType()) {
        NOTREACHED_IN_MIGRATION();
      }
  }

  shell_surface_->SetBounds(display_id, bounds_in_screen);
  shell_surface_->OnSurfaceCommit();
  bounds_ = gfx::Rect(bounds_in_screen);
  UpdateWindowInfoToArc();
}

void ArcGhostWindowDelegate::OnDragStarted(int component) {}

void ArcGhostWindowDelegate::OnDragFinished(int x, int y, bool canceled) {}

void ArcGhostWindowDelegate::OnZoomLevelChanged(exo::ZoomChange zoom_change) {}

// ArcGhostWindowHandler::Observer
void ArcGhostWindowDelegate::OnAppInstanceConnected() {
  // Update window info to ARC when app instance connected, since the previous
  // window info may not be delivered.
  UpdateWindowInfoToArc();
}

void ArcGhostWindowDelegate::OnWindowCloseRequested(int window_id) {
  if (window_id != window_id_)
    return;
  pending_close_ = true;
  UpdateWindowInfoToArc();
}

void ArcGhostWindowDelegate::OnAppStatesUpdate(const std::string& app_id,
                                               bool ready,
                                               bool need_fixup) {
  if (app_id != app_id_)
    return;

  // Currently the type update is oneway. If an App need fixup, is not able to
  // become another state before it's ready.
  if (need_fixup) {
    static_cast<ArcGhostWindowShellSurface*>(shell_surface_)
        ->SetWindowType(arc::GhostWindowType::kFixup);
  }
}

bool ArcGhostWindowDelegate::SetDisplayId(int64_t display_id) {
  std::optional<double> scale_factor = GetDisplayScaleFactor(display_id);
  if (!scale_factor.has_value()) {
    LOG(ERROR) << "Invalid display id for ARC Ghost Window";
    scale_factor_ = 1.;
    return false;
  }
  scale_factor_ = scale_factor.value();
  display_id_ = display_id;
  return true;
}

void ArcGhostWindowDelegate::UpdateWindowInfoToArc() {
  ArcGhostWindowHandler::Get()->OnWindowInfoUpdated(
      window_id_, pending_close_ ? kNullWindowState : (int)window_state_,
      display_id_, gfx::ScaleToRoundedRect(bounds_, scale_factor_));
}

}  // namespace ash::full_restore
