// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_

#include "ash/components/arc/ime/arc_ime_bridge.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/key_event_source_rewriter.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method.h"

namespace content {
class BrowserContext;
}

namespace arc {

class ArcBridgeService;

// Manager for ARC input overlay feature which improves input compatibility
// for touch-only apps.
class ArcInputOverlayManager : public KeyedService,
                               public aura::EnvObserver,
                               public aura::WindowObserver,
                               public aura::client::FocusChangeObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcInputOverlayManager* GetForBrowserContext(
      content::BrowserContext* context);
  ArcInputOverlayManager(content::BrowserContext* browser_context,
                         ArcBridgeService* arc_bridge_service);
  ArcInputOverlayManager(const ArcInputOverlayManager&) = delete;
  ArcInputOverlayManager& operator=(const ArcInputOverlayManager&) = delete;
  ~ArcInputOverlayManager() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // KeyedService:
  void Shutdown() override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  friend class ArcInputOverlayManagerTest;

  class InputMethodObserver;

  // TODO(djacobo|cuicuiruan): Sort this, functions first, members last.
  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  base::flat_map<aura::Window*, std::unique_ptr<input_overlay::TouchInjector>>
      input_overlay_enabled_windows_;
  bool is_text_input_active_ = false;
  raw_ptr<ui::InputMethod> input_method_ = nullptr;
  std::unique_ptr<InputMethodObserver> input_method_observer_;
  // Only one window is registered since there is only one window can be focused
  // each time.
  raw_ptr<aura::Window> registered_top_level_window_ = nullptr;
  std::unique_ptr<KeyEventSourceRewriter> key_event_source_rewriter_;
  std::unique_ptr<input_overlay::DisplayOverlayController>
      display_overlay_controller_;

  void ReadData(const std::string& package_name,
                aura::Window* top_level_window);
  void NotifyTextInputState();
  void AddObserverToInputMethod();
  void RemoveObserverFromInputMethod();
  // Only top level window will be registered successfully.
  void RegisterWindow(aura::Window* window);
  void UnRegisterWindow(aura::Window* window);
  void AddDisplayOverlayController();
  void RemoveDisplayOverlayController();

  base::WeakPtrFactory<ArcInputOverlayManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
