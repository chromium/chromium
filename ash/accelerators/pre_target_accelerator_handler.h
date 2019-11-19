// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_PRE_TARGET_ACCELERATOR_HANDLER_H_
#define ASH_ACCELERATORS_PRE_TARGET_ACCELERATOR_HANDLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/wm/core/accelerator_delegate.h"

namespace aura {
class Window;
}

namespace ui {
class Accelerator;
class KeyEvent;
}  // namespace ui

namespace ash {

// PreTargetAcceleratorHandler is responsible for handling accelerators that
// are processed before the target is given a chance to process the
// accelerator. This typically includes system or reserved accelerators.
// PreTargetAcceleratorHandler does not actually handle the accelerators, rather
// it calls to AcceleratorController to actually process the accelerator.
class ASH_EXPORT PreTargetAcceleratorHandler
    : public ::wm::AcceleratorDelegate {
 public:
  PreTargetAcceleratorHandler();
  ~PreTargetAcceleratorHandler() override;

  // wm::AcceleratorDelegate:
  bool ProcessAccelerator(const ui::KeyEvent& event,
                          const ui::Accelerator& accelerator) override;

 private:
  // Returns true if the window should be allowed a chance to handle
  // system keys.
  bool CanConsumeSystemKeys(aura::Window* target, const ui::KeyEvent& event);

  // Returns true if the |accelerator| should be processed now.
  bool ShouldProcessAcceleratorNow(aura::Window* target,
                                   const ui::KeyEvent& event,
                                   const ui::Accelerator& accelerator);

  DISALLOW_COPY_AND_ASSIGN(PreTargetAcceleratorHandler);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_PRE_TARGET_ACCELERATOR_HANDLER_H_
