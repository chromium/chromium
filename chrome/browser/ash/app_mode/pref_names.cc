// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/pref_names.h"

namespace ash::prefs {

// Dictionary of kiosk cryptohomes scheduled to removed upon next startup.
// Keys are cryptohome ids, values are device local account emails, which
// describe the app this cryptohome is associated to.
const char kAllKioskUsersToRemove[] = "all-kiosk-users-to-remove";

}  // namespace ash::prefs
