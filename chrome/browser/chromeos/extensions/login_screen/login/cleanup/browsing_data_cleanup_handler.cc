// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browsing_data_cleanup_handler.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browsing_data_remover.h"

namespace chromeos {

BrowsingDataCleanupHandler::BrowsingDataCleanupHandler() = default;

BrowsingDataCleanupHandler::~BrowsingDataCleanupHandler() = default;

void BrowsingDataCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  DCHECK(callback_.is_null());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  callback_ = std::move(callback);

  remover_ = profile->GetBrowsingDataRemover();
  remover_->AddObserver(this);

  remover_->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::ALL_DATA_TYPES,
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB |
          content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      this);
}

void BrowsingDataCleanupHandler::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  remover_->RemoveObserver(this);

  if (failed_data_types == 0) {
    std::move(callback_).Run(absl::nullopt);
    return;
  }

  std::move(callback_).Run("Failed to cleanup all data with flag: " +
                           base::NumberToString(failed_data_types));
}

}  // namespace chromeos
