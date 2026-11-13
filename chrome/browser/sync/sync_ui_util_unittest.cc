// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <string>

#include "chrome/browser/webauthn/passkey_unlock_error_strings.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError) {
  syncer::TestSyncService service;
  service.SetInitialSyncFeatureSetupComplete(true);
  service.SetPassphraseRequired();
  EXPECT_TRUE(ShouldShowSyncPassphraseError(&service));
}

// TODO(crbug.com/40066949): Remove this test when ConsentLevel::kSync is
// removed.
TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_SyncDisabled) {
  syncer::TestSyncService service;
  service.SetSignedIn(signin::ConsentLevel::kSync);  // nocheck
  service.SetInitialSyncFeatureSetupComplete(false);
  service.SetPassphraseRequired();
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_NotUsingPassphrase) {
  syncer::TestSyncService service;
  service.SetInitialSyncFeatureSetupComplete(true);
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, GetAvatarSyncErrorDescription_PasskeysLocked) {
  const std::u16string description = GetAvatarSyncErrorDescription(
      syncer::SyncService::UserActionableError::kPasskeysLocked,
      "test@gmail.com");
  EXPECT_FALSE(description.empty());
  EXPECT_EQ(description, webauthn::GetPasskeyErrorProfileMenuDetails());
  EXPECT_NE(description,
            GetAvatarSyncErrorDescription(
                syncer::SyncService::UserActionableError::kNeedsPassphrase,
                "test@gmail.com"));
}

}  // namespace
