// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class CrowdDenyFakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  CrowdDenyFakeSafeBrowsingDatabaseManager();

  CrowdDenyFakeSafeBrowsingDatabaseManager(
      const CrowdDenyFakeSafeBrowsingDatabaseManager&) = delete;
  CrowdDenyFakeSafeBrowsingDatabaseManager& operator=(
      const CrowdDenyFakeSafeBrowsingDatabaseManager&) = delete;

  void SetSimulatedMetadataForUrl(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata);

  void RemoveAllBlocklistedUrls();

  void set_simulate_timeout(bool simulate_timeout) {
    simulate_timeout_ = simulate_timeout;
  }

  void set_simulate_synchronous_result(bool simulate_synchronous_result) {
    simulate_synchronous_result_ = simulate_synchronous_result;
  }

 protected:
  ~CrowdDenyFakeSafeBrowsingDatabaseManager() override;

  // safe_browsing::TestSafeBrowsingDatabaseManager:
  bool CheckApiBlocklistUrl(const GURL& url, Client* client) override;
  bool CancelApiCheck(Client* client) override;

 private:
  safe_browsing::ThreatMetadata GetSimulatedMetadataOrSafe(const GURL& url);

  std::set<raw_ptr<Client, SetExperimental>> pending_clients_;
  std::map<GURL, safe_browsing::ThreatMetadata>
      url_to_simulated_threat_metadata_;
  bool simulate_timeout_ = false;
  bool simulate_synchronous_result_ = false;

  base::WeakPtrFactory<CrowdDenyFakeSafeBrowsingDatabaseManager> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
