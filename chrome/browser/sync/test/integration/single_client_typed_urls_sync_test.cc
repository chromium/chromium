// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AddUrlToHistoryWithTransition;
using typed_urls_helper::CheckAllProfilesHaveSameTypedURLs;
using typed_urls_helper::DeleteUrlFromHistory;
using typed_urls_helper::GetTypedUrlsFromClient;

const char kSanityHistoryUrl[] = "http://www.sanity-history.google.com";

// TODO(crbug.com/1365291): Evaluate which of these tests should be kept after
// kSyncEnableHistoryDataType is enabled and HISTORY has replaced TYPED_URLS.

class SingleClientTypedUrlsSyncTest : public SyncTest {
 public:
  SingleClientTypedUrlsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientTypedUrlsSyncTest() override = default;

  bool UseVerifier() override {
// These tests are running on Android, but it has no multiple profile support,
// so verifier needs to be disabled.
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    // TODO(crbug.com/1137779): rewrite tests to not use verifier.
    return true;
#endif
  }
};

// Flaky on android: https://crbug.com/1159479
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Sanity DISABLED_Sanity
#else
#define MAYBE_Sanity Sanity
#endif
IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, MAYBE_Sanity) {
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

// Flaky on android: https://crbug.com/1159479
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TwoVisits DISABLED_TwoVisits
#else
#define MAYBE_TwoVisits TwoVisits
#endif
IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, MAYBE_TwoVisits) {
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

// Flaky on android: https://crbug.com/1159479
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeleteTyped DISABLED_DeleteTyped
#else
#define MAYBE_DeleteTyped DeleteTyped
#endif
IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, MAYBE_DeleteTyped) {
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

// Flaky on android: https://crbug.com/1159479
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeleteNonTyped DISABLED_DeleteNonTyped
#else
#define MAYBE_DeleteNonTyped DeleteNonTyped
#endif
IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, MAYBE_DeleteNonTyped) {
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
