// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_DISCOVERY_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_DISCOVERY_METRICS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

// Class that manages recording metrics related to app discovery from the
// launcher.
class AppDiscoveryMetricsManager {
 public:
  explicit AppDiscoveryMetricsManager(Profile* profile);
  ~AppDiscoveryMetricsManager();

  AppDiscoveryMetricsManager(const AppDiscoveryMetricsManager&) = delete;
  AppDiscoveryMetricsManager& operator=(const AppDiscoveryMetricsManager&) =
      delete;

  // Called when the launcher is opened.
  void OnLauncherOpen();

  // Called when a result is launched from the launcher.
  void OnOpenResult(ChromeSearchResult* result, const std::u16string& query);

 private:
  // Returns whether app sync is enabled for |profile_| or not.
  bool IsAppSyncEnabled();

  // Profile of the current user.
  raw_ptr<Profile> profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_DISCOVERY_METRICS_MANAGER_H_
