// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_SERVICE_UTIL_H_
#define CHROME_BROWSER_ASH_DBUS_SERVICE_UTIL_H_

#include <string>

#include "chrome/browser/profiles/profile.h"

namespace ash {

// Gets the profile for a user, or the active profile if the user ID hash is
// empty.
Profile* GetProfileFromUserIdHash(const std::string& user_id_hash);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_SERVICE_UTIL_H_
