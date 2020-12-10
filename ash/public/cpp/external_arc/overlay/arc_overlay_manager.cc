// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller_impl.h"
#include "base/logging.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"

namespace {

ash::ArcOverlayManager* singleton = nullptr;

const char* kBillingIdPrefix = "billing_id:";

}  // namespace

namespace ash {

ArcOverlayManager::ArcOverlayManager() {
  DCHECK(!singleton);
  singleton = this;

  env_observer_.Observe(aura::Env::GetInstance());
}

ArcOverlayManager::~ArcOverlayManager() {
  DCHECK(singleton);
  singleton = nullptr;
}

ArcOverlayManager* ArcOverlayManager::instance() {
  return singleton;
}

std::unique_ptr<ArcOverlayController> ArcOverlayManager::CreateController(
    aura::Window* host_window) {
  return std::make_unique<ArcOverlayControllerImpl>(host_window);
}

base::ScopedClosureRunner ArcOverlayManager::RegisterHostWindow(
    std::string overlay_token,
    aura::Window* host_window) {
  DCHECK_EQ(0u, token_to_controller_map_.count(overlay_token));
  DCHECK(host_window);

  token_to_controller_map_.emplace(overlay_token,
                                   CreateController(host_window));

  return base::ScopedClosureRunner(
      base::BindOnce(&ArcOverlayManager::DeregisterHostWindow,
                     base::Unretained(this), std::move(overlay_token)));
}

void ArcOverlayManager::DeregisterHostWindow(const std::string& overlay_token) {
  auto it = token_to_controller_map_.find(overlay_token);
  DCHECK(it != token_to_controller_map_.end());
  if (it == token_to_controller_map_.end())
    return;

  token_to_controller_map_.erase(it);
}

void ArcOverlayManager::OnWindowInitialized(aura::Window* window) {
  // Ignore windows that do not have a delegate set.
  if (!window->delegate())
    return;

  // We only ever observe the most recent window being created
  unknown_window_observation_.Reset();
  unknown_window_observation_.Observe(window);
}

void ArcOverlayManager::OnWindowDestroying(aura::Window* window) {
  if (unknown_window_observation_.IsObservingSource(window))
    unknown_window_observation_.Reset();

  if (overlay_window_observations_.IsObservingSource(window))
    overlay_window_observations_.RemoveObservation(window);
}

void ArcOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  // We only care about property changes on the single unknown window.
  // (We also are observing other windows via overlay_window_observations_)
  if (!unknown_window_observation_.IsObservingSource(window))
    return;

  // exo::ShellSurfaceBase sets this key soon after creating the window
  if (!exo::IsShellMainSurfaceKey(key))
    return;

  // It may still be of interest as an overlay, but we don't need to observe it
  // as an unknown window.
  unknown_window_observation_.Reset();

  // If this isn't actually a variant of a exo::ShellSurfaceBase, it is not an
  // overlay candidate.
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  if (!shell_surface_base)
    return;

  auto* shell_root_surface = shell_surface_base->root_surface();
  DCHECK(shell_root_surface);

  // If the client_surface_id doesn't have a particular prefix, it is not an
  // overlay candidate.
  std::string client_surface_id = shell_root_surface->GetClientSurfaceId();
  if (!base::StartsWith(client_surface_id, kBillingIdPrefix))
    return;

  // This window seems to be an overlay candidate. Continue observing it as one
  // until it is ready. exo::ShellSurfaceBase is still setting it up.
  overlay_window_observations_.AddObservation(window);
}

void ArcOverlayManager::OnWindowVisibilityChanged(aura::Window* window,
                                                  bool visible) {
  // For this event, we only care about windows that are potential overlays.
  if (!overlay_window_observations_.IsObservingSource(window))
    return;

  // We only care about windows that are now visible.
  if (!visible)
    return;

  // We do not need to keep observing the window.
  overlay_window_observations_.RemoveObservation(window);

  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  DCHECK(shell_surface_base);
  auto* shell_root_surface = shell_surface_base->root_surface();
  DCHECK(shell_root_surface);

  std::string client_surface_id = shell_root_surface->GetClientSurfaceId();
  std::string overlay_token =
      client_surface_id.substr(strlen(kBillingIdPrefix));

  // Find and attach the overlay to the host window.
  RegisterOverlayWindow(std::move(overlay_token), shell_surface_base);
}

void ArcOverlayManager::RegisterOverlayWindow(
    std::string overlay_token,
    exo::ShellSurfaceBase* shell_surface_base) {
  auto it = token_to_controller_map_.find(overlay_token);
  if (it == token_to_controller_map_.end()) {
    LOG(WARNING) << "No host window registered for token " << overlay_token;
    return;
  }

  // Use the shell surface widget window as the overlay
  DCHECK(shell_surface_base->GetWidget());
  DCHECK(shell_surface_base->GetWidget()->GetNativeWindow());
  it->second->AttachOverlay(shell_surface_base->GetWidget()->GetNativeWindow());
}

}  // namespace ash
