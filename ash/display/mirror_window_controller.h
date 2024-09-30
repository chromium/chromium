// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host_delegate.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace aura {
class Window;
namespace client {
class ScreenPositionClient;
}
}

namespace display {
class Display;
class ManagedDisplayInfo;
}

namespace ash {
class AshWindowTreeHost;
class MirrorWindowTestApi;

// An object that copies the content of the primary root window to a
// mirror window. This also draws a mouse cursor as the mouse cursor
// is typically drawn by the window system.
class ASH_EXPORT MirrorWindowController : public aura::WindowTreeHostObserver,
                                          public AshWindowTreeHostDelegate {
 public:
  MirrorWindowController();

  MirrorWindowController(const MirrorWindowController&) = delete;
  MirrorWindowController& operator=(const MirrorWindowController&) = delete;

  ~MirrorWindowController() override;

  // Updates the root window's bounds using |display_info|.
  // Creates the new root window if one doesn't exist.
  void UpdateWindow(
      const std::vector<display::ManagedDisplayInfo>& display_info);

  // Same as above, but using existing display info
  // for the mirrored display.
  void UpdateWindow();

  // Close the mirror window if they're not necessary any longer.
  void CloseIfNotNecessary();

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(aura::WindowTreeHost* host) override;

  // Returns the display::Display for the mirroring root window.
  display::Display GetDisplayForRootWindow(const aura::Window* root) const;

  // Returns the AshWindwoTreeHost created for |display_id|.
  AshWindowTreeHost* GetAshWindowTreeHostForDisplayId(int64_t display_id);

  // Returns all root windows hosting mirroring displays.
  aura::Window::Windows GetAllRootWindows() const;

  // AshWindowTreeHostDelegate:
  const display::Display* GetDisplayById(int64_t display_id) const override;
  void SetCurrentEventTargeterSourceHost(
      aura::WindowTreeHost* targeter_src_host) override;

  const aura::WindowTreeHost* current_event_targeter_src_host() const {
    return current_event_targeter_src_host_;
  }

 private:
  friend class MirrorWindowTestApi;

  struct MirroringHostInfo;

  // Close the mirror window. When |delay_host_deletion| is true, the window
  // tree host will be deleted in an another task on UI thread. This is
  // necessary to safely delete the WTH that is currently handling input events.
  void Close(bool delay_host_deletion);

  void CloseAndDeleteHost(MirroringHostInfo* host_info,
                          bool delay_host_deletion);

  typedef std::map<int64_t, raw_ptr<MirroringHostInfo, CtnExperimental>>
      MirroringHostInfoMap;
  MirroringHostInfoMap mirroring_host_info_map_;

  raw_ptr<aura::WindowTreeHost, DanglingUntriaged>
      current_event_targeter_src_host_;

  display::DisplayManager::MultiDisplayMode multi_display_mode_;

  // The id of the display being mirrored.
  int64_t reflecting_source_id_ = display::kInvalidDisplayId;

  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
