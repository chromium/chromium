// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/pref_names.h"

namespace chromeos {
namespace prefs {

// Dictionary of kiosk cryptohomes scheduled to removed upon next startup.
// Keys are cryptohome ids, values are device local account emails, which
// describe the app this cryptohome is associated to.
const char kAllKioskUsersToRemove[] = "all-kiosk-users-to-remove";

// Legacy prefs that will only be set when some cryptohomes were scheduled
// to be removed, but chrome update happened.
// TODO(crbug.com/1014431): Remove these prefs where the migration is
// completed.
// Dictionary that stores the list of Chrome kiosk cryptohomes to
// be deleted along with their app_ids.
const char kRegularKioskUsersToRemove[] = "kiosk-users-to-remove";
// List of Android kiosk cryptohomes scheduled to be removed upon next
// startup.
const char kArcKioskUsersToRemove[] = "arc-kiosk-users-to-remove";

}  // namespace prefs
}  // namespace chromeos
