// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/arc/input_overlay/db/data_controller.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/key_event_source_rewriter.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

class ArcAppListPrefs;

namespace arc {
class ArcBridgeService;
}  // namespace arc

namespace content {
class BrowserContext;
}  // namespace content

namespace display {
enum class TabletState;
}  // namespace display

namespace ui {
class InputMethod;
}  // namespace ui

namespace arc::input_overlay {

// Manager for ARC input overlay feature which improves input compatibility
// for touch-only apps.
class ArcInputOverlayManager : public KeyedService,
                               public aura::EnvObserver,
                               public aura::WindowObserver,
                               public aura::client::FocusChangeObserver,
                               public display::DisplayObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser `context` is not allowed to use ARC.
  static ArcInputOverlayManager* GetForBrowserContext(
      content::BrowserContext* context);
  ArcInputOverlayManager(content::BrowserContext* browser_context,
                         ::arc::ArcBridgeService* arc_bridge_service);
  ArcInputOverlayManager(const ArcInputOverlayManager&) = delete;
  ArcInputOverlayManager& operator=(const ArcInputOverlayManager&) = delete;
  ~ArcInputOverlayManager() override;

  static void EnsureFactoryBuilt();

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
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

  // KeyedService:
  void Shutdown() override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  friend class ArcInputOverlayManagerTest;
  friend class GameControlsTestBase;
  friend class TestArcInputOverlayManager;

  class InputMethodObserver;

  // Remove `window` from observation list.
  void RemoveWindowObservation(aura::Window* window);
  void UnregisterAndRemoveObservation(aura::Window* window);
  // Read default data.
  static std::unique_ptr<TouchInjector> ReadDefaultData(
      std::unique_ptr<TouchInjector> touch_injector);
  // Called when finishing reading default data.
  void OnFinishReadDefaultData(std::unique_ptr<TouchInjector> touch_injector);
  // Apply the customized proto data.
  void OnProtoDataAvailable(std::unique_ptr<TouchInjector> touch_injector,
                            std::unique_ptr<AppDataProto> proto);
  // Callback function triggered by Save button.
  void OnSaveProtoFile(std::unique_ptr<AppDataProto> proto,
                       std::string package_name);

  // Returns true if the app has Game Controls opt-out metadata set to true.
  bool IsGameControlsOptOut(const std::string& package_name);
  // Checks app category.
  void CheckAppCategory(std::unique_ptr<TouchInjector> touch_injector);
  // Called after getting app category.
  void OnDidCheckAppCategory(std::unique_ptr<TouchInjector> touch_injector,
                             arc::mojom::AppCategory app_category);
  // Checks if it is O4C app.
  void CheckO4C(std::unique_ptr<TouchInjector> touch_injector);

  void NotifyTextInputState();
  void AddObserverToInputMethod();
  void RemoveObserverFromInputMethod();
  // Only top level window will be registered/unregistered successfully.
  void RegisterWindow(aura::Window* window);
  void UnRegisterWindow(aura::Window* window);
  void RegisterFocusedWindow();
  // Add display overlay controller related to `touch_injector`. Virtual for
  // test.
  virtual void AddDisplayOverlayController(TouchInjector* touch_injector);
  void RemoveDisplayOverlayController();
  // Reset for removing pending `touch_injector` because of no GIO data or
  // window destroying.
  void ResetForPendingTouchInjector(
      std::unique_ptr<TouchInjector> touch_injector);
  // Called when data loading finished from files or mojom calls for
  // `touch_injector`. `is_o4c` is true if the game is optimized for ChromeOS.
  void OnLoadingFinished(std::unique_ptr<TouchInjector> touch_injector,
                         bool is_o4c = false);
  // Once there is an error when checking Android side, reset `TouchInjector` if
  // it has empty actions. Otherwise, finish data loading. This is called for
  // mojom connection error. Once the mojom connection failed, it considers GIO
  // is available if there is mapping data.
  void MayKeepTouchInjectorAfterError(
      std::unique_ptr<TouchInjector> touch_injector);

  ArcAppListPrefs* GetArcAppListPrefs();

  // Returns `window`'s anchor window if `window` is a game dashboard main menu
  // dialog window or `ash::AnchoredNudge` or a transient window, or returns
  // `window` itself if `window` is none of above windows. For Alpha/AlphaV2
  // version, it still returns `window` itself.
  aura::Window* GetAnchorWindow(aura::Window* window);

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  base::flat_map<aura::Window*, std::unique_ptr<TouchInjector>>
      input_overlay_enabled_windows_;
  display::ScopedDisplayObserver display_observer_{this};

  // To avoid UAF issue reported in crbug.com/1363030. Save the windows which
  // prepare or start loading the GIO default key mapping data. Once window is
  // destroying or the GIO data reading is finished, window is removed from this
  // set.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>> loading_data_windows_;
  bool is_text_input_active_ = false;
  raw_ptr<ui::InputMethod> input_method_ = nullptr;
  std::unique_ptr<InputMethodObserver> input_method_observer_;
  // Only one window is registered since there is only one window can be focused
  // each time.
  raw_ptr<aura::Window> registered_top_level_window_ = nullptr;
  std::unique_ptr<KeyEventSourceRewriter> key_event_source_rewriter_;
  std::unique_ptr<DisplayOverlayController> display_overlay_controller_;
  std::unique_ptr<DataController> data_controller_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ArcInputOverlayManager> weak_ptr_factory_{this};
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
