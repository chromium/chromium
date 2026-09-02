// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_test_util.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::private_verification_tokens::test::CreateTestIssuer;
using ::private_verification_tokens::test::FutureExpiration;
using ::private_verification_tokens::test::GetFutureExpiration;

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

class PrivateVerificationTokensServiceTest : public testing::Test {
 public:
  PrivateVerificationTokensServiceTest()
      : test_issuer_(CreateTestIssuer(2, "1")) {
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

  const private_verification_tokens::PrivacyPassAthmIssuer& test_issuer()
      const {
    return *test_issuer_;
  }

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
      void OnInitializationComplete() override {
        if (callback_) {
          std::move(callback_).Run();
        }
      }

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

  void WaitForTokensStored(PrivateVerificationTokensService* target_service) {
    base::test::TestFuture<void> future;
    class Waiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit Waiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnTokensStored() override {
        if (callback_) {
          std::move(callback_).Run();
        }
      }

     private:
      base::OnceClosure callback_;
    };
    Waiter waiter(future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&waiter);
    observation.Observe(target_service);
    EXPECT_TRUE(future.Wait());
  }

  void SetTestIssuerConfig(PrivateVerificationTokensService* target_service) {
    const GURL issuer_request_url_a("https://a.com/pvt/issue");
    const url::Origin redeemer_a =
        url::Origin::Create(GURL("https://r1.a.com"));
    const GURL issuer_request_url_b("https://b.org/issue/p/i");
    const url::Origin redeemer_b =
        url::Origin::Create(GURL("https://r2.b.org"));
    const GURL issuer_request_url_c("https://c.net/some/issue/path");
    const url::Origin redeemer_c = url::Origin::Create(GURL("https://c.net"));
    const GURL issuer_request_url_d("https://d.com/pvt/i");
    const url::Origin redeemer_d = url::Origin::Create(GURL("https://d.com"));
    const std::string encoded_public_key =
        base::Base64Encode(test_issuer_->public_key_bytes());
    const std::string encoded_public_key_proof =
        base::Base64Encode(test_issuer_->public_key_proof_bytes());
    const FutureExpiration future_expiration = GetFutureExpiration();
    const std::string expiration_str = future_expiration.string_rep;
    const std::string json_str = base::StringPrintf(
        R"({
      "issuers": [
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "1"
        },
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "1"
        },
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "1"
        },
        {
          "issuerRequestUrl": "%s",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 2,
          "expiration": "%s",
          "redeemers": [
            "%s"
          ],
          "deploymentId": "1"
        }
      ]
    })",
        issuer_request_url_a.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_a.Serialize(),
        issuer_request_url_b.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_b.Serialize(),
        issuer_request_url_c.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_c.Serialize(),
        issuer_request_url_d.spec(), encoded_public_key,
        encoded_public_key_proof, expiration_str, redeemer_d.Serialize());

    auto config =
        private_verification_tokens::PrivateVerificationTokensIssuerConfig::
            Create(base::test::ParseJsonDict(json_str));
    ASSERT_TRUE(config);
    target_service->SetIssuerConfig(config);
  }

  void AdvanceTime(base::TimeDelta time_delta) {
    ASSERT_FALSE(time_delta.is_negative());
    task_environment_.FastForwardBy(time_delta);
  }

 private:
  std::optional<private_verification_tokens::PrivacyPassAthmIssuer>
      test_issuer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(PrivateVerificationTokensServiceTest, GetTokenForRedemption_Success) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  const url::Origin redeemer_a = url::Origin::Create(GURL("https://r1.a.com"));
  auto token = service()->GetTokenForRedemption(redeemer_a);
  ASSERT_TRUE(token.has_value());
  EXPECT_EQ(token->second, base::Base64Encode(std::vector<uint8_t>{1, 2, 3}));

  // Calling again should still return the token because GetTokenForRedemption
  // does not delete or consume it.
  auto token_second = service()->GetTokenForRedemption(redeemer_a);
  ASSERT_TRUE(token_second.has_value());
  EXPECT_EQ(token_second->first, token->first);
  EXPECT_EQ(token_second->second, token->second);

  // Verify tokens are still in the database.
  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));

  // Explicitly deleting the token removes it from cache and database.
  base::test::TestFuture<void> delete_future;
  service()->DeleteToken(token->first, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<url::Origin>> future2;
  service()->GetTokenIssuers(future2.GetCallback());
  auto issuers2 = future2.Take();
  EXPECT_THAT(issuers2,
              testing::ElementsAre(url::Origin::Create(GURL("https://b.org"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_UnregisteredRedeemer_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto token = service()->GetTokenForRedemption(
      url::Origin::Create(GURL("https://unregistered.com")));
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_NoTokensInStore_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  // c.net is a registered issuer & redeemer in config, but has no tokens
  // stored.
  auto token = service()->GetTokenForRedemption(
      url::Origin::Create(GURL("https://c.net")));
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_AntiAbuseBlockedForIssuer_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  const url::Origin issuer_a = url::Origin::Create(GURL("https://a.com"));
  const url::Origin redeemer_a = url::Origin::Create(GURL("https://r1.a.com"));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(issuer_a.GetURL(), issuer_a.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_BLOCK);

  auto token = service()->GetTokenForRedemption(redeemer_a);
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_AntiAbuseBlockedForRedeemer_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  const url::Origin redeemer_a = url::Origin::Create(GURL("https://r1.a.com"));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(redeemer_a.GetURL(), redeemer_a.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_BLOCK);

  auto token = service()->GetTokenForRedemption(redeemer_a);
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_NoConfig_ReturnsNullopt) {
  WaitForInitialization(service());

  auto token = service()->GetTokenForRedemption(
      url::Origin::Create(GURL("https://r1.a.com")));
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_WhenShuttingDown_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  service()->Shutdown();

  auto token = service()->GetTokenForRedemption(
      url::Origin::Create(GURL("https://r1.a.com")));
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteToken_PendingBeforeInitialization_Success) {
  EXPECT_FALSE(service()->is_initialized());

  base::test::TestFuture<void> delete_future;
  service()->DeleteToken(1, delete_future.GetCallback());
  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service());
  EXPECT_TRUE(delete_future.Wait());
}

TEST_F(PrivateVerificationTokensServiceTest,
       DeleteToken_WhenShuttingDown_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  service()->Shutdown();
  base::test::TestFuture<void> future;
  service()->DeleteToken(1, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(PrivateVerificationTokensServiceTest,
       IsRegisteredRedeemer_RegisteredRedeemer_ReturnsTrue) {
  SetTestIssuerConfig(service());

  EXPECT_TRUE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://r1.a.com"))));
  EXPECT_TRUE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://r2.b.org"))));
  EXPECT_TRUE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://c.net"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       IsRegisteredRedeemer_UnregisteredRedeemer_ReturnsFalse) {
  SetTestIssuerConfig(service());

  EXPECT_FALSE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://unknown.com"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       IsRegisteredRedeemer_NoConfig_ReturnsFalse) {
  EXPECT_FALSE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://r1.a.com"))));
}

TEST_F(PrivateVerificationTokensServiceEmptyDatabaseTest,
       MaybeFetchTokens_Success_FetchesAndStoresTokens) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(request.url, GURL("https://c.net/some/issue/path"));
        std::string request_body;
        if (request.request_body) {
          for (const auto& element : *request.request_body->elements()) {
            if (element.type() == network::DataElement::Tag::kBytes) {
              const auto& bytes =
                  element.As<network::DataElementBytes>().bytes();
              request_body.append(bytes.begin(), bytes.end());
            }
          }
        }

        auto response = test_issuer().issue_batch_from_bytes(
            rs_std::SliceRef<const uint8_t>(base::as_byte_span(request_body)),
            /*hidden_metadata=*/0);
        ASSERT_TRUE(response.has_value());
        test_url_loader_factory.AddResponse(
            request.url.spec(),
            std::string(response->begin(), response->end()));
      }));

  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  WaitForTokensStored(service());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers,
              testing::ElementsAre(url::Origin::Create(GURL("https://c.net"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_TokensAboveThreshold_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  // Store a second token for a.com so count is 2 (threshold for batch_size 2 is
  // > 1).
  const auto expiration = base::Time::Now() + base::Hours(2);
  base::test::TestFuture<void> future;
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      tokens;
  tokens.emplace_back(url::Origin::Create(GURL("https://a.com")),
                      std::vector<uint8_t>{1, 2, 4}, 1, expiration, 1);
  service()->StoreTokens(std::move(tokens), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://a.com/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_TokensAtOrBelowThreshold_FetchesTokens) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  // Store starts with 1 token for a.com. With batch_size 2, 1 <= (2/2 = 1), so
  // fetch is triggered.
  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://a.com/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 1);
  EXPECT_EQ(test_url_loader_factory.GetPendingRequest(0)->request.url,
            GURL("https://a.com/pvt/issue"));
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_UnregisteredIssuer_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://unregistered.com/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_AntiAbuseBlocked_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  const url::Origin issuer_c = url::Origin::Create(GURL("https://c.net"));
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(issuer_c.GetURL(), issuer_c.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_BLOCK);

  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_NullFactory_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"), nullptr);
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_WhenShuttingDown_DoesNothing) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  service()->Shutdown();

  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
}

TEST_F(PrivateVerificationTokensServiceEmptyDatabaseTest,
       MaybeFetchTokens_PendingBeforeInitialization_QueuesOperation) {
  SetTestIssuerConfig(service());

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(request.url, GURL("https://c.net/some/issue/path"));
        std::string request_body;
        if (request.request_body) {
          for (const auto& element : *request.request_body->elements()) {
            if (element.type() == network::DataElement::Tag::kBytes) {
              const auto& bytes =
                  element.As<network::DataElementBytes>().bytes();
              request_body.append(bytes.begin(), bytes.end());
            }
          }
        }
        auto response = test_issuer().issue_batch_from_bytes(
            rs_std::SliceRef<const uint8_t>(base::as_byte_span(request_body)),
            /*hidden_metadata=*/0);
        ASSERT_TRUE(response.has_value());
        test_url_loader_factory.AddResponse(
            request.url.spec(),
            std::string(response->begin(), response->end()));
      }));

  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  // Not initialized yet, so no fetch should be completed yet.
  EXPECT_FALSE(service()->is_initialized());

  WaitForInitialization(service());
  WaitForTokensStored(service());

  base::test::TestFuture<std::vector<url::Origin>> future;
  service()->GetTokenIssuers(future.GetCallback());
  auto issuers = future.Take();
  EXPECT_THAT(issuers,
              testing::ElementsAre(url::Origin::Create(GURL("https://c.net"))));
}

TEST_F(PrivateVerificationTokensServiceTest,
       MaybeFetchTokens_ConfigExpired_ReturnsEarly) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto origin = url::Origin::Create(GURL("https://c.net"));
  base::Time expiration =
      service()->issuer_config()->config().at(origin).public_key.expiration();
  base::TimeDelta advance_delta = expiration - base::Time::Now();
  AdvanceTime(advance_delta);

  network::TestURLLoaderFactory test_url_loader_factory;
  service()->MaybeFetchTokens(GURL("https://c.net/pvt/issue"),
                              test_url_loader_factory.GetSafeWeakWrapper());

  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
}

TEST_F(PrivateVerificationTokensServiceTest,
       GetTokenForRedemption_ConfigExpired_ReturnsNullopt) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto origin = url::Origin::Create(GURL("https://a.com"));
  base::Time expiration =
      service()->issuer_config()->config().at(origin).public_key.expiration();
  base::TimeDelta advance_delta = expiration - base::Time::Now();
  AdvanceTime(advance_delta);

  const url::Origin redeemer_a = url::Origin::Create(GURL("https://r1.a.com"));
  auto token = service()->GetTokenForRedemption(redeemer_a);
  EXPECT_FALSE(token.has_value());
}

TEST_F(PrivateVerificationTokensServiceTest,
       IsRegisteredRedeemer_ConfigExpired_ReturnsFalse) {
  WaitForInitialization(service());
  SetTestIssuerConfig(service());

  auto origin = url::Origin::Create(GURL("https://a.com"));
  base::Time expiration =
      service()->issuer_config()->config().at(origin).public_key.expiration();
  base::TimeDelta advance_delta = expiration - base::Time::Now();
  AdvanceTime(advance_delta);

  EXPECT_FALSE(service()->IsRegisteredRedeemer(
      url::Origin::Create(GURL("https://r1.a.com"))));
}

}  // namespace
