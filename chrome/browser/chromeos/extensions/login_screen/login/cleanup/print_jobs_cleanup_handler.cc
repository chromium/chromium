// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/print_jobs_cleanup_handler.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {

PrintJobsCleanupHandler::PrintJobsCleanupHandler() = default;

PrintJobsCleanupHandler::~PrintJobsCleanupHandler() = default;

void PrintJobsCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  DCHECK(callback_.is_null());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  callback_ = std::move(callback);

  ash::printing::print_management::PrintingManagerFactory::GetForProfile(
      profile)
      ->DeleteAllPrintJobs(
          base::BindOnce(&PrintJobsCleanupHandler::OnDeleteAllPrintJobsDone,
                         base::Unretained(this)));
}

void PrintJobsCleanupHandler::OnDeleteAllPrintJobsDone(bool success) {
  if (!success) {
    std::move(callback_).Run("Failed to delete all print jobs");
    return;
  }

  std::move(callback_).Run(std::nullopt);
}

}  // namespace chromeos
