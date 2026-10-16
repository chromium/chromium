// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/scoped_global_hotkey.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace dictation {

ScopedGlobalHotkey::ScopedGlobalHotkey(ui::GlobalAcceleratorListener* listener,
                                       const ui::Accelerator& accelerator,
                                       base::RepeatingClosure on_pressed)
    : listener_(listener),
      accelerator_(accelerator),
      on_pressed_(std::move(on_pressed)) {
  CHECK(on_pressed_);
  // With the Linux XDG GlobalShortcuts portal (e.g. on Wayland), users bind
  // keys to named commands in the desktop environment, and arbitrary
  // accelerators can't be registered. Skip in that case.
  if (!listener_ || listener_->IsRegistrationHandledExternally()) {
    return;
  }
  // Fails if the accelerator is taken or shortcut handling is suspended.
  registered_ = listener_->RegisterAccelerator(accelerator_, this);
}

ScopedGlobalHotkey::~ScopedGlobalHotkey() {
  if (registered_) {
    listener_->UnregisterAccelerator(accelerator_, this);
  }
}

void ScopedGlobalHotkey::OnKeyPressed(const ui::Accelerator& accelerator) {
  // `on_pressed_` typically destroys `this`, so run it asynchronously rather
  // than inside the listener's notification. Dropped if `this` is destroyed
  // first.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ScopedGlobalHotkey::RunOnPressed,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ScopedGlobalHotkey::ExecuteCommand(const std::string& accelerator_group_id,
                                        const std::string& command_id) {}

void ScopedGlobalHotkey::RunOnPressed() {
  on_pressed_.Run();
}

}  // namespace dictation
