// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/win/win_util.h"
#include "chrome/common/extensions/command.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_hot_key_observer.h"

using content::BrowserThread;

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static GlobalShortcutListenerWin* instance =
      new GlobalShortcutListenerWin();
  return instance;
}

GlobalShortcutListenerWin::GlobalShortcutListenerWin()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListenerWin::~GlobalShortcutListenerWin() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerWin::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  DCHECK(!hotkeys_.empty());  // Also don't start if no hotkey is registered.
  is_listening_ = true;
}

void GlobalShortcutListenerWin::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  DCHECK(hotkeys_.empty());  // Make sure the map is clean before ending.
  is_listening_ = false;
}

void GlobalShortcutListenerWin::OnWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  // SingletonHwndHotKeyObservers should only send us hot key messages.
  DCHECK_EQ(WM_HOTKEY, static_cast<int>(message));

  int key_code = HIWORD(lparam);
  int modifiers = 0;
  modifiers |= (LOWORD(lparam) & MOD_SHIFT) ? ui::EF_SHIFT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_ALT) ? ui::EF_ALT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_CONTROL) ? ui::EF_CONTROL_DOWN : 0;
  ui::Accelerator accelerator(
      ui::KeyboardCodeForWindowsKeyCode(key_code), modifiers);

  NotifyKeyPressed(accelerator);
}

bool GlobalShortcutListenerWin::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(hotkeys_.find(accelerator) == hotkeys_.end());

  // TODO(https://crbug.com/950704): We should be using
  // |media_keys_listener_manager->StartWatchingMediaKey(...)| here, but that
  // currently breaks the GlobalCommandsApiTest.GlobalDuplicatedMediaKey test.
  // Instead, we'll just disable the MediaKeysListenerManager handling here, and
  // listen using the fallback RegisterHotKey method.
  if (content::MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled() &&
      Command::IsMediaKey(accelerator)) {
    content::MediaKeysListenerManager* media_keys_listener_manager =
        content::MediaKeysListenerManager::GetInstance();
    DCHECK(media_keys_listener_manager);

    registered_media_keys_++;
    media_keys_listener_manager->DisableInternalMediaKeyHandling();
  }

  // Convert Accelerator modifiers to OS modifiers.
  int modifiers = 0;
  modifiers |= accelerator.IsShiftDown() ? MOD_SHIFT : 0;
  modifiers |= accelerator.IsCtrlDown() ? MOD_CONTROL : 0;
  modifiers |= accelerator.IsAltDown() ? MOD_ALT : 0;

  // Create an observer that registers a hot key for |accelerator|.
  std::unique_ptr<gfx::SingletonHwndHotKeyObserver> observer =
      gfx::SingletonHwndHotKeyObserver::Create(
          base::BindRepeating(&GlobalShortcutListenerWin::OnWndProc,
                              base::Unretained(this)),
          accelerator.key_code(), modifiers);

  if (!observer) {
    // Most likely error: 1409 (Hotkey already registered).
    return false;
  }

  hotkeys_[accelerator] = std::move(observer);
  return true;
}

void GlobalShortcutListenerWin::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  HotKeyMap::iterator it = hotkeys_.find(accelerator);
  DCHECK(it != hotkeys_.end());

  // TODO(https://crbug.com/950704): We should be using
  // |media_keys_listener_manager->StopWatchingMediaKey(...)| here.
  if (content::MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled() &&
      Command::IsMediaKey(accelerator)) {
    registered_media_keys_--;
    DCHECK_GE(registered_media_keys_, 0);
    if (registered_media_keys_ == 0) {
      content::MediaKeysListenerManager* media_keys_listener_manager =
          content::MediaKeysListenerManager::GetInstance();
      DCHECK(media_keys_listener_manager);

      media_keys_listener_manager->EnableInternalMediaKeyHandling();
    }
  }

  hotkeys_.erase(it);
}

void GlobalShortcutListenerWin::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  // We should not receive media key events that we didn't register for.
  DCHECK(hotkeys_.find(accelerator) != hotkeys_.end());
  NotifyKeyPressed(accelerator);
}

}  // namespace extensions
