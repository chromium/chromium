// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_x11.h"

#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace extensions {

GlobalShortcutListenerX11::GlobalShortcutListenerX11() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListenerX11::~GlobalShortcutListenerX11() = default;

void GlobalShortcutListenerX11::StartListening() {
  DCHECK(!registered_hot_keys_.empty());
  XGlobalShortcutListener::StartListening();
}

void GlobalShortcutListenerX11::StopListening() {
  DCHECK(registered_hot_keys_.empty());
  XGlobalShortcutListener::StopListening();
}

bool GlobalShortcutListenerX11::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(registered_hot_keys_.find(accelerator) == registered_hot_keys_.end());

  const bool registered = XGlobalShortcutListener::RegisterAccelerator(
      accelerator.key_code(), accelerator.IsAltDown(), accelerator.IsCtrlDown(),
      accelerator.IsShiftDown());
  if (registered)
    registered_hot_keys_.insert(accelerator);

  return registered;
}

void GlobalShortcutListenerX11::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(registered_hot_keys_.find(accelerator) != registered_hot_keys_.end());

  XGlobalShortcutListener::UnregisterAccelerator(
      accelerator.key_code(), accelerator.IsAltDown(), accelerator.IsCtrlDown(),
      accelerator.IsShiftDown());

  registered_hot_keys_.erase(accelerator);
}

void GlobalShortcutListenerX11::OnKeyPressed(ui::KeyboardCode key_code,
                                             bool is_alt_down,
                                             bool is_ctrl_down,
                                             bool is_shift_down) {
  int modifiers = 0;
  if (is_alt_down)
    modifiers |= ui::EF_ALT_DOWN;
  if (is_ctrl_down)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (is_shift_down)
    modifiers |= ui::EF_SHIFT_DOWN;
  NotifyKeyPressed(ui::Accelerator(key_code, modifiers));
}

}  // namespace extensions
