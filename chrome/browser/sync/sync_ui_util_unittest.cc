// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace
