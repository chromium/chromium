// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

class PrivateVerificationTokensServiceTest : public testing::Test {
 public:
  PrivateVerificationTokensServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    HostContentSettingsMapFactory::GetForProfile(&profile_)
        ->SetDefaultContentSetting(ContentSettingsType::ANTI_ABUSE,
                                   CONTENT_SETTING_ALLOW);
    db_path_ = temp_dir_.GetPath().Append(kDatabaseName);
    PrepopulateDatabase();
    service_ = PrivateVerificationTokensService::Create(
        temp_dir_.GetPath(),
        HostContentSettingsMapFactory::GetForProfile(&profile_));
    ASSERT_TRUE(service_);
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    WaitForAllTasksPosted();
  }

  void WaitForAllTasksPosted() {
    base::test::TestFuture<void> cleanup_future;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, cleanup_future.GetCallback());
    EXPECT_TRUE(cleanup_future.Wait());
  }

  virtual void PrepopulateDatabase() {
    StoreInDatabase(db_path_, CreateTestTokens());
  }

  void StoreInDatabase(
      const base::FilePath& db_path,
      const std::vector<
          private_verification_tokens::PrivateVerificationTokensToken>&
          tokens) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::unique_ptr<
        private_verification_tokens::PrivateVerificationTokensDatabase>
        database = private_verification_tokens::
            PrivateVerificationTokensDatabase::Create(db_path);
    ASSERT_TRUE(database);
    database->StoreTokens(tokens);
  }

  PrivateVerificationTokensService* service() { return service_.get(); }
  TestingProfile* profile() { return &profile_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }
  const base::FilePath& db_path() const { return db_path_; }

  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
  CreateTestTokens() const {
    std::vector<private_verification_tokens::PrivateVerificationTokensToken>
        tokens;
    const auto expiration = base::Time::Now() + base::Hours(2);
    tokens.emplace_back(url::Origin::Create(GURL("https://a.com")),
                        std::vector<uint8_t>{1, 2, 3}, 1, expiration, 1);
    tokens.emplace_back(url::Origin::Create(GURL("https://b.org")),
                        std::vector<uint8_t>{4, 5, 6, 7}, 2, expiration, 1);
    return tokens;
  }

  void WaitForInitialization(PrivateVerificationTokensService* target_service) {
    if (target_service->is_initialized()) {
      return;
    }
    base::test::TestFuture<void> init_future;
    class Waiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit Waiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnInitializationComplete() override { std::move(callback_).Run(); }

     private:
      base::OnceClosure callback_;
    };
    Waiter waiter(init_future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&waiter);
    observation.Observe(target_service);
    EXPECT_TRUE(init_future.Wait());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  TestingProfile profile_;
  std::unique_ptr<PrivateVerificationTokensService> service_;
};

class PrivateVerificationTokensServiceEmptyDatabaseTest
    : public PrivateVerificationTokensServiceTest {
 public:
  void PrepopulateDatabase() override { StoreInDatabase(db_path(), {}); }
};

TEST_F(PrivateVerificationTokensServiceEmptyDatabaseTest,
       EmptyDatabase_ReturnsEmpty) {
  WaitForInitialization(service());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  auto issuers = future.Take();
  EXPECT_TRUE(issuers.empty());
}

class PrivateVerificationTokensServiceNoDatabaseFileTest
    : public PrivateVerificationTokensServiceTest {
 public:
  void PrepopulateDatabase() override {
    // Do nothing to ensure no database file exists on disk.
  }
};

TEST_F(PrivateVerificationTokensServiceNoDatabaseFileTest,
       NoDatabaseFile_ReturnsEmpty) {
  WaitForInitialization(service());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  auto issuers = future.Take();
  EXPECT_TRUE(issuers.empty());
}

TEST_F(PrivateVerificationTokensServiceTest, StoreTokens_Success) {
  WaitForInitialization(service());

  const auto expiration = base::Time::Now() + base::Hours(2);
  const auto c_origin = url::Origin::Create(GURL("https://c.net"));
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      new_tokens = {
          private_verification_tokens::PrivateVerificationTokensToken(
              c_origin, std::vector<uint8_t>{10, 11}, 3, expiration, 1),
      };

  base::test::TestFuture<void> store_future;
  service()->StoreTokens(std::move(new_tokens), store_future.GetCallback());
  EXPECT_TRUE(store_future.Wait());

  base::test::TestFuture<std::vector<url::Origin>> get_future;
  service()->GetTokenIssuers(get_future.GetCallback());

  auto issuers = get_future.Take();
  EXPECT_THAT(issuers,
              testing::UnorderedElementsAre(
                  url::Origin::Create(GURL("https://a.com")),
                  url::Origin::Create(GURL("https://b.org")), c_origin));
}

TEST_F(PrivateVerificationTokensServiceTest,
       StoreTokens_WhenAntiAbuseSettingBlockedForOrigin_FiltersOriginTokens) {
  WaitForInitialization(service());

  const auto c_origin = url::Origin::Create(GURL("https://c.net"));
  const auto d_origin = url::Origin::Create(GURL("https://d.com"));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(d_origin.GetURL(), d_origin.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_BLOCK);

  const auto expiration = base::Time::Now() + base::Hours(2);
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      new_tokens = {
          private_verification_tokens::PrivateVerificationTokensToken(
              c_origin, std::vector<uint8_t>{10, 11}, 3, expiration, 1),
          private_verification_tokens::PrivateVerificationTokensToken(
              d_origin, std::vector<uint8_t>{12, 13}, 4, expiration, 1),
      };

  base::test::TestFuture<void> store_future;
  service()->StoreTokens(std::move(new_tokens), store_future.GetCallback());
  EXPECT_TRUE(store_future.Wait());

  base::test::TestFuture<std::vector<url::Origin>> get_future;
  service()->GetTokenIssuers(get_future.GetCallback());

  auto issuers = get_future.Take();
  EXPECT_THAT(issuers,
              testing::UnorderedElementsAre(
                  url::Origin::Create(GURL("https://a.com")),
                  url::Origin::Create(GURL("https://b.org")), c_origin));
}

TEST_F(PrivateVerificationTokensServiceTest, GetTokenIssuers_Success) {
  WaitForInitialization(service());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  auto issuers = future.Take();
  EXPECT_EQ(issuers.size(), 2u);
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenIssuers_WhenShuttingDown_ReturnsEmpty) {
  WaitForInitialization(service());

  service()->Shutdown();

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  auto issuers = future.Take();
  EXPECT_TRUE(issuers.empty());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenIssuers_PendingBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  WaitForInitialization(service());

  auto issuers = future.Take();
  EXPECT_EQ(issuers.size(), 2u);
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenIssuers_PendingShutdownBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Shut down the service before initialization; this should flush pending
  // operations with empty results.
  service()->Shutdown();

  auto issuers = future.Take();
  EXPECT_TRUE(issuers.empty());
}

TEST_F(PrivateVerificationTokensServiceTest, DeleteTokens_Success) {
  WaitForInitialization(service());

  // Verify tokens exist first.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service()->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_EQ(issuers_future.Get().size(), 2u);

  // Delete tokens for a.com.
  base::test::TestFuture<void> delete_future;
  service()->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});
  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future2;
  service()->GetTokenIssuers(issuers_future2.GetCallback());
  auto issuers = issuers_future2.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokens_PendingBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<void> delete_future;
  service()->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service());
  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service()->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokens_PendingShutdownBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<void> delete_future;
  service()->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});

  EXPECT_FALSE(delete_future.IsReady());

  // Shut down the service before initialization; this should flush pending
  // operations.
  service()->Shutdown();

  EXPECT_TRUE(delete_future.Wait());
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokens_WhenShuttingDown_ReturnsImmediately) {
  WaitForInitialization(service());

  service()->Shutdown();

  base::test::TestFuture<void> future;
  service()->DeleteTokens(
      base::Time::Min(), base::Time::Max(), future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});

  EXPECT_TRUE(future.Wait());
}

TEST_F(PrivateVerificationTokensServiceTest, DeleteTokensByFilter_Success) {
  WaitForInitialization(service());

  // Verify tokens exist first.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service()->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_EQ(issuers_future.Get().size(), 2u);

  base::test::TestFuture<void> delete_future;

  base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter =
      base::BindRepeating([](const blink::StorageKey& key) {
        return key == blink::StorageKey::CreateFirstParty(
                          url::Origin::Create(GURL("https://a.com")));
      });

  service()->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                  storage_key_filter,
                                  delete_future.GetCallback());

  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future2;
  service()->GetTokenIssuers(issuers_future2.GetCallback());
  auto issuers = issuers_future2.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokensByFilter_WhenShuttingDown_ReturnsImmediately) {
  WaitForInitialization(service());

  service()->Shutdown();

  base::test::TestFuture<void> delete_future;

  base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter =
      base::BindRepeating([](const blink::StorageKey& key) {
        return key == blink::StorageKey::CreateFirstParty(
                          url::Origin::Create(GURL("https://a.com")));
      });

  service()->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                  storage_key_filter,
                                  delete_future.GetCallback());

  EXPECT_TRUE(delete_future.Wait());
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokensByFilter_PendingBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<void> delete_future;

  base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter =
      base::BindRepeating([](const blink::StorageKey& key) {
        return key == blink::StorageKey::CreateFirstParty(
                          url::Origin::Create(GURL("https://a.com")));
      });

  service()->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                  storage_key_filter,
                                  delete_future.GetCallback());

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service());

  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service()->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteTokensByFilter_NullFilterPendingBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<void> delete_future;

  auto storage_key_filter =
      base::RepeatingCallback<bool(const blink::StorageKey&)>();

  service()->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                  storage_key_filter,
                                  delete_future.GetCallback());

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service());

  EXPECT_TRUE(delete_future.Wait());

  // Verify deletion worked.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service()->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 0u);
}

TEST_F(PrivateVerificationTokensServiceTest, SetIssuerConfig_UpdatesService) {
  EXPECT_FALSE(service()->issuer_config());

  base::DictValue dict;
  dict.Set(private_verification_tokens::kIssuersKey, base::ListValue());
  auto config = private_verification_tokens::
      PrivateVerificationTokensIssuerConfig::Create(std::move(dict));
  ASSERT_TRUE(config);

  service()->SetIssuerConfig(config);
  EXPECT_EQ(service()->issuer_config(), config);
}

TEST_F(PrivateVerificationTokensServiceTest,
       GlobalIssuerConfig_PropagatesToNewServiceInstances) {
  base::ScopedClosureRunner reset_global_config(base::BindOnce(
      &PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig,
      PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig()));

  base::DictValue dict;
  dict.Set(private_verification_tokens::kIssuersKey, base::ListValue());
  auto config = private_verification_tokens::
      PrivateVerificationTokensIssuerConfig::Create(std::move(dict));
  ASSERT_TRUE(config);

  PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(config);
  EXPECT_EQ(PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig(),
            config);

  TestingProfile new_profile;
  auto* new_service =
      PrivateVerificationTokensServiceFactory::GetForProfile(&new_profile);
  ASSERT_TRUE(new_service);
  EXPECT_EQ(new_service->issuer_config(), config);
}

}  // namespace
