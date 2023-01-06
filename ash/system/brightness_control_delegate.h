// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Delegate for controlling the brightness.
class BrightnessControlDelegate {
 public:
  virtual ~BrightnessControlDelegate() {}

  // Handles an accelerator-driven request to decrease or increase the screen
  // brightness.
  virtual void HandleBrightnessDown() = 0;
  virtual void HandleBrightnessUp() = 0;

  // Requests that the brightness be set to |percent|, in the range
  // [0.0, 100.0].  |gradual| specifies whether the transition to the new
  // brightness should be animated or instantaneous.
  virtual void SetBrightnessPercent(double percent, bool gradual) = 0;

  // Asynchronously invokes |callback| with the current brightness, in the range
  // [0.0, 100.0]. In case of error, it is called with nullopt.
  virtual void GetBrightnessPercent(
      base::OnceCallback<void(absl::optional<double>)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_CONTROL_DELEGATE_H_
