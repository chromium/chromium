// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/browser_cleanup_handler.h"

#include <thread>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browsing_data_remover.h"

namespace {

// TODO(maleksandrov, b:258196743) Move the logic into BrowserList class.
bool HasBrowsersForProfile(Profile* profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile())
      return true;
  }
  return false;
}

}  // namespace

namespace chromeos {

BrowserCleanupHandler::BrowserCleanupHandler() = default;

BrowserCleanupHandler::~BrowserCleanupHandler() = default;

void BrowserCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  DCHECK(callback_.is_null());

  profile_ = ProfileManager::GetActiveUserProfile();
  if (!profile_) {
    std::move(callback).Run("There is no active user");
    return;
  }

  callback_ = std::move(callback);

  if (!HasBrowsersForProfile(profile_)) {
    RemoveBrowserHistory();
    return;
  }

  BrowserList::AddObserver(this);

  // `on_close_success` doesn't wait for browser to close and is therefore not
  // used. `on_close_aborted` cannot be reached because `skip_beforeunload` is
  // true. Instead, this process should trigger `OnBrowserRemoved` method.
  BrowserList::CloseAllBrowsersWithProfile(
      profile_,
      /*on_close_success=*/BrowserList::CloseCallback(),
      /*on_close_aborted=*/BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
}

void BrowserCleanupHandler::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  // In case any browser window is still open for current profile the cleanup
  // must not proceed otherwise some open tabs can remain in browser data.
  for (Browser* open_browser : *BrowserList::GetInstance()) {
    if (open_browser->profile() == profile_)
      return;
  }

  BrowserList::RemoveObserver(this);
  RemoveBrowserHistory();
}

void BrowserCleanupHandler::RemoveBrowserHistory() {
  data_remover_ = profile_->GetBrowsingDataRemover();
  data_remover_->AddObserver(this);

  // This process should trigger `OnBrowsingDataRemoverDone` method.
  data_remover_->RemoveAndReply(
      /*delete_begin=*/base::Time(),
      /*delete_end=*/base::Time::Max(),
      /*remove_mask=*/chrome_browsing_data_remover::ALL_DATA_TYPES,
      /*origin_type_mask=*/
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB |
          content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      /*observer=*/this);
}

void BrowserCleanupHandler::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  data_remover_->RemoveObserver(this);

  if (failed_data_types == 0) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  std::move(callback_).Run("Failed to cleanup all data with flag: " +
                           base::NumberToString(failed_data_types));
}

}  // namespace chromeos
