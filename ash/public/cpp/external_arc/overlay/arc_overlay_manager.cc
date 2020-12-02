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
  // We only ever observe the most recent window being created
  if (observed_window_observer_.IsObserving())
    observed_window_observer_.RemoveObservation();
  observed_window_ = window;
  observed_window_observer_.Observe(observed_window_);
}

void ArcOverlayManager::OnWindowDestroying(aura::Window* window) {
  if (observed_window_observer_.IsObservingSource(window)) {
    observed_window_observer_.RemoveObservation();
    observed_window_ = nullptr;
  }
}

void ArcOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  // shell_surface_base sets this key soon after creating the widget
  if (!exo::IsShellMainSurfaceKey(key))
    return;

  // We don't need to observe the window after this.
  observed_window_observer_.RemoveObservation();
  observed_window_ = nullptr;

  // If this isn't a variant of a ShellSurfaceBase, ignore it
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  if (!shell_surface_base)
    return;

  auto* shell_root_surface = shell_surface_base->root_surface();
  DCHECK(shell_root_surface);

  // If client surface id doesn't have a particular prefix, ignore it entirely.
  std::string client_surface_id = shell_root_surface->GetClientSurfaceId();
  if (!base::StartsWith(client_surface_id, kBillingIdPrefix))
    return;

  std::string overlay_token =
      client_surface_id.substr(strlen(kBillingIdPrefix));

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
