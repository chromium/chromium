// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/scoped_accelerator_disabler.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"

namespace ash::kiosk {

namespace {

class DefaultAcceleratorController : public AcceleratorController {
 public:
  DefaultAcceleratorController() = default;
  DefaultAcceleratorController(const DefaultAcceleratorController&) = delete;
  DefaultAcceleratorController& operator=(const DefaultAcceleratorController&) =
      delete;
  ~DefaultAcceleratorController() override = default;

  void DisableAccelerators() override {
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        true);
  }

  void EnableAccelerators() override {
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        false);
  }
};

}  // namespace

std::unique_ptr<AcceleratorController> CreateAcceleratorController() {
  return std::make_unique<DefaultAcceleratorController>();
}

ScopedAcceleratorDisabler::ScopedAcceleratorDisabler(
    AcceleratorController& controller)
    : controller_(controller) {
  controller_->DisableAccelerators();
}

ScopedAcceleratorDisabler::~ScopedAcceleratorDisabler() {
  controller_->EnableAccelerators();
}

}  // namespace ash::kiosk
