// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_MAC_H_

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include <Carbon/Carbon.h>

#include <map>
#include <memory>

#include "ui/base/accelerators/media_keys_listener.h"

namespace extensions {

// Mac-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts. Handles basic keyboard intercepting and
// forwards its output to the base class for processing.
//
// This class does two things:
// 1. Intercepts media keys. Uses an event tap for intercepting media keys
// (PlayPause, NextTrack, PreviousTrack).
// 2. Binds keyboard shortcuts (hot keys). Carbon RegisterEventHotKey API for
// binding to non-media key global hot keys (eg. Command-Shift-1).
class GlobalShortcutListenerMac : public GlobalShortcutListener,
                                  public ui::MediaKeysListener::Delegate {
 public:
  GlobalShortcutListenerMac();

  GlobalShortcutListenerMac(const GlobalShortcutListenerMac&) = delete;
  GlobalShortcutListenerMac& operator=(const GlobalShortcutListenerMac&) =
      delete;

  ~GlobalShortcutListenerMac() override;

 private:
  using KeyId = int;
  using AcceleratorIdMap = std::map<ui::Accelerator, KeyId>;
  using IdAcceleratorMap = std::map<KeyId, ui::Accelerator>;
  using IdHotKeyRefMap = std::map<KeyId, EventHotKeyRef>;

  // Keyboard event callbacks.
  void OnHotKeyEvent(EventHotKeyID hot_key_id);

  // GlobalShortcutListener implementation.
  void StartListening() override;
  void StopListening() override;
  bool RegisterAcceleratorImpl(const ui::Accelerator& accelerator) override;
  void UnregisterAcceleratorImpl(const ui::Accelerator& accelerator) override;

  // ui::MediaKeysListener::Delegate:
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // Mac-specific functions for registering hot keys with modifiers.
  bool RegisterHotKey(const ui::Accelerator& accelerator, KeyId hot_key_id);
  void UnregisterHotKey(const ui::Accelerator& accelerator);

  // Enable and disable the hot key event handler.
  void StartWatchingHotKeys();
  void StopWatchingHotKeys();

  // Whether or not any hot keys are currently registered.
  bool IsAnyHotKeyRegistered();

  // The callback for when a hot key event happens.
  static OSStatus HotKeyHandler(
      EventHandlerCallRef next_handler, EventRef event, void* user_data);

  // Whether this object is listening for global shortcuts.
  bool is_listening_ = false;

  // The hotkey identifier for the next global shortcut that is added.
  KeyId hot_key_id_ = 0;

  // A map of all hotkeys (media keys and shortcuts) mapping to their
  // corresponding hotkey IDs. For quickly finding if an accelerator is
  // registered.
  AcceleratorIdMap accelerator_ids_;

  // The inverse map for quickly looking up accelerators by hotkey id.
  IdAcceleratorMap id_accelerators_;

  // Keyboard shortcut IDs to hotkeys map for unregistration.
  IdHotKeyRefMap id_hot_key_refs_;

  // Event handler for keyboard shortcut hot keys.
  EventHandlerRef event_handler_ = nullptr;

  // Media keys listener.
  std::unique_ptr<ui::MediaKeysListener> media_keys_listener_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_GLOBAL_SHORTCUT_LISTENER_MAC_H_
