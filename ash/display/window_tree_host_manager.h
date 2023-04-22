// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_WINDOW_TREE_HOST_MANAGER_H_
#define ASH_DISPLAY_WINDOW_TREE_HOST_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host_delegate.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Insets;
}

namespace ash {
class AshWindowTreeHost;
struct AshWindowTreeHostInitParams;
class CursorWindowController;
class FocusActivationStore;
class MirrorWindowController;
class RootWindowController;
class RoundedDisplayProvider;

// WindowTreeHostManager owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
class ASH_EXPORT WindowTreeHostManager
    : public display::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public display::ContentProtectionManager::Observer,
      public display::DisplayManager::Delegate,
      public ui::ImeKeyEventDispatcher,
      public AshWindowTreeHostDelegate {
 public:
  // TODO(oshima): Consider moving this to display::DisplayObserver.
  class ASH_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Invoked only once after all displays are initialized
    // after startup.
    virtual void OnDisplaysInitialized() {}

    // Invoked when the display configuration change is requested,
    // but before the change is applied to aura/ash.
    virtual void OnDisplayConfigurationChanging() {}

    // Invoked when the all display configuration changes
    // have been applied.
    virtual void OnDisplayConfigurationChanged() {}

    // Invoked in WindowTreeHostManager::Shutdown().
    virtual void OnWindowTreeHostManagerShutdown() {}
  };

  WindowTreeHostManager();

  WindowTreeHostManager(const WindowTreeHostManager&) = delete;
  WindowTreeHostManager& operator=(const WindowTreeHostManager&) = delete;

  ~WindowTreeHostManager() override;

  void Start();
  void Shutdown();

  // Returns primary display's ID.
  // TODO(oshima): Move this out from WindowTreeHostManager;
  static int64_t GetPrimaryDisplayId();

  // Returns true if the current primary display ID is valid.
  static bool HasValidPrimaryDisplayId();

  CursorWindowController* cursor_window_controller() {
    return cursor_window_controller_.get();
  }

  MirrorWindowController* mirror_window_controller() {
    return mirror_window_controller_.get();
  }

  // Create a WindowTreeHost for the primary display. This replaces
  // |initial_bounds| in |init_params|.
  void CreatePrimaryHost(const AshWindowTreeHostInitParams& init_params);

  // Initializes all WindowTreeHosts.
  void InitHosts();

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the root window for primary display.
  aura::Window* GetPrimaryRootWindow();

  // Returns the root window for |display_id|.
  aura::Window* GetRootWindowForDisplayId(int64_t id);

  // Returns AshWTH for given display |id|. Returns nullptr if the WTH does not
  // exist.
  AshWindowTreeHost* GetAshWindowTreeHostForDisplayId(int64_t id);

  // Returns all root windows. In non extended desktop mode, this
  // returns the primary root window only.
  aura::Window::Windows GetAllRootWindows();

  // Returns all root window controllers. In non extended desktop
  // mode, this return a RootWindowController for the primary root window only.
  std::vector<RootWindowController*> GetAllRootWindowControllers();

  // Gets/Sets/Clears the overscan insets for the specified |display_id|. See
  // display_manager.h for the details.
  gfx::Insets GetOverscanInsets(int64_t display_id) const;
  void SetOverscanInsets(int64_t display_id, const gfx::Insets& insets_in_dip);

  // Checks if the mouse pointer is on one of displays, and moves to
  // the center of the nearest display if it's outside of all displays.
  void UpdateMouseLocationAfterDisplayChange();

  // Sets the work area's |insets| to the display assigned to |window|.
  bool UpdateWorkAreaOfDisplayNearestWindow(const aura::Window* window,
                                            const gfx::Insets& insets);

  ui::InputMethod* input_method() { return input_method_.get(); }

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // Enables the rounded corners mask texture for a display. It creates
  // `RoundedDisplayProvider` for a display as needed and updates the surface if
  // required.
  void EnableRoundedCorners(const display::Display& display);

  // Updates the rounded corners masks textures on the display by submitting a
  // compositor frame if needed.
  void MaybeUpdateRoundedDisplaySurface(const display::Display& display);

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(aura::WindowTreeHost* host) override;

  // display::ContentProtectionManager::Observer overrides:
  void OnDisplaySecurityChanged(int64_t display_id, bool secure) override;

  // display::DisplayManager::Delegate overrides:
  void CreateOrUpdateMirroringDisplay(
      const display::DisplayInfoList& info_list) override;
  void CloseMirroringDisplayIfNotNecessary() override;
  void SetPrimaryDisplayId(int64_t id) override;
  void PreDisplayConfigurationChange(bool clear_focus) override;
  void PostDisplayConfigurationChange() override;

  // ui::ImeKeyEventDispatcher overrides:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override;

  // ash::AshWindowTreeHostDelegate overrides:
  const display::Display* GetDisplayById(int64_t display_id) const override;
  void SetCurrentEventTargeterSourceHost(
      aura::WindowTreeHost* targeter_src_host) override;

  // Get the rounded display provider for a display.
  RoundedDisplayProvider* GetRoundedDisplayProvider(int64_t display_id);

  // Deletes the RoundedDisplayProviders for displays with rounded-corners.
  // Needs to be called before `Shell::CloseAllRootWindowChildWindows()` since
  // we need host_windows for proper deletion of the providers.
  void ShutdownRoundedDisplays();

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowTreeHostManagerTest, BoundsUpdated);
  FRIEND_TEST_ALL_PREFIXES(WindowTreeHostManagerTest, SecondaryDisplayLayout);
  friend class MirrorWindowController;

  // Creates a WindowTreeHost for |display| and stores it in the
  // |window_tree_hosts_| map.
  AshWindowTreeHost* AddWindowTreeHostForDisplay(
      const display::Display& display,
      const AshWindowTreeHostInitParams& params);

  // Delete the AsWindowTreeHost. This does not remove the entry from
  // |window_tree_hosts_|. Caller has to explicitly remove it.
  void DeleteHost(AshWindowTreeHost* host_to_delete);

  // Create RoundedDisplayProvider for the display if needed.
  void AddRoundedDisplayProviderIfNeeded(const display::Display& display);
  void RemoveRoundedDisplayProvider(const display::Display& display);

  typedef std::map<int64_t, AshWindowTreeHost*> WindowTreeHostMap;
  // The mapping from display ID to its window tree host.
  WindowTreeHostMap window_tree_hosts_;

  // The mapping from display ID to its rounded display provider.
  base::flat_map<int64_t, std::unique_ptr<RoundedDisplayProvider>>
      rounded_display_providers_map_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  // Store the primary window tree host temporarily while replacing
  // display.
  raw_ptr<AshWindowTreeHost, ExperimentalAsh> primary_tree_host_for_replace_;

  std::unique_ptr<FocusActivationStore> focus_activation_store_;

  std::unique_ptr<CursorWindowController> cursor_window_controller_;
  std::unique_ptr<MirrorWindowController> mirror_window_controller_;

  std::unique_ptr<ui::InputMethod> input_method_;

  // Stores the current cursor location (in native coordinates and screen
  // coordinates respectively). The locations are used to restore the cursor
  // location when the display configuration changes and to determine whether
  // the mouse should be moved after a display configuration change.
  gfx::Point cursor_location_in_native_coords_for_restore_;
  gfx::Point cursor_location_in_screen_coords_for_restore_;

  // Stores the cursor's display. The id is used to determine whether the mouse
  // should be moved after a display configuration change.
  int64_t cursor_display_id_for_restore_;

  // Receive DisplayObserver callbacks between Start and Shutdown.
  absl::optional<display::ScopedDisplayObserver> display_observer_;

  // A repeating timer to trigger sending UMA metrics for primary display's
  // effective resolution at fixed intervals.
  std::unique_ptr<base::RepeatingTimer> effective_resolution_UMA_timer_;

  base::WeakPtrFactory<WindowTreeHostManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_WINDOW_TREE_HOST_MANAGER_H_
