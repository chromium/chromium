// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/browser_with_test_window_test.h"
#endif

namespace {

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError) {
  syncer::TestSyncService service;
  service.SetInitialSyncFeatureSetupComplete(true);
  service.SetPassphraseRequired();
  EXPECT_TRUE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_SyncDisabled) {
  syncer::TestSyncService service;
  service.SetInitialSyncFeatureSetupComplete(false);
  service.SetPassphraseRequired();
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_NotUsingPassphrase) {
  syncer::TestSyncService service;
  service.SetInitialSyncFeatureSetupComplete(true);
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

#if !BUILDFLAG(IS_ANDROID)
using SyncUIUtilTestWithBrowser = BrowserWithTestWindowTest;

TEST_F(SyncUIUtilTestWithBrowser, ShowBookmarksLimitExceededHelp) {
  syncer::MockSyncService service;

  EXPECT_CALL(service,
              AcknowledgeBookmarksLimitExceededError(
                  syncer::SyncService::BookmarksLimitExceededHelpClickedSource::
                      kSettings));
  ShowBookmarksLimitExceededHelp(
      browser(), &service,
      syncer::SyncService::BookmarksLimitExceededHelpClickedSource::kSettings);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
