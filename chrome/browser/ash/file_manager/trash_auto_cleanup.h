// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_

#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_info_validator.h"
#include "chrome/browser/profiles/profile.h"

namespace file_manager::trash {

inline constexpr base::TimeDelta kMaxTrashAge = base::Days(30);

enum class AutoCleanupResult {
  kCleanupSuccessful = 0,
  kNoOldFilesToCleanup,
  kTrashInfoParsingError,
  kDeletionError,
};

// Handles the 30-day Trash files autocleanup.
class TrashAutoCleanup {
 public:
  ~TrashAutoCleanup();

  TrashAutoCleanup(const TrashAutoCleanup&) = delete;
  TrashAutoCleanup& operator=(const TrashAutoCleanup&) = delete;

  static std::unique_ptr<TrashAutoCleanup> Create(Profile* profile);

 private:
  friend class TrashAutoCleanupTest;

  explicit TrashAutoCleanup(Profile* profile);

  void Init();
  void StartCleanup();
  void OnTrashInfoFilesToDeleteEnumerated(
      const std::vector<base::FilePath>& trash_info_paths_to_delete);
  void OnTrashInfoFilesParsed(
      std::vector<file_manager::trash::ParsedTrashInfoDataOrError>
          parsed_data_or_error);
  void OnCleanupDone(bool success);

  void SetCleanupDoneCallbackForTest(
      base::OnceCallback<void(AutoCleanupResult result)> cleanup_done_closure);

  raw_ptr<Profile> profile_;
  std::unique_ptr<file_manager::trash::TrashInfoValidator> validator_ = nullptr;
  std::vector<base::FilePath> trash_info_directories_;
  base::OnceCallback<void(AutoCleanupResult result)>
      cleanup_done_closure_for_test_;

  base::WeakPtrFactory<TrashAutoCleanup> weak_ptr_factory_{this};
};

}  // namespace file_manager::trash

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_
