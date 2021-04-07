// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_X11_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_X11_H_

#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "ui/base/x/x11_global_shortcut_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace extensions {

// X11-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles basic keyboard intercepting and
// forwards its output to the base class for processing.
class GlobalShortcutListenerX11 : public GlobalShortcutListener,
                                  public ui::XGlobalShortcutListener {
 public:
  GlobalShortcutListenerX11();
  GlobalShortcutListenerX11(const GlobalShortcutListenerX11&) = delete;
  GlobalShortcutListenerX11& operator=(const GlobalShortcutListenerX11&) =
      delete;
  ~GlobalShortcutListenerX11() override;

 private:
  // GlobalShortcutListener implementation.
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;

  // ui::XGlobalShortcutListener:
  void OnKeyPressed(ui::KeyboardCode key_code,
                    bool is_alt_down,
                    bool is_ctrl_down,
                    bool is_shift_down) override;

  std::set<ui::Accelerator> registered_hot_keys_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_X11_H_
