// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_CLEANUP_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_CLEANUP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"

namespace policy::local_user_files {

// Kicks-off user files removal when LocalUserFilesEnabled is set to 'false'.
class LocalFilesCleanup : public LocalUserFilesPolicyObserver {
 public:
  LocalFilesCleanup();
  ~LocalFilesCleanup() override;

 private:
  // policy::local_user_files::Observer overrides:
  void OnLocalUserFilesPolicyChanged() override;

  // Callback called once cleanup is done.
  void CleanupDone(const std::optional<std::string>& error_message);

  chromeos::FilesCleanupHandler cleanup_handler_;

  // Tracks whether a cleanup is already in progress.
  bool in_progress_ = false;

  // Tracks number of requested cleanup during this user session.
  size_t cleanups_count_ = 0;

  base::WeakPtrFactory<LocalFilesCleanup> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_CLEANUP_H_
