// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_EPHEMERAL_USER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_EPHEMERAL_USER_H_

#include "ash/ash_export.h"

#include "components/account_id/account_id.h"

namespace ash {

// Returns true if the user's wallpaper is to be treated as ephemeral.
// Guest users and other users whose wallpaper should not persist after sign out
// are considered ephemeral.
ASH_EXPORT bool IsEphemeralUser(const AccountId& account_id);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_EPHEMERAL_USER_H_
