// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SCOPED_GLOBAL_HOTKEY_H_
#define CHROME_BROWSER_DICTATION_SCOPED_GLOBAL_HOTKEY_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace dictation {

// Registers `accelerator` OS-wide for as long as this object is alive and runs
// `on_pressed` (asynchronously) when it is struck. Intended to exist only while
// Dictation is actively listening, so that the user can stop Dictation from
// any window.
//
// Registration is best-effort. Nothing is registered if `listener` is null,
// shortcut handling is suspended, registration is handled externally (e.g. the
// Linux portal), or the accelerator is already taken by another caller/app.
class ScopedGlobalHotkey : public ui::GlobalAcceleratorListener::Observer {
 public:
  ScopedGlobalHotkey(ui::GlobalAcceleratorListener* listener,
                     const ui::Accelerator& accelerator,
                     base::RepeatingClosure on_pressed);
  ScopedGlobalHotkey(const ScopedGlobalHotkey&) = delete;
  ScopedGlobalHotkey& operator=(const ScopedGlobalHotkey&) = delete;
  ~ScopedGlobalHotkey() override;

  bool is_registered() const { return registered_; }
  const ui::Accelerator& accelerator() const { return accelerator_; }

  // ui::GlobalAcceleratorListener::Observer:
  void OnKeyPressed(const ui::Accelerator& accelerator) override;
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override;

 private:
  void RunOnPressed();

  const raw_ptr<ui::GlobalAcceleratorListener> listener_;
  const ui::Accelerator accelerator_;
  const base::RepeatingClosure on_pressed_;
  bool registered_ = false;
  base::WeakPtrFactory<ScopedGlobalHotkey> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SCOPED_GLOBAL_HOTKEY_H_
