// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_PREFS_H_
#define ASH_PUBLIC_CPP_SHELF_PREFS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"

class PrefService;

namespace ash {

// Values used for prefs::kShelfAutoHideBehavior.
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehaviorAlways[];
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehaviorNever[];

// Values used for prefs::kShelfAlignment.
ASH_PUBLIC_EXPORT extern const char kShelfAlignmentBottom[];
ASH_PUBLIC_EXPORT extern const char kShelfAlignmentLeft[];
ASH_PUBLIC_EXPORT extern const char kShelfAlignmentRight[];

// Get the shelf auto hide behavior preference for a particular display.
ASH_PUBLIC_EXPORT ShelfAutoHideBehavior
GetShelfAutoHideBehaviorPref(PrefService* prefs, int64_t display_id);

// Set the shelf auto hide behavior preference for a particular display.
ASH_PUBLIC_EXPORT void SetShelfAutoHideBehaviorPref(
    PrefService* prefs,
    int64_t display_id,
    ShelfAutoHideBehavior behavior);

// Get the shelf alignment preference for a particular display.
ASH_PUBLIC_EXPORT ShelfAlignment GetShelfAlignmentPref(PrefService* prefs,
                                                       int64_t display_id);

// Set the shelf alignment preference for a particular display.
ASH_PUBLIC_EXPORT void SetShelfAlignmentPref(PrefService* prefs,
                                             int64_t display_id,
                                             ShelfAlignment alignment);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_PREFS_H_
