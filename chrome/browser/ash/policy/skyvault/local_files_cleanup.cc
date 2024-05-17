// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_files_cleanup.h"

#include <optional>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {

constexpr char kCleanupCountHistogram[] = "SkyVault.LocalUserFilesCleanupCount";

LocalFilesCleanup::LocalFilesCleanup() {}

LocalFilesCleanup::~LocalFilesCleanup() {}

void LocalFilesCleanup::OnLocalUserFilesPolicyChanged() {
  if (!LocalUserFilesAllowed() && !in_progress_) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(kCleanupCountHistogram, ++cleanups_count_, 1,
                                50, 50);
    in_progress_ = true;
    cleanup_handler_.Cleanup(base::BindOnce(&LocalFilesCleanup::CleanupDone,
                                            weak_factory_.GetWeakPtr()));
  }
}

void LocalFilesCleanup::CleanupDone(
    const std::optional<std::string>& error_message) {
  in_progress_ = false;
  if (error_message.has_value()) {
    LOG(ERROR) << "Local files cleanup failed: " << error_message.value();
  } else {
    VLOG(1) << "Local files cleanup done";
  }
}

}  // namespace policy::local_user_files
