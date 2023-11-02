// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_PREFS_MIGRATION_UMA_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_PREFS_MIGRATION_UMA_H_

namespace file_manager {

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// FileManagerPrefsMigrationStatus enum listing in
// tools/metrics/histograms/enums.xml.
enum class FileManagerPrefsMigrationStatus {
  kSuccess = 0,
  kFailNoExistingPreferences = 1,
  kFailMigratingPreferences = 2,
  kMaxValue = kFailMigratingPreferences,
};

extern const char kPrefsMigrationStatusUMA[] =
    "FileBrowser.SWA.PrefsMigrationStatus";

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_PREFS_MIGRATION_UMA_H_
