// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_sync_delegate_impl.h"

#include <memory>

#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/driver/test_sync_user_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordStoreBackendSyncDelegateImplTest : public testing::Test {
 protected:
  PasswordStoreBackendSyncDelegateImplTest() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    testing_profile_ = builder.Build();

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            testing_profile_.get(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })));

    sync_delegate_ = std::make_unique<PasswordStoreBackendSyncDelegateImpl>(
        testing_profile_.get());
  }

  ~PasswordStoreBackendSyncDelegateImplTest() override = default;

  syncer::TestSyncService* sync_service() { return sync_service_; }

  password_manager::PasswordStoreBackend::SyncDelegate* sync_delegate() {
    return sync_delegate_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<syncer::TestSyncService> sync_service_;
  std::unique_ptr<PasswordStoreBackendSyncDelegateImpl> sync_delegate_;
};

TEST_F(PasswordStoreBackendSyncDelegateImplTest, SyncDisabled) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  sync_service()->SetHasSyncConsent(false);
  EXPECT_FALSE(sync_delegate()->IsSyncingPasswordsEnabled());
  EXPECT_EQ(absl::nullopt, sync_delegate()->GetSyncingAccount());
}

TEST_F(PasswordStoreBackendSyncDelegateImplTest,
       SyncEnabledButNotForPasswords) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service()->SetHasSyncConsent(true);
  static_cast<syncer::TestSyncUserSettings*>(sync_service()->GetUserSettings())
      ->SetSelectedTypes(/*sync_everything=*/false,
                         {syncer::UserSelectableType::kHistory});
  EXPECT_FALSE(sync_delegate()->IsSyncingPasswordsEnabled());
  EXPECT_EQ(absl::nullopt, sync_delegate()->GetSyncingAccount());
}

TEST_F(PasswordStoreBackendSyncDelegateImplTest, SyncEnabled) {
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service()->SetHasSyncConsent(true);
  AccountInfo active_info;
  active_info.email = "test@email.com";
  sync_service()->SetAccountInfo(active_info);
  EXPECT_TRUE(sync_delegate()->IsSyncingPasswordsEnabled());
  EXPECT_TRUE(sync_delegate()->GetSyncingAccount().has_value());
  EXPECT_EQ(active_info.email, sync_delegate()->GetSyncingAccount().value());
}
