// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
#define ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/brightness_control_delegate.h"

namespace ash {
namespace system {

// A class which controls brightness when F6, F7 or a multimedia key for
// brightness is pressed.
class ASH_EXPORT BrightnessControllerChromeos
    : public ash::BrightnessControlDelegate {
 public:
  BrightnessControllerChromeos() {}

  BrightnessControllerChromeos(const BrightnessControllerChromeos&) = delete;
  BrightnessControllerChromeos& operator=(const BrightnessControllerChromeos&) =
      delete;

  ~BrightnessControllerChromeos() override {}

  // Overridden from ash::BrightnessControlDelegate:
  void HandleBrightnessDown() override;
  void HandleBrightnessUp() override;
  void SetBrightnessPercent(double percent, bool gradual) override;
  void GetBrightnessPercent(
      base::OnceCallback<void(std::optional<double>)> callback) override;
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_CONTROLLER_CHROMEOS_H_
