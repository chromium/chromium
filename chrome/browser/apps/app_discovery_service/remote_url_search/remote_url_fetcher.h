// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_FETCHER_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_FETCHER_H_

#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/app_fetcher_manager.h"
#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_index.h"

class Profile;

namespace apps {

// A fetcher for app recommendations coming from a hardcoded URL. This manages
// the querying the URL, and indexing and searching the results.
class RemoteUrlFetcher : public AppFetcher {
 public:
  explicit RemoteUrlFetcher(Profile* profile);
  ~RemoteUrlFetcher() override;

  RemoteUrlFetcher(const RemoteUrlFetcher&) = delete;
  RemoteUrlFetcher& operator=(const RemoteUrlFetcher&) = delete;

  // AppFetcher:
  void GetApps(ResultCallback callback) override;

 private:
  std::unique_ptr<RemoteUrlIndex> index_;

  bool enabled_ = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_REMOTE_URL_SEARCH_REMOTE_URL_FETCHER_H_
