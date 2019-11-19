// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_TYPES_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_TYPES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace keyboard {

// Flags that affect whether or not the virtual keyboard should be enabled.
// Enabled/Disabled flag pairs are mutually exclusive, but flags from multiple
// sources may be set. Precedence is determined by the implementation in
// KeyboardController::IsKeyboardEnableRequested.
enum class KeyboardEnableFlag {
  // Enabled by policy.
  kPolicyEnabled,

  // Disabled by policy.
  kPolicyDisabled,

  // Disabled by the Android keyboard.
  kAndroidDisabled,

  // Enabled by a first-party extension.
  kExtensionEnabled,

  // Disabled by a first-party extension.
  kExtensionDisabled,

  // Enabled by an a11y controller.
  kAccessibilityEnabled,

  // Enabled by the shelf/launcher controller.
  kShelfEnabled,

  // Enabled by the touch controller.
  kTouchEnabled,

  // Enabled via the "--enable-virtual-keyboard" command line switch.
  // Used for development and debugging.
  kCommandLineEnabled,
};

// Container types used to set and identify container behavior. Used in UMA
// stats gathering, so values should never be changed or reused.
enum class ContainerType {
  // Corresponds to a ContainerFullWidthBehavior.
  kFullWidth = 0,

  // Corresponds to a ContainerFloatingBehavior.
  kFloating = 1,

  // Corresponds to a ContainerFullscreenBehavior.
  // kFullscreen = 2,  // Deprecated; feature was abandoned.

  kMaxValue = ContainerType::kFloating,
};

}  // namespace keyboard

#endif  // ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_TYPES_H_
