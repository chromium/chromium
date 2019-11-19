// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_WIN_H_

#include <windows.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace gfx {

class SingletonHwndHotKeyObserver;

}  // namespace gfx

namespace extensions {

// Windows-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles setting up a keyboard hook and
// forwarding its output to the base class for processing.
class GlobalShortcutListenerWin : public GlobalShortcutListener,
                                  public ui::MediaKeysListener::Delegate {
 public:
  GlobalShortcutListenerWin();
  ~GlobalShortcutListenerWin() override;

 private:
  // The implementation of our Window Proc, called by SingletonHwndObserver.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // GlobalShortcutListener implementation.
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;

  // ui::MediaKeysListener::Delegate implementation.
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // Whether this object is listening for global shortcuts.
  bool is_listening_;

  // The number of media keys currently registered.
  int registered_media_keys_ = 0;

  // A map of registered accelerators and their registration ids. The value is
  // null for media keys if kHardwareMediaKeyHandling is true.
  using HotKeyMap = std::map<ui::Accelerator,
                             std::unique_ptr<gfx::SingletonHwndHotKeyObserver>>;
  HotKeyMap hotkeys_;

  DISALLOW_COPY_AND_ASSIGN(GlobalShortcutListenerWin);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_WIN_H_
