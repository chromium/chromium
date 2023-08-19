// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <IOKit/hidsystem/ev_keymap.h>

#import "base/apple/foundation_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "extensions/common/command.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

using content::BrowserThread;
using extensions::GlobalShortcutListenerMac;

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static GlobalShortcutListenerMac* instance =
      new GlobalShortcutListenerMac();
  return instance;
}

GlobalShortcutListenerMac::GlobalShortcutListenerMac() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the MediaKeysListenerManager is not enabled, we need to create our own
  // global MediaKeysListener to receive media keys.
  if (!content::MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled()) {
    media_keys_listener_ = ui::MediaKeysListener::Create(
        this, ui::MediaKeysListener::Scope::kGlobal);
    DCHECK(media_keys_listener_);
  }
}

GlobalShortcutListenerMac::~GlobalShortcutListenerMac() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // By this point, UnregisterAccelerator should have been called for all
  // keyboard shortcuts. Still we should clean up.
  if (is_listening_)
    StopListening();

  if (IsAnyHotKeyRegistered())
    StopWatchingHotKeys();
}

void GlobalShortcutListenerMac::StartListening() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!accelerator_ids_.empty());
  DCHECK(!id_accelerators_.empty());
  DCHECK(!is_listening_);

  is_listening_ = true;
}

void GlobalShortcutListenerMac::StopListening() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(accelerator_ids_.empty());  // Make sure the set is clean.
  DCHECK(id_accelerators_.empty());
  DCHECK(is_listening_);

  is_listening_ = false;
}

void GlobalShortcutListenerMac::OnHotKeyEvent(EventHotKeyID hot_key_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This hot key should be registered.
  DCHECK(id_accelerators_.find(hot_key_id.id) != id_accelerators_.end());
  // Look up the accelerator based on this hot key ID.
  const ui::Accelerator& accelerator = id_accelerators_[hot_key_id.id];
  NotifyKeyPressed(accelerator);
}

bool GlobalShortcutListenerMac::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(accelerator_ids_.find(accelerator) == accelerator_ids_.end());

  if (Command::IsMediaKey(accelerator)) {
    // We should listen for media key presses through a MediaKeysListener. If
    // the MediaKeysListenerManager is enabled, we should listen through it,
    // which will tell the manager to send us the media key presses and prevent
    // the browser from using them.
    if (content::MediaKeysListenerManager::
            IsMediaKeysListenerManagerEnabled()) {
      content::MediaKeysListenerManager* media_keys_listener_manager =
          content::MediaKeysListenerManager::GetInstance();
      DCHECK(media_keys_listener_manager);

      if (!media_keys_listener_manager->StartWatchingMediaKey(
              accelerator.key_code(), this)) {
        return false;
      }
    } else {
      media_keys_listener_->StartWatchingMediaKey(accelerator.key_code());
    }
  } else {
    // Register hot_key if they are non-media keyboard shortcuts.
    if (!RegisterHotKey(accelerator, hot_key_id_))
      return false;

    if (!IsAnyHotKeyRegistered()) {
      StartWatchingHotKeys();
    }
  }

  // Store the hotkey-ID mappings we will need for lookup later.
  id_accelerators_[hot_key_id_] = accelerator;
  accelerator_ids_[accelerator] = hot_key_id_;
  ++hot_key_id_;
  return true;
}

void GlobalShortcutListenerMac::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(accelerator_ids_.find(accelerator) != accelerator_ids_.end());

  // Unregister the hot_key if it's a keyboard shortcut.
  if (!Command::IsMediaKey(accelerator))
    UnregisterHotKey(accelerator);

  // Remove hot_key from the mappings.
  KeyId key_id = accelerator_ids_[accelerator];
  id_accelerators_.erase(key_id);
  accelerator_ids_.erase(accelerator);

  if (Command::IsMediaKey(accelerator)) {
    // If we're listening to media keys through the MediaKeysListenerManager,
    // then inform the manager that we're no longer listening for the given key.
    if (content::MediaKeysListenerManager::
            IsMediaKeysListenerManagerEnabled()) {
      content::MediaKeysListenerManager* media_keys_listener_manager =
          content::MediaKeysListenerManager::GetInstance();
      DCHECK(media_keys_listener_manager);

      media_keys_listener_manager->StopWatchingMediaKey(accelerator.key_code(),
                                                        this);
    } else {
      media_keys_listener_->StopWatchingMediaKey(accelerator.key_code());
    }
  } else {
    // If we unregistered a hot key, and no more hot keys are registered, remove
    // the hot key handler.
    if (!IsAnyHotKeyRegistered()) {
      StopWatchingHotKeys();
    }
  }
}

void GlobalShortcutListenerMac::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  if (accelerator_ids_.find(accelerator) != accelerator_ids_.end()) {
    // If matched, callback to the event handling system.
    NotifyKeyPressed(accelerator);
  }
}

bool GlobalShortcutListenerMac::RegisterHotKey(
    const ui::Accelerator& accelerator, KeyId hot_key_id) {
  EventHotKeyID event_hot_key_id;

  // Signature uniquely identifies the application that owns this hot_key.
  event_hot_key_id.signature = base::apple::CreatorCodeForApplication();
  event_hot_key_id.id = hot_key_id;

  // Translate ui::Accelerator modifiers to cmdKey, altKey, etc.
  int modifiers = 0;
  modifiers |= (accelerator.IsShiftDown() ? shiftKey : 0);
  modifiers |= (accelerator.IsCtrlDown() ? controlKey : 0);
  modifiers |= (accelerator.IsAltDown() ? optionKey : 0);
  modifiers |= (accelerator.IsCmdDown() ? cmdKey : 0);

  int key_code =
      ui::MacKeyCodeForWindowsKeyCode(accelerator.key_code(), /*flags=*/0,
                                      /*us_keyboard_shifted_character=*/nullptr,
                                      /*keyboard_character=*/nullptr);

  // Register the event hot key.
  EventHotKeyRef hot_key_ref;
  OSStatus status = RegisterEventHotKey(key_code, modifiers, event_hot_key_id,
                                        GetApplicationEventTarget(),
                                        /*inOptions=*/0, &hot_key_ref);
  if (status != noErr)
    return false;

  id_hot_key_refs_[hot_key_id] = hot_key_ref;
  return true;
}

void GlobalShortcutListenerMac::UnregisterHotKey(
    const ui::Accelerator& accelerator) {
  // Ensure this accelerator is already registered.
  DCHECK(accelerator_ids_.find(accelerator) != accelerator_ids_.end());
  // Get the ref corresponding to this accelerator.
  KeyId key_id = accelerator_ids_[accelerator];
  EventHotKeyRef ref = id_hot_key_refs_[key_id];
  // Unregister the event hot key.
  UnregisterEventHotKey(ref);

  // Remove the event from the mapping.
  id_hot_key_refs_.erase(key_id);
}

void GlobalShortcutListenerMac::StartWatchingHotKeys() {
  DCHECK(!event_handler_);
  EventHandlerUPP hot_key_function = NewEventHandlerUPP(HotKeyHandler);
  EventTypeSpec event_type;
  event_type.eventClass = kEventClassKeyboard;
  event_type.eventKind = kEventHotKeyPressed;
  InstallApplicationEventHandler(
      hot_key_function, 1, &event_type, this, &event_handler_);
}

void GlobalShortcutListenerMac::StopWatchingHotKeys() {
  DCHECK(event_handler_);
  RemoveEventHandler(event_handler_);
  event_handler_ = nullptr;
}

bool GlobalShortcutListenerMac::IsAnyHotKeyRegistered() {
  for (auto& accelerator_id : accelerator_ids_) {
    if (!Command::IsMediaKey(accelerator_id.first)) {
      return true;
    }
  }
  return false;
}

// static
OSStatus GlobalShortcutListenerMac::HotKeyHandler(
    EventHandlerCallRef next_handler, EventRef event, void* user_data) {
  // Extract the hotkey from the event.
  EventHotKeyID hot_key_id;
  OSStatus result =
      GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                        /*outActualType=*/nullptr, sizeof(hot_key_id),
                        /*outActualSize=*/nullptr, &hot_key_id);
  if (result != noErr)
    return result;

  GlobalShortcutListenerMac* shortcut_listener =
      static_cast<GlobalShortcutListenerMac*>(user_data);
  shortcut_listener->OnHotKeyEvent(hot_key_id);
  return noErr;
}

}  // namespace extensions
