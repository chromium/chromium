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

void CrowdDenyFakeSafeBrowsingDatabaseManager::SetSimulatedVerdictForUrl(
    const GURL& url,
    bool is_abusive) {
  url_to_simulated_verdict_[url] = is_abusive;
}

void CrowdDenyFakeSafeBrowsingDatabaseManager::RemoveAllBlocklistedUrls() {
  url_to_simulated_verdict_.clear();
}

CrowdDenyFakeSafeBrowsingDatabaseManager::
    ~CrowdDenyFakeSafeBrowsingDatabaseManager() {
  EXPECT_THAT(pending_clients_, testing::IsEmpty());
}

bool CrowdDenyFakeSafeBrowsingDatabaseManager::CheckNotificationAbuseUrl(
    const GURL& url,
    Client* client) {
  if (simulate_synchronous_result_) {
    return true;
  }

  if (simulate_timeout_) {
    EXPECT_THAT(pending_clients_, testing::Not(testing::Contains(client)));
    pending_clients_.insert(client);
  } else {
    bool is_abusive = GetSimulatedVerdictOrSafe(url);
    client->OnCheckNotificationAbuseUrlResult(is_abusive);
  }
  return false;
}

bool CrowdDenyFakeSafeBrowsingDatabaseManager::CancelNotificationAbuseCheck(
    Client* client) {
  EXPECT_THAT(pending_clients_, testing::Contains(client));
  pending_clients_.erase(client);
  return true;
}

bool CrowdDenyFakeSafeBrowsingDatabaseManager::GetSimulatedVerdictOrSafe(
    const GURL& url) {
  auto it = url_to_simulated_verdict_.find(url);
  return it != url_to_simulated_verdict_.end() ? it->second : false;
}
