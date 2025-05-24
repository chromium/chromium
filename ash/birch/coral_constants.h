// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_CORAL_CONSTANTS_H_
#define ASH_BIRCH_CORAL_CONSTANTS_H_

#include "ash/ash_export.h"

namespace ash {

// The maximum number of entities to build suppression context.
inline constexpr int kMaxItemsForCoralSuppressionContext = 10;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ASH_EXPORT CoralSource {
  kUnknown,    // the initial unset source value.
  kPostLogin,  // the source of the item is from post-login coral response.
  kInSession,  // the source of the item is from Overview session response.
  kMaxValue = kInSession,
};

}  // namespace ash

#endif  // ASH_BIRCH_CORAL_CONSTANTS_H_
