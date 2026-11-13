// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_SCOPED_ACCELERATOR_DISABLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_SCOPED_ACCELERATOR_DISABLER_H_

#include <memory>

#include "base/memory/raw_ref.h"

namespace ash::kiosk {

// Interface used to disable and re-enable Ash accelerators during Kiosk launch
// and crash recovery. Overridable in tests.
class AcceleratorController {
 public:
  virtual ~AcceleratorController() = default;
  virtual void EnableAccelerators() = 0;
  virtual void DisableAccelerators() = 0;
};

// Creates the production `AcceleratorController` backed by `ash::Shell`.
std::unique_ptr<AcceleratorController> CreateAcceleratorController();

// RAII helper that disables accelerators on construction and re-enables them on
// destruction.
class [[nodiscard]] ScopedAcceleratorDisabler {
 public:
  explicit ScopedAcceleratorDisabler(AcceleratorController& controller);
  ScopedAcceleratorDisabler(const ScopedAcceleratorDisabler&) = delete;
  ScopedAcceleratorDisabler& operator=(const ScopedAcceleratorDisabler&) =
      delete;
  ~ScopedAcceleratorDisabler();

 private:
  const raw_ref<AcceleratorController> controller_;
};

}  // namespace ash::kiosk

#endif  // CHROME_BROWSER_ASH_APP_MODE_SCOPED_ACCELERATOR_DISABLER_H_
