// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_
#define ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_

#include <ostream>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// The window's pin type enum.
enum class WindowPinType {
  kNone,

  // The window is pinned on top of other windows.
  kPinned,

  // The window is pinned on top of other windows. It is similar to
  // kPinned but does not allow user to exit the mode by shortcut key.
  kTrustedPinned,
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& stream,
                                           WindowPinType pin_type);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_PIN_TYPE_H_
