// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_MANAGER_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_MANAGER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
class Env;
}  // namespace aura

namespace exo {
class ShellSurfaceBase;
}

namespace ash {

class ArcOverlayController;

// Allows an exo::ShellSurface window to become an overlay on top of a different
// Aura window.
class ASH_PUBLIC_EXPORT ArcOverlayManager : public aura::EnvObserver,
                                            public aura::WindowObserver {
 public:
  // Returns the single ArcOverlayManager instance.
  static ArcOverlayManager* instance();

  ArcOverlayManager();
  ~ArcOverlayManager() override;

  // Disallow copying and moving.
  // Note: Moving an observer would require some work to deregister/re-register
  // it. If the instance is owned by a unique_ptr, the pointer can be moved much
  // more cheaply.
  ArcOverlayManager(const ArcOverlayManager&) = delete;
  ArcOverlayManager(ArcOverlayManager&&) = delete;
  ArcOverlayManager& operator=(const ArcOverlayManager&) = delete;
  ArcOverlayManager& operator=(ArcOverlayManager&&) = delete;

  virtual std::unique_ptr<ArcOverlayController> CreateController(
      aura::Window* host_window);

  // The host window must be registered before the overlay window.
  // The returned closure should be destroyed when the overlay window is
  // destroyed.
  base::ScopedClosureRunner RegisterHostWindow(std::string overlay_token,
                                               aura::Window* host_window);
  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

 private:
  void DeregisterHostWindow(const std::string& overlay_token);
  void RegisterOverlayWindow(std::string overlay_token,
                             exo::ShellSurfaceBase* shell_surface_base);

  base::flat_map<std::string, std::unique_ptr<ArcOverlayController>>
      token_to_controller_map_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  // This tracks newly created arc windows until they're being shown, or
  // destoryed.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_MANAGER_H_
