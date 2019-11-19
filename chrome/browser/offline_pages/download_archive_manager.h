// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_DOWNLOAD_ARCHIVE_MANAGER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_DOWNLOAD_ARCHIVE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/offline_pages/core/archive_manager.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// Manages the directories used for OfflinePages.  Dynamically get the directory
// to use for downloads from the Download system.
class DownloadArchiveManager : public ArchiveManager {
 public:
  DownloadArchiveManager(
      const base::FilePath& temporary_archives_dir,
      const base::FilePath& private_archives_dir,
      const base::FilePath& public_archives_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      PrefService* prefs);
  ~DownloadArchiveManager() override;

  const base::FilePath& GetPublicArchivesDir() override;

 private:
  PrefService* prefs_;
  base::FilePath download_archives_dir_;

  DISALLOW_COPY_AND_ASSIGN(DownloadArchiveManager);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_DOWNLOAD_ARCHIVE_MANAGER_H_
