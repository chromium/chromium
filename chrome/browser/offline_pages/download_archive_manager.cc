// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/offline_pages/download_archive_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {

DownloadArchiveManager::DownloadArchiveManager(
    const base::FilePath& temporary_archives_dir,
    const base::FilePath& private_archives_dir,
    const base::FilePath& public_archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    PrefService* prefs)
    : ArchiveManager(temporary_archives_dir,
                     private_archives_dir,
                     public_archives_dir,
                     task_runner),
      prefs_(prefs) {}

DownloadArchiveManager::~DownloadArchiveManager() {}

const base::FilePath& DownloadArchiveManager::GetPublicArchivesDir() {
  if (prefs_) {
    // Use the preference set by the download location dialog, if present.
    std::string directory_preference =
        prefs_->GetString(prefs::kDownloadDefaultDirectory);
    if (!directory_preference.empty()) {
      download_archives_dir_ = base::FilePath(directory_preference);
      // Must set the member variable so the reference will outlive the
      // funciton call.
      return download_archives_dir_;
    }
  }

  return ArchiveManager::GetPublicArchivesDir();
}

}  // namespace offline_pages
