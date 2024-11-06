// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_

#include "chrome/browser/profiles/profile.h"

namespace file_manager::trash {

// Handles the 30-day Trash files autocleanup.
class TrashAutoCleanup {
 public:
  ~TrashAutoCleanup() = default;

  TrashAutoCleanup(const TrashAutoCleanup&) = delete;
  TrashAutoCleanup& operator=(const TrashAutoCleanup&) = delete;

  static std::unique_ptr<TrashAutoCleanup> Create(Profile* profile);

 private:
  explicit TrashAutoCleanup(Profile* profile);
  void Init();

  raw_ptr<Profile> profile_;
};

}  // namespace file_manager::trash

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_AUTO_CLEANUP_H_
