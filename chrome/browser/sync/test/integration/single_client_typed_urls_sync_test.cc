// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/driver/profile_sync_service.h"

using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AddUrlToHistoryWithTransition;
using typed_urls_helper::CheckAllProfilesHaveSameTypedURLs;
using typed_urls_helper::DeleteUrlFromHistory;
using typed_urls_helper::GetTypedUrlsFromClient;

const char kSanityHistoryUrl[] = "http://www.sanity-history.google.com";

class SingleClientTypedUrlsSyncTest : public SyncTest {
 public:
  SingleClientTypedUrlsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientTypedUrlsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientTypedUrlsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  AddUrlToHistory(0, new_url);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Wait for sync and verify client did not change.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());
}

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, TwoVisits) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  // Adding twice should add two visits with distinct timestamps.
  AddUrlToHistory(0, new_url);
  AddUrlToHistory(0, new_url);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Wait for sync and verify client did not change.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());
}

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, DeleteTyped) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  // Adding twice should add two visits with distinct timestamps.
  AddUrlToHistory(0, new_url);
  AddUrlToHistory(0, new_url);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Wait for sync and verify client did not change.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Now delete the URL we just added, wait for sync, and verify the deletion.
  DeleteUrlFromHistory(0, new_url);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());
}

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, DeleteNonTyped) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  // Add a non-typed URL.
  AddUrlToHistoryWithTransition(0, new_url, ui::PAGE_TRANSITION_LINK,
                                history::SOURCE_BROWSED);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Wait for sync and verify client did not change.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());

  // Now delete the URL we just added, wait for sync and verify the deletion.
  DeleteUrlFromHistory(0, new_url);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());
  ASSERT_TRUE(CheckAllProfilesHaveSameTypedURLs());
}
