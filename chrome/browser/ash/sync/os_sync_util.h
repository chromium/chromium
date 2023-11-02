// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_OS_SYNC_UTIL_H_
#define CHROME_BROWSER_ASH_SYNC_OS_SYNC_UTIL_H_

class PrefService;

namespace os_sync_util {

// Sets up the OS sync feature and its model types depending on the user's
// existing browser sync model types. |prefs| are user profile prefs.
void MigrateOsSyncPreferences(PrefService* prefs);

}  // namespace os_sync_util

#endif  // CHROME_BROWSER_ASH_SYNC_OS_SYNC_UTIL_H_
