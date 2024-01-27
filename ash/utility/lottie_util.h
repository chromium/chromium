// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_LOTTIE_UTIL_H_
#define ASH_UTILITY_LOTTIE_UTIL_H_

#include <string_view>

#include "ash/ash_export.h"

namespace ash {

// Standard id prefix for all entities in a Lottie animation that are meant to
// be dynamically controlled as run-time (as opposed to fixed for the lifetime
// of the animation). Examples include:
// * Image asset ids, where the client may dynamically embed photos of
//   interest into the animation.
// * Color/Text node names, where the client may set the color or text at
//   run-time to something other than what's baked into the Lottie file.
//
// Note this convention is the standard for all ash animations, but it is not a
// generic Lottie file standard.
inline constexpr std::string_view kLottieCustomizableIdPrefix = "_CrOS";

// Simple convenience function that checks the |id| for the prefix above.
ASH_EXPORT bool IsCustomizableLottieId(std::string_view id);

}  // namespace ash

#endif  // ASH_UTILITY_LOTTIE_UTIL_H_
