// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/ozone/public/platform_global_shortcut_listener.h"

namespace ui {
class Accelerator;
}  // namespace ui

namespace extensions {

// Ozone-specific implementation of the GlobalShortcutListener interface.
//
// Connects Aura with the platform implementation, and manages data conversions
// required on the way: Aura operates with ui::Accelerator while the platform is
// only aware of the basic components such as the key code and modifiers.
class GlobalShortcutListenerOzone
    : public GlobalShortcutListener,
      public ui::PlatformGlobalShortcutListenerDelegate {
 public:
  GlobalShortcutListenerOzone();

  GlobalShortcutListenerOzone(const GlobalShortcutListenerOzone&) = delete;
  GlobalShortcutListenerOzone& operator=(const GlobalShortcutListenerOzone&) =
      delete;

  ~GlobalShortcutListenerOzone() override;

 private:
  // GlobalShortcutListener:
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;

  // ui::PlatformGlobalShortcutListenerDelegate:
  void OnKeyPressed(ui::KeyboardCode key_code,
                    bool is_alt_down,
                    bool is_ctrl_down,
                    bool is_shift_down) override;
  void OnPlatformListenerDestroyed() override;

  bool is_listening_ = false;
  std::set<ui::Accelerator> registered_hot_keys_;

  // The platform implementation.
  raw_ptr<ui::PlatformGlobalShortcutListener>
      platform_global_shortcut_listener_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_OZONE_H_
