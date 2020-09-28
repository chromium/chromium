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
#include "ui/gfx/x/xproto.h"

using content::BrowserThread;

namespace {

// The modifiers masks used for grabing keys. Due to XGrabKey only working on
// exact modifiers, we need to grab all key combination including zero or more
// of the following: Num lock, Caps lock and Scroll lock. So that we can make
// sure the behavior of global shortcuts is consistent on all platforms.
const x11::ModMask kModifiersMasks[] = {
    {},                  // No additional modifier.
    x11::ModMask::c_2,   // Num lock
    x11::ModMask::Lock,  // Caps lock
    x11::ModMask::c_5,   // Scroll lock
    x11::ModMask::c_2 | x11::ModMask::Lock,
    x11::ModMask::c_2 | x11::ModMask::c_5,
    x11::ModMask::Lock | x11::ModMask::c_5,
    x11::ModMask::c_2 | x11::ModMask::Lock | x11::ModMask::c_5};

x11::ModMask GetNativeModifiers(const ui::Accelerator& accelerator) {
  constexpr auto kNoMods = x11::ModMask{};
  return (accelerator.IsShiftDown() ? x11::ModMask::Shift : kNoMods) |
         (accelerator.IsCtrlDown() ? x11::ModMask::Control : kNoMods) |
         (accelerator.IsAltDown() ? x11::ModMask::c_1 : kNoMods);
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
      connection_(x11::Connection::Get()),
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

  auto modifiers = GetNativeModifiers(accelerator);
  auto keysym = XKeysymForWindowsKeyCode(accelerator.key_code(), false);
  auto keycode = connection_->KeysymToKeycode(static_cast<x11::KeySym>(keysym));
  gfx::X11ErrorTracker err_tracker;

  // Because XGrabKey only works on the exact modifiers mask, we should register
  // our hot keys with modifiers that we want to ignore, including Num lock,
  // Caps lock, Scroll lock. See comment about |kModifiersMasks|.
  for (auto mask : kModifiersMasks) {
    connection_->GrabKey({false, x_root_window_, modifiers | mask, keycode,
                          x11::GrabMode::Async, x11::GrabMode::Async});
  }

  if (err_tracker.FoundNewError()) {
    // We may have part of the hotkeys registered, clean up.
    for (auto mask : kModifiersMasks)
      connection_->UngrabKey({keycode, x_root_window_, modifiers | mask});

    return false;
  }

  registered_hot_keys_.insert(accelerator);
  return true;
}

void GlobalShortcutListenerX11::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(registered_hot_keys_.find(accelerator) != registered_hot_keys_.end());

  auto modifiers = GetNativeModifiers(accelerator);
  auto keysym = XKeysymForWindowsKeyCode(accelerator.key_code(), false);
  auto keycode = connection_->KeysymToKeycode(static_cast<x11::KeySym>(keysym));

  for (auto mask : kModifiersMasks)
    connection_->UngrabKey({keycode, x_root_window_, modifiers | mask});

  registered_hot_keys_.erase(accelerator);
}

void GlobalShortcutListenerX11::OnKeyPressEvent(const ui::KeyEvent& event) {
  DCHECK_EQ(event.type(), ui::ET_KEY_PRESSED);
  ui::Accelerator accelerator(event.key_code(), event.flags());
  if (registered_hot_keys_.find(accelerator) != registered_hot_keys_.end())
    NotifyKeyPressed(accelerator);
}

}  // namespace extensions
