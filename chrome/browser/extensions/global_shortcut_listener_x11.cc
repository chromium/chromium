// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_x11.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

using content::BrowserThread;

namespace {

// The modifiers masks used for grabing keys. Due to XGrabKey only working on
// exact modifiers, we need to grab all key combination including zero or more
// of the following: Num lock, Caps lock and Scroll lock. So that we can make
// sure the behavior of global shortcuts is consistent on all platforms.
const unsigned int kModifiersMasks[] = {0,         // No additional modifier.
                                        Mod2Mask,  // Num lock
                                        LockMask,  // Caps lock
                                        Mod5Mask,  // Scroll lock
                                        Mod2Mask | LockMask,
                                        Mod2Mask | Mod5Mask,
                                        LockMask | Mod5Mask,
                                        Mod2Mask | LockMask | Mod5Mask};

int GetNativeModifiers(const ui::Accelerator& accelerator) {
  int modifiers = 0;
  modifiers |= accelerator.IsShiftDown() ? ShiftMask : 0;
  modifiers |= accelerator.IsCtrlDown() ? ControlMask : 0;
  modifiers |= accelerator.IsAltDown() ? Mod1Mask : 0;

  return modifiers;
}

}  // namespace

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static GlobalShortcutListenerX11* instance = new GlobalShortcutListenerX11();
  return instance;
}

GlobalShortcutListenerX11::GlobalShortcutListenerX11()
    : is_listening_(false),
      x_display_(gfx::GetXDisplay()),
      x_root_window_(ui::GetX11RootWindow()) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListenerX11::~GlobalShortcutListenerX11() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerX11::StartListening() {
  DCHECK(!is_listening_);                 // Don't start twice.
  DCHECK(!registered_hot_keys_.empty());  // Also don't start if no hotkey is
                                          // registered.

  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

  is_listening_ = true;
}

void GlobalShortcutListenerX11::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  DCHECK(registered_hot_keys_.empty());  // Make sure the set is clean before
                                         // ending.

  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  is_listening_ = false;
}

bool GlobalShortcutListenerX11::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return event->type() == ui::ET_KEY_PRESSED;
}

uint32_t GlobalShortcutListenerX11::DispatchEvent(
    const ui::PlatformEvent& event) {
  CHECK_EQ(event->type(), ui::ET_KEY_PRESSED);
  OnKeyPressEvent(*event->AsKeyEvent());
  return ui::POST_DISPATCH_NONE;
}

bool GlobalShortcutListenerX11::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(registered_hot_keys_.find(accelerator) == registered_hot_keys_.end());

  int modifiers = GetNativeModifiers(accelerator);
  KeyCode keycode = XKeysymToKeycode(
      x_display_, XKeysymForWindowsKeyCode(accelerator.key_code(), false));
  gfx::X11ErrorTracker err_tracker;

  // Because XGrabKey only works on the exact modifiers mask, we should register
  // our hot keys with modifiers that we want to ignore, including Num lock,
  // Caps lock, Scroll lock. See comment about |kModifiersMasks|.
  for (unsigned int kModifiersMask : kModifiersMasks) {
    XGrabKey(x_display_, keycode, modifiers | kModifiersMask,
             static_cast<uint32_t>(x_root_window_), false, GrabModeAsync,
             GrabModeAsync);
  }

  if (err_tracker.FoundNewError()) {
    // We may have part of the hotkeys registered, clean up.
    for (unsigned int kModifiersMask : kModifiersMasks) {
      XUngrabKey(x_display_, keycode, modifiers | kModifiersMask,
                 static_cast<uint32_t>(x_root_window_));
    }

    return false;
  }

  registered_hot_keys_.insert(accelerator);
  return true;
}

void GlobalShortcutListenerX11::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(registered_hot_keys_.find(accelerator) != registered_hot_keys_.end());

  int modifiers = GetNativeModifiers(accelerator);
  KeyCode keycode = XKeysymToKeycode(
      x_display_, XKeysymForWindowsKeyCode(accelerator.key_code(), false));

  for (unsigned int kModifiersMask : kModifiersMasks) {
    XUngrabKey(x_display_, keycode, modifiers | kModifiersMask,
               static_cast<uint32_t>(x_root_window_));
  }
  registered_hot_keys_.erase(accelerator);
}

void GlobalShortcutListenerX11::OnKeyPressEvent(const ui::KeyEvent& event) {
  DCHECK_EQ(event.type(), ui::ET_KEY_PRESSED);
  ui::Accelerator accelerator(event.key_code(), event.flags());
  if (registered_hot_keys_.find(accelerator) != registered_hot_keys_.end())
    NotifyKeyPressed(accelerator);
}

}  // namespace extensions
