// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_PREFS_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_PREFS_H_

class PrefRegistrySimple;

namespace tab_group_sync {

// Register prefs for tab group sync. Presently only used on Android.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace tab_group_sync

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_PREFS_H_
