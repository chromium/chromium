// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "build/config/linux/dbus/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

using content::BrowserThread;

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  if (global_accelerator_listener) {
    static GlobalShortcutListener* const instance =
        new GlobalShortcutListener(global_accelerator_listener);
    return instance;
  }

  return nullptr;
}

GlobalShortcutListener::GlobalShortcutListener(
    ui::GlobalAcceleratorListener* global_accelerator_listener)
    : global_accelerator_listener_(global_accelerator_listener) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListener::~GlobalShortcutListener() = default;

bool GlobalShortcutListener::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    Observer* observer) {
  CHECK(global_accelerator_listener_);
  return global_accelerator_listener_->RegisterAccelerator(accelerator,
                                                           observer);
}

void GlobalShortcutListener::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    Observer* observer) {
  CHECK(global_accelerator_listener_);
  global_accelerator_listener_->UnregisterAccelerator(accelerator, observer);
}

void GlobalShortcutListener::UnregisterAccelerators(Observer* observer) {
  CHECK(global_accelerator_listener_);
  global_accelerator_listener_->UnregisterAccelerators(observer);
}

void GlobalShortcutListener::SetShortcutHandlingSuspended(bool suspended) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(global_accelerator_listener_);
  global_accelerator_listener_->SetShortcutHandlingSuspended(suspended);
}

bool GlobalShortcutListener::IsShortcutHandlingSuspended() const {
  CHECK(global_accelerator_listener_);
  return global_accelerator_listener_->IsShortcutHandlingSuspended();
}

bool GlobalShortcutListener::IsRegistrationHandledExternally() const {
  return global_accelerator_listener_->IsRegistrationHandledExternally();
}

void GlobalShortcutListener::OnCommandsChanged(
    const std::string& accelerator_group_id,
    const std::string& profile_id,
    const ui::CommandMap& commands,
    Observer* observer) {
  global_accelerator_listener_->OnCommandsChanged(
      accelerator_group_id, profile_id, commands, observer);
}

}  // namespace extensions
