// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class TouchModeMouseRewriter;

// Manager for ARC resize lock feature.
class ArcResizeLockManager : public KeyedService,
                             public aura::EnvObserver,
                             public aura::WindowObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcResizeLockManager* GetForBrowserContext(
      content::BrowserContext* context);

  ArcResizeLockManager(content::BrowserContext* browser_context,
                       ArcBridgeService* arc_bridge_service);
  ArcResizeLockManager(const ArcResizeLockManager&) = delete;
  ArcResizeLockManager& operator=(const ArcResizeLockManager&) = delete;
  ~ArcResizeLockManager() override;

  CompatModeButtonController* compat_mode_button_controller() {
    return compat_mode_button_controller_.get();
  }

  // KeyedService:
  void Shutdown() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Sets `pref_delegate_` to `delegate`, ensuring that it was not already set.
  // Also, calls `compat_mode_button_controller_->SetPrefDelegate()` with
  // `delegate`.
  void SetPrefDelegate(ArcResizeLockPrefDelegate* delegate);

  static void EnsureFactoryBuilt();

 private:
  friend class ArcResizeLockManagerTest;

  void EnableResizeLock(aura::Window* window);
  void DisableResizeLock(aura::Window* window);
  void UpdateResizeLockState(aura::Window* window);
  void UpdateShadow(aura::Window* window);

  // virtual for unittest.
  virtual void ShowSplashScreenDialog(aura::Window* window,
                                      bool is_fully_locked);

  raw_ptr<ArcResizeLockPrefDelegate> pref_delegate_{nullptr};

  // Using unique_ptr to allow unittest to override.
  std::unique_ptr<CompatModeButtonController> compat_mode_button_controller_;

  std::unique_ptr<TouchModeMouseRewriter> touch_mode_mouse_rewriter_;

  base::flat_set<raw_ptr<aura::Window, CtnExperimental>>
      resize_lock_enabled_windows_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  base::WeakPtrFactory<ArcResizeLockManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_RESIZE_LOCK_MANAGER_H_
