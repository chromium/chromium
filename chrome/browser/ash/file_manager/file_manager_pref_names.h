// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_PREF_NAMES_H_

class PrefRegistrySimple;

namespace file_manager::prefs {

extern const char kSmbfsEnableVerboseLogging[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace file_manager::prefs

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_PREF_NAMES_H_
