// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"

#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

CrowdDenyFakeSafeBrowsingDatabaseManager::
    CrowdDenyFakeSafeBrowsingDatabaseManager()
    : safe_browsing::TestSafeBrowsingDatabaseManager(
          content::GetUIThreadTaskRunner({})) {}

void CrowdDenyFakeSafeBrowsingDatabaseManager::SetSimulatedMetadataForUrl(
    const GURL& url,
    const safe_browsing::ThreatMetadata& metadata) {
  url_to_simulated_threat_metadata_[url] = metadata;
}

void CrowdDenyFakeSafeBrowsingDatabaseManager::RemoveAllBlocklistedUrls() {
  url_to_simulated_threat_metadata_.clear();
}

CrowdDenyFakeSafeBrowsingDatabaseManager::
    ~CrowdDenyFakeSafeBrowsingDatabaseManager() {
  EXPECT_THAT(pending_clients_, testing::IsEmpty());
}

bool CrowdDenyFakeSafeBrowsingDatabaseManager::CheckApiBlocklistUrl(
    const GURL& url,
    Client* client) {
  if (simulate_synchronous_result_)
    return true;

  if (simulate_timeout_) {
    EXPECT_THAT(pending_clients_, testing::Not(testing::Contains(client)));
    pending_clients_.insert(client);
  } else {
    auto result = GetSimulatedMetadataOrSafe(url);
    client->OnCheckApiBlocklistUrlResult(url, std::move(result));
  }
  return false;
}

bool CrowdDenyFakeSafeBrowsingDatabaseManager::CancelApiCheck(Client* client) {
  EXPECT_THAT(pending_clients_, testing::Contains(client));
  pending_clients_.erase(client);
  return true;
}

safe_browsing::ThreatMetadata
CrowdDenyFakeSafeBrowsingDatabaseManager::GetSimulatedMetadataOrSafe(
    const GURL& url) {
  auto it = url_to_simulated_threat_metadata_.find(url);
  return it != url_to_simulated_threat_metadata_.end()
             ? it->second
             : safe_browsing::ThreatMetadata();
}
