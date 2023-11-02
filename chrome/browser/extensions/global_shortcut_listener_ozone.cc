// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_ozone.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/ozone/public/ozone_platform.h"

using content::BrowserThread;

namespace extensions {

GlobalShortcutListenerOzone::GlobalShortcutListenerOzone() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_global_shortcut_listener_ =
      ui::OzonePlatform::GetInstance()->GetPlatformGlobalShortcutListener(this);
}

GlobalShortcutListenerOzone::~GlobalShortcutListenerOzone() {
  if (is_listening_)
    StopListening();

  if (platform_global_shortcut_listener_)
    platform_global_shortcut_listener_->ResetDelegate();
}

void GlobalShortcutListenerOzone::StartListening() {
  DCHECK(!is_listening_);
  DCHECK(!registered_hot_keys_.empty());

  if (platform_global_shortcut_listener_)
    platform_global_shortcut_listener_->StartListening();

  is_listening_ = true;
}

void GlobalShortcutListenerOzone::StopListening() {
  DCHECK(is_listening_);
  DCHECK(registered_hot_keys_.empty());

  if (platform_global_shortcut_listener_)
    platform_global_shortcut_listener_->StopListening();

  is_listening_ = false;
}

bool GlobalShortcutListenerOzone::RegisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(!base::Contains(registered_hot_keys_, accelerator));

  if (!platform_global_shortcut_listener_)
    return false;

  const bool registered =
      platform_global_shortcut_listener_->RegisterAccelerator(
          accelerator.key_code(), accelerator.IsAltDown(),
          accelerator.IsCtrlDown(), accelerator.IsShiftDown());
  if (registered)
    registered_hot_keys_.insert(accelerator);
  return registered;
}

void GlobalShortcutListenerOzone::UnregisterAcceleratorImpl(
    const ui::Accelerator& accelerator) {
  DCHECK(base::Contains(registered_hot_keys_, accelerator));
  // Otherwise how could the accelerator be registered?
  DCHECK(platform_global_shortcut_listener_);

  platform_global_shortcut_listener_->UnregisterAccelerator(
      accelerator.key_code(), accelerator.IsAltDown(), accelerator.IsCtrlDown(),
      accelerator.IsShiftDown());
  registered_hot_keys_.erase(accelerator);
}

void GlobalShortcutListenerOzone::OnKeyPressed(ui::KeyboardCode key_code,
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

void GlobalShortcutListenerOzone::OnPlatformListenerDestroyed() {
  platform_global_shortcut_listener_ = nullptr;
}

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static GlobalShortcutListenerOzone* instance =
      new GlobalShortcutListenerOzone();
  return instance;
}

}  // namespace extensions
