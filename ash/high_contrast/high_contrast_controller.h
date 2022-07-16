// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
#define ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"

namespace aura {
class Window;
}

namespace ash {

// Controls the accessibility high contrast mode for classic ash.
class ASH_EXPORT HighContrastController : public ShellObserver {
 public:
  HighContrastController();

  HighContrastController(const HighContrastController&) = delete;
  HighContrastController& operator=(const HighContrastController&) = delete;

  ~HighContrastController() override;

  // Set high contrast mode and update all available displays.
  void SetEnabled(bool enabled);

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

 private:
  // Update high contrast mode on the passed display.
  void UpdateDisplay(aura::Window* root_window);

  // Indicates if the high contrast mode is enabled or disabled.
  bool enabled_;
};

}  // namespace ash

#endif  // ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
