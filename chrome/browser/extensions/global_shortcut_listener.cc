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

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "base/feature_list.h"
#include "chrome/browser/extensions/global_shortcut_listener_linux.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
BASE_FEATURE(kGlobalShortcutsPortal,
             "GlobalShortcutsPortal",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}  // namespace

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

#if BUILDFLAG(IS_OZONE) && BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
  if (base::FeatureList::IsEnabled(kGlobalShortcutsPortal)) {
    static GlobalShortcutListenerLinux* const linux_instance =
        new GlobalShortcutListenerLinux(nullptr);
    return linux_instance;
  }
#endif
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

  if (IsShortcutHandlingSuspended()) {
    return false;
  }

  accelerators_.push_back(accelerator);
  return global_accelerator_listener_->RegisterAccelerator(accelerator,
                                                           observer);
}

void GlobalShortcutListener::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    Observer* observer) {
  CHECK(global_accelerator_listener_);

  if (IsShortcutHandlingSuspended()) {
    return;
  }

  global_accelerator_listener_->UnregisterAccelerator(accelerator, observer);
  RemoveAccelerator(accelerator);
}

void GlobalShortcutListener::UnregisterAccelerators(Observer* observer) {
  CHECK(global_accelerator_listener_);

  if (IsShortcutHandlingSuspended()) {
    return;
  }

  std::vector<ui::Accelerator> removed =
      global_accelerator_listener_->UnregisterAccelerators(observer);
  for (const ui::Accelerator& accelerator : removed) {
    RemoveAccelerator(accelerator);
  }
}

void GlobalShortcutListener::SetShortcutHandlingSuspended(bool suspended) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(global_accelerator_listener_);

  if (shortcut_handling_suspended_ == suspended) {
    return;
  }

  shortcut_handling_suspended_ = suspended;
  for (auto& accelerator : accelerators_) {
    // On Linux, when shortcut handling is suspended we cannot simply early
    // return in NotifyKeyPressed (similar to what we do for non-global
    // shortcuts) because we'd eat the keyboard event thereby preventing the
    // user from setting the shortcut. Therefore we must unregister while
    // handling is suspended and register when handling resumes.
    if (shortcut_handling_suspended_) {
      global_accelerator_listener_->StopListeningForAccelerator(accelerator);
    } else {
      global_accelerator_listener_->StartListeningForAccelerator(accelerator);
    }
  }
}

bool GlobalShortcutListener::IsShortcutHandlingSuspended() const {
  return shortcut_handling_suspended_;
}

bool GlobalShortcutListener::IsRegistrationHandledExternally() const {
  return false;
}

void GlobalShortcutListener::OnCommandsChanged(
    const std::string& accelerator_group_id,
    const std::string& profile_id,
    const ui::CommandMap& commands,
    Observer* observer) {}

void GlobalShortcutListener::RemoveAccelerator(
    const ui::Accelerator& accelerator) {
  std::erase(accelerators_, accelerator);
}

}  // namespace extensions
