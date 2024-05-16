// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_storage_notice/account_storage_notice.h"

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AccountStorageNoticeTest : public ::testing::Test {
 public:
  AccountStorageNoticeTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kAccountStorageNoticeShown, false);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  syncer::TestSyncService* sync_service() { return &sync_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
};

TEST_F(AccountStorageNoticeTest, ShouldNotShowIfSyncing) {
  sync_service()->SetSignedInWithSyncFeatureOn();

  EXPECT_FALSE(
      AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

TEST_F(AccountStorageNoticeTest, ShouldNotShowIfSignedOut) {
  sync_service()->SetSignedOut();
  // TODO(crbug.com/340502030): SetSignedOut() should empty the selected types.
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});

  EXPECT_FALSE(
      AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

TEST_F(AccountStorageNoticeTest, ShouldNotShowIfPasswordsDataTypeDisabled) {
  sync_service()->SetSignedInWithoutSyncFeature();
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_FALSE(
      AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

TEST_F(AccountStorageNoticeTest, ShouldNotShowIfAlreadyShown) {
  sync_service()->SetSignedInWithoutSyncFeature();
  pref_service()->SetBoolean(
      password_manager::prefs::kAccountStorageNoticeShown, true);

  EXPECT_FALSE(
      AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

TEST_F(AccountStorageNoticeTest, ShouldNotShowIfFlagDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
  sync_service()->SetSignedInWithoutSyncFeature();

  EXPECT_FALSE(
      AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

TEST_F(AccountStorageNoticeTest, ShouldShow) {
  base::test::ScopedFeatureList enable_feature(
      syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
  sync_service()->SetSignedInWithoutSyncFeature();

  EXPECT_TRUE(AccountStorageNotice::ShouldShow(pref_service(), sync_service()));
}

}  // namespace
