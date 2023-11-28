// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller_impl.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"

namespace {

ash::ArcOverlayManager* singleton = nullptr;

const char* kBillingIdPrefix = "billing_id:";

std::optional<std::string> GetOverlayTokenForArcWindow(aura::Window* window) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  DCHECK(shell_surface_base);
  auto* shell_root_surface = shell_surface_base->root_surface();
  DCHECK(shell_root_surface);

  // If the client_surface_id doesn't have a particular prefix, it is not an
  // overlay candidate.
  std::string client_surface_id = shell_root_surface->GetClientSurfaceId();
  if (!base::StartsWith(client_surface_id, kBillingIdPrefix))
    return {};

  return client_surface_id.substr(strlen(kBillingIdPrefix));
}

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
  // Ignore windows that are container (no delegate), or non arc window.
  if (!window->delegate() || !ash::IsArcWindow(window))
    return;

  // See if a potentially valid overlay token is set on the window, to confirm
  // that it is intended to be an overlay window.
  std::optional<std::string> token = GetOverlayTokenForArcWindow(window);
  if (!token)
    return;

  // Disable animations on overlay windows.
  window->SetProperty(aura::client::kAnimationsDisabledKey, true);

  window_observations_.AddObservation(window);
}

void ArcOverlayManager::OnWindowDestroying(aura::Window* window) {
  window_observations_.RemoveObservation(window);
}

void ArcOverlayManager::OnWindowVisibilityChanged(aura::Window* window,
                                                  bool visible) {
  // We only care about windows that are now visible.
  if (!visible)
    return;

  // |window| can be descendants or ancestors.
  if (!window_observations_.IsObservingSource(window))
    return;

  // We do not need to keep observing the window.
  window_observations_.RemoveObservation(window);

  // Get the overlay token.
  std::optional<std::string> token = GetOverlayTokenForArcWindow(window);
  if (!token)
    return;

  // Find and attach the overlay to the host window.
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  RegisterOverlayWindow(std::move(token).value(), shell_surface_base);
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
  auto* window = shell_surface_base->GetWidget()->GetNativeWindow();
  it->second->AttachOverlay(window);

  window->SetProperty(aura::client::kSkipImeProcessing, true);
}

}  // namespace ash
