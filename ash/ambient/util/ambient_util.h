// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_
#define ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_

#include "ash/ash_export.h"
#include "ash/login/ui/lock_screen.h"

namespace ash {

namespace ambient {
namespace util {

// Returns true if Ash is showing lock screen.
ASH_EXPORT bool IsShowing(LockScreen::ScreenType type);

}  // namespace util
}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_AMBIENT_UTIL_H_
