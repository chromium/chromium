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

// Sets the shelf preference value for the display with the given |display_id|.
ASH_PUBLIC_EXPORT void SetPerDisplayShelfPref(PrefService* prefs,
                                              int64_t display_id,
                                              const char* pref_key,
                                              const std::string& value);

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

// Whether the desk button should be shown according to the current prefs.
ASH_PUBLIC_EXPORT bool GetDeskButtonVisibility(PrefService* prefs);

// Set the show desk button pref to reflect the user manually showing or
// hiding the desk button.
ASH_PUBLIC_EXPORT void SetShowDeskButtonInShelfPref(PrefService* prefs,
                                                    bool show);

// Set the device uses desks pref to reflect if the user has ever added a
// virtual desk (true) or not (false).
ASH_PUBLIC_EXPORT void SetDeviceUsesDesksPref(PrefService* prefs,
                                              bool uses_desks);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_PREFS_H_
