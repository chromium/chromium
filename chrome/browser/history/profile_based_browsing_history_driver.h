// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_PROFILE_BASED_BROWSING_HISTORY_DRIVER_H_
#define CHROME_BROWSER_HISTORY_PROFILE_BASED_BROWSING_HISTORY_DRIVER_H_

#include <vector>

#include "components/history/core/browser/browsing_history_driver.h"

class Profile;

// Base class that implements non-interface half of BrowsingHistoryDriver,
// backed by a profile.
class ProfileBasedBrowsingHistoryDriver
    : public history::BrowsingHistoryDriver {
 public:
  ProfileBasedBrowsingHistoryDriver(const ProfileBasedBrowsingHistoryDriver&) =
      delete;
  ProfileBasedBrowsingHistoryDriver& operator=(
      const ProfileBasedBrowsingHistoryDriver&) = delete;

  // BrowsingHistoryDriver implementation.
  void OnRemoveVisits(
      const std::vector<history::ExpireHistoryArgs>& expire_list) override;
  bool AllowHistoryDeletions() override;
  bool ShouldHideWebHistoryUrl(const GURL& url) override;
  history::WebHistoryService* GetWebHistoryService() override;
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      history::WebHistoryService* history_service,
      base::OnceCallback<void(bool)> callback) override;

  virtual Profile* GetProfile() = 0;

 protected:
  ProfileBasedBrowsingHistoryDriver() {}
};

#endif  // CHROME_BROWSER_HISTORY_PROFILE_BASED_BROWSING_HISTORY_DRIVER_H_
