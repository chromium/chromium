// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_CONSTANTS_H_
#define ASH_PUBLIC_CPP_ASH_CONSTANTS_H_

#include "ash/public/cpp/accessibility_controller_enums.h"

// Most Ash constants live in ash/constants/ash_constants.h. Only a few of theme
// are here to avoid making //ash/constants depend on //ash/public/cpp.
namespace ash {

constexpr AutoclickEventType kDefaultAutoclickEventType =
    AutoclickEventType::kLeftClick;

// The default automatic click menu position.
constexpr FloatingMenuPosition kDefaultAutoclickMenuPosition =
    FloatingMenuPosition::kSystemDefault;

// The default floating accessibility menu position.
constexpr FloatingMenuPosition kDefaultFloatingMenuPosition =
    FloatingMenuPosition::kSystemDefault;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_CONSTANTS_H_
