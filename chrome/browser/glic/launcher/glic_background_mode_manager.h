// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

class GlicController;
class GlicStatusIcon;
class ScopedKeepAlive;
class StatusTray;

namespace ui {
class Accelerator;
}

namespace glic {

// This is a global feature in the browser process that manages the
// enabling/disabling of glic background mode. When background mode is enabled,
// chrome is set to keep alive the browser process, so that this class can
// listen to a global hotkey, and provide a status icon for triggering the UI.
class GlicBackgroundModeManager
    : public GlicLauncherConfiguration::Observer,
      public ui::GlobalAcceleratorListener::Observer {
 public:
  explicit GlicBackgroundModeManager(StatusTray* status_tray);
  ~GlicBackgroundModeManager() override;

  // GlicConfiguration::Observer
  void OnEnabledChanged(bool enabled) override;
  void OnGlobalHotkeyChanged(ui::Accelerator hotkey) override;

  // ui::GlobalAcceleratorListener::Observer
  void OnKeyPressed(const ui::Accelerator& accelerator) override;

  ui::Accelerator RegisteredHotkeyForTesting() {
    return actual_registered_hotkey_;
  }

 private:
  void EnterBackgroundMode();
  void ExitBackgroundMode();
  void EnableLaunchOnStartup(bool should_launch);
  void RegisterHotkey(ui::Accelerator updated_hotkey);
  void UnregisterHotkey();
  void UpdateState();

  // A helper class for observing pref changes.
  std::unique_ptr<GlicLauncherConfiguration> configuration_;

  // An abstraction used to show/hide the UI.
  std::unique_ptr<GlicController> controller_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // TODO(https://crbug.com/378139555): Figure out how to not dangle this
  // pointer (and other instances of StatusTray).
  raw_ptr<StatusTray, DanglingUntriaged> status_tray_;
  // Class that represents the glic status icon. Only exists when the background
  // mode is enabled.
  std::unique_ptr<GlicStatusIcon> status_icon_;

  bool enabled_ = false;
  // The actual registered hotkey may be different from the expected hotkey
  // because the Glic launcher may be disabled or registration fails which
  // results in no hotkey being registered and is represented with an empty
  // accelerator.
  ui::Accelerator expected_registered_hotkey_;
  ui::Accelerator actual_registered_hotkey_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
