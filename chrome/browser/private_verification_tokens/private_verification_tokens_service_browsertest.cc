// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "components/private_verification_tokens/mojom/private_verification_tokens_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

class PrivateVerificationTokensServiceBrowserTest : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    db_path_ = GetProfile()->GetPath().Append(kDatabaseName);
    PrepopulateDatabase();
  }

  virtual void PrepopulateDatabase() {
    StoreInDatabase(db_path_, CreateTestTokens());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

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

  void WaitForInitialization(PrivateVerificationTokensService* service) {
    if (service->is_initialized()) {
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
    observation.Observe(service);
    EXPECT_TRUE(init_future.Wait());
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

  void VerifyTokens(
      const std::vector<private_verification_tokens::mojom::
                            PrivateVerificationTokensTokenPtr>& actual_tokens,
      const std::vector<
          private_verification_tokens::PrivateVerificationTokensToken>&
          expected_tokens) {
    base::flat_map<url::Origin, std::vector<uint8_t>> expected_map;
    for (const auto& token : expected_tokens) {
      expected_map[token.issuer()] = token.token();
    }

    EXPECT_EQ(actual_tokens.size(), expected_map.size());
    for (const auto& token : actual_tokens) {
      auto it = expected_map.find(token->issuer);
      ASSERT_TRUE(it != expected_map.end());
      EXPECT_EQ(token->serialized_token, it->second);
    }
  }

  // SetTestIssuerConfig sets a fixed config for the given service.
  void SetTestIssuerConfig(PrivateVerificationTokensService* service) {
    const url::Origin issuer_a = url::Origin::Create(GURL("https://a.com"));
    const url::Origin redeemer_a =
        url::Origin::Create(GURL("https://r1.a.com"));
    const url::Origin issuer_b = url::Origin::Create(GURL("https://b.org"));
    const url::Origin redeemer_b =
        url::Origin::Create(GURL("https://r2.b.org"));
    // The only redeemer is same as the issuer for c and d.
    const url::Origin issuer_c = url::Origin::Create(GURL("https://c.net"));
    const url::Origin issuer_d = url::Origin::Create(GURL("https://d.com"));
    const std::vector<uint8_t> serialized_public_key = {3, 6, 8, 12, 14};
    const std::string encoded_public_key =
        base::Base64Encode(serialized_public_key);
    const std::vector<uint8_t> serialized_public_key_proof = {1, 2, 4, 8};
    const std::string encoded_public_key_proof =
        base::Base64Encode(serialized_public_key_proof);
    const std::string expiration_str = "12";
    const std::string json_str = base::StringPrintf(
        R"({
      "issuers": [
        {
          "issuerRequestUrl": "%s/pvt/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "%s",
          "redeemers": ["%s"],
          "deploymentId": "test-deployment-id"
        },
        {
          "issuerRequestUrl": "%s/pvt/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "%s",
          "redeemers": ["%s"],
          "deploymentId": "test-deployment-id"
        },
        {
          "issuerRequestUrl": "%s/pvt/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "%s",
          "redeemers": ["%s"],
          "deploymentId": "test-deployment-id"
        },
        {
          "issuerRequestUrl": "%s/pvt/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 3,
          "expiration": "%s",
          "redeemers": ["%s"],
          "deploymentId": "test-deployment-id"
        }
      ]
    })",
        issuer_a.Serialize(), encoded_public_key, encoded_public_key_proof,
        expiration_str, redeemer_a.Serialize(), issuer_b.Serialize(),
        encoded_public_key, encoded_public_key_proof, expiration_str,
        redeemer_b.Serialize(), issuer_c.Serialize(), encoded_public_key,
        encoded_public_key_proof, expiration_str, issuer_c.Serialize(),
        issuer_d.Serialize(), encoded_public_key, encoded_public_key_proof,
        expiration_str, issuer_d.Serialize());

    auto config =
        private_verification_tokens::PrivateVerificationTokensIssuerConfig::
            Create(base::test::ParseJsonDict(json_str));
    ASSERT_TRUE(config);
    service->SetIssuerConfig(config);
  }

 protected:
  base::FilePath db_path_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetForProfile_FeatureEnabled_ReturnsInstance) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(service);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokens_Success) {
  Profile* profile = GetProfile();

  // Get service
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  // Call GetTokens and verify
  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  auto expected_tokens = CreateTestTokens();
  VerifyTokens(tokens, expected_tokens);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokens_WhenAntiAbuseSettingBlocked_ReturnsEmpty) {
  Profile* profile = GetProfile();

  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(ContentSettingsType::ANTI_ABUSE,
                                 CONTENT_SETTING_BLOCK);

  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  EXPECT_TRUE(tokens.empty());
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    GetTokens_WhenAntiAbuseSettingBlockedForOrigin_FiltersOriginTokens) {
  Profile* profile = GetProfile();

  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(
          GURL("https://a.com"), GURL("https://a.com"),
          ContentSettingsType::ANTI_ABUSE, CONTENT_SETTING_BLOCK);

  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  const auto expiration = base::Time::Now() + base::Hours(2);
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      expected_tokens;
  expected_tokens.emplace_back(url::Origin::Create(GURL("https://b.org")),
                               std::vector<uint8_t>{4, 5, 6, 7}, 2, expiration,
                               1);
  VerifyTokens(tokens, expected_tokens);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       StoreTokens_Success) {
  Profile* profile = GetProfile();

  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  const auto expiration = base::Time::Now() + base::Hours(2);
  const auto c_origin = url::Origin::Create(GURL("https://c.net"));
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      new_tokens = {
          private_verification_tokens::PrivateVerificationTokensToken(
              c_origin, std::vector<uint8_t>{10, 11}, 3, expiration, 1),
      };

  base::test::TestFuture<void> store_future;
  service->StoreTokens(std::move(new_tokens), store_future.GetCallback());
  EXPECT_TRUE(store_future.Wait());

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      get_future;
  service->GetTokens(get_future.GetCallback());

  auto tokens = get_future.Take();
  auto expected_tokens = CreateTestTokens();
  expected_tokens.emplace_back(c_origin, std::vector<uint8_t>{10, 11}, 3,
                               expiration, 1);
  VerifyTokens(tokens, expected_tokens);
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    StoreTokens_WhenAntiAbuseSettingBlockedForOrigin_FiltersOriginTokens) {
  Profile* profile = GetProfile();

  const auto c_origin = url::Origin::Create(GURL("https://c.net"));
  const auto d_origin = url::Origin::Create(GURL("https://d.com"));

  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(c_origin.GetURL(), c_origin.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_BLOCK);

  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  const auto expiration = base::Time::Now() + base::Hours(2);
  std::vector<private_verification_tokens::PrivateVerificationTokensToken>
      new_tokens = {
          private_verification_tokens::PrivateVerificationTokensToken(
              c_origin, std::vector<uint8_t>{10, 11}, 3, expiration, 1),
          private_verification_tokens::PrivateVerificationTokensToken(
              d_origin, std::vector<uint8_t>{12, 13}, 4, expiration, 1),
      };

  base::test::TestFuture<void> store_future;
  service->StoreTokens(std::move(new_tokens), store_future.GetCallback());
  EXPECT_TRUE(store_future.Wait());

  // Reset setting for c.net so GetTokens includes it if present, allowing us to
  // verify that StoreTokens dropped c.net's token.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(c_origin.GetURL(), c_origin.GetURL(),
                                      ContentSettingsType::ANTI_ABUSE,
                                      CONTENT_SETTING_ALLOW);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      get_future;
  service->GetTokens(get_future.GetCallback());

  auto tokens = get_future.Take();
  auto expected_tokens = CreateTestTokens();
  expected_tokens.emplace_back(d_origin, std::vector<uint8_t>{12, 13}, 4,
                               expiration, 1);
  VerifyTokens(tokens, expected_tokens);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokens_WhenShuttingDown_ReturnsEmpty) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  WaitForInitialization(service);
  service->Shutdown();

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  EXPECT_TRUE(tokens.empty());
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokenIssuers_WhenShuttingDown_ReturnsEmpty) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  WaitForInitialization(service);
  service->Shutdown();

  base::test::TestFuture<std::vector<url::Origin>> future;
  service->GetTokenIssuers(future.GetCallback());

  auto tokens = future.Take();
  EXPECT_TRUE(tokens.empty());
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       BindReceiver_Success) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  mojo::Remote<
      private_verification_tokens::mojom::PrivateVerificationTokensProvider>
      remote;
  service->BindReceiver(remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  remote->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  auto expected_tokens = CreateTestTokens();
  VerifyTokens(tokens, expected_tokens);
}

class PrivateVerificationTokensServiceDisabledBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceDisabledBrowserTest,
                       GetForProfile_FeatureDisabled_ReturnsNull) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(service);
}

class PrivateVerificationTokensServiceEmptyDatabaseBrowserTest
    : public PrivateVerificationTokensServiceBrowserTest {
 public:
  void PrepopulateDatabase() override { StoreInDatabase(db_path_, {}); }
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceEmptyDatabaseBrowserTest,
                       GetTokens_EmptyDbFile_ReturnsEmpty) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  EXPECT_TRUE(tokens.empty());
}

class PrivateVerificationTokensServiceNoDatabaseFileBrowserTest
    : public PrivateVerificationTokensServiceBrowserTest {
 public:
  void PrepopulateDatabase() override {
    // Do nothing.
  }
};

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceNoDatabaseFileBrowserTest,
    GetTokens_NoDbFile_ReturnsEmpty) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  auto tokens = future.Take();
  EXPECT_TRUE(tokens.empty());
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokenIssuers_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  base::test::TestFuture<std::vector<url::Origin>> future;
  service->GetTokenIssuers(future.GetCallback());

  auto issuers = future.Take();
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       DeleteTokens_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  // Verify tokens exist first.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_EQ(issuers_future.Get().size(), 2u);

  // Delete tokens for a.com.
  base::test::TestFuture<void> delete_future;
  service->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});
  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future2;
  service->GetTokenIssuers(issuers_future2.GetCallback());
  auto issuers = issuers_future2.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokens_PendingBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  // The callback should not have run yet because it's pending initialization.
  EXPECT_FALSE(future.IsReady());

  // Now wait for initialization. This should trigger the pending callback.
  WaitForInitialization(service);

  // The callback should now have run.
  auto tokens = future.Take();
  auto expected_tokens = CreateTestTokens();
  VerifyTokens(tokens, expected_tokens);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokenIssuers_PendingBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<url::Origin>> future;
  service->GetTokenIssuers(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  WaitForInitialization(service);

  auto issuers = future.Take();
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           url::Origin::Create(GURL("https://a.com")),
                           url::Origin::Create(GURL("https://b.org"))));
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       DeleteTokens_PendingBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> delete_future;
  service->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service);

  EXPECT_TRUE(delete_future.Wait());

  // Verify deletion worked.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetTokens_PendingShutdownBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>>
      future;
  service->GetTokens(future.GetCallback());

  // The callback should not have run yet because it's pending initialization.
  EXPECT_FALSE(future.IsReady());

  // Shut down the service before initialization; this should clear the
  // callbacks.
  service->Shutdown();

  // The callback should now have run, but with an empty result since we never
  // got a chance to initialize our DB.
  auto tokens = future.Take();
  EXPECT_EQ(tokens.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    GetTokenIssuers_PendingShutdownBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<std::vector<url::Origin>> future;
  service->GetTokenIssuers(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Shut down the service before initialization; this should clear the
  // callbacks.
  service->Shutdown();

  // The callback should now have run, but with an empty result since we never
  // got a chance to initialize our DB.
  auto issuers = future.Take();
  EXPECT_EQ(issuers.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    DeleteTokens_PendingShutdownBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> delete_future;
  service->DeleteTokens(
      base::Time::Min(), base::Time::Max(), delete_future.GetCallback(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))});

  EXPECT_FALSE(delete_future.IsReady());

  // Shut down the service before initialization; this should clear the
  // callbacks.
  service->Shutdown();

  // Verify that the callback is run even though we couldn't have deleted the
  // tokens without an initialized store.
  EXPECT_TRUE(delete_future.Wait());
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       DeleteTokensByFilter_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  // Verify tokens exist first.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_EQ(issuers_future.Get().size(), 2u);

  // Delete tokens for a.com.
  base::test::TestFuture<void> delete_future;
  base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter =
      base::BindRepeating([](const blink::StorageKey& key) {
        return key == blink::StorageKey::CreateFirstParty(
                          url::Origin::Create(GURL("https://a.com")));
      });

  service->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                storage_key_filter,
                                delete_future.GetCallback());

  EXPECT_TRUE(delete_future.Wait());

  // Verify only b.org remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future2;
  service->GetTokenIssuers(issuers_future2.GetCallback());
  auto issuers = issuers_future2.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    DeleteTokensByFilter_PendingBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> delete_future;

  base::RepeatingCallback<bool(const blink::StorageKey&)> storage_key_filter =
      base::BindRepeating([](const blink::StorageKey& key) {
        return key == blink::StorageKey::CreateFirstParty(
                          url::Origin::Create(GURL("https://a.com")));
      });

  service->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                storage_key_filter,
                                delete_future.GetCallback());

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service);

  EXPECT_TRUE(delete_future.Wait());

  // Verify deletion worked.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 1u);
  EXPECT_EQ(issuers[0], url::Origin::Create(GURL("https://b.org")));
}

IN_PROC_BROWSER_TEST_F(
    PrivateVerificationTokensServiceBrowserTest,
    DeleteTokensByFilter_NullFilterPendingBeforeInitialization_Success) {
  Profile* profile = GetProfile();
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> delete_future;

  auto storage_key_filter =
      base::RepeatingCallback<bool(const blink::StorageKey&)>();

  service->DeleteTokensByFilter(base::Time::Min(), base::Time::Max(),
                                storage_key_filter,
                                delete_future.GetCallback());

  EXPECT_FALSE(delete_future.IsReady());

  WaitForInitialization(service);

  EXPECT_TRUE(delete_future.Wait());

  // Verify deletion worked.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  auto issuers = issuers_future.Take();
  EXPECT_EQ(issuers.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       SetIssuerConfig_UpdatesService) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->issuer_config());

  base::DictValue dict;
  dict.Set(private_verification_tokens::kIssuersKey, base::ListValue());
  auto config = private_verification_tokens::
      PrivateVerificationTokensIssuerConfig::Create(std::move(dict));
  ASSERT_TRUE(config);

  service->SetIssuerConfig(config);
  EXPECT_EQ(service->issuer_config(), config);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GlobalIssuerConfig_PropagatesToFactoryAndProfiles) {
  base::DictValue dict;
  dict.Set(private_verification_tokens::kIssuersKey, base::ListValue());
  auto config = private_verification_tokens::
      PrivateVerificationTokensIssuerConfig::Create(std::move(dict));
  ASSERT_TRUE(config);

  PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(config);
  EXPECT_EQ(PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig(),
            config);

  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);
  EXPECT_EQ(service->issuer_config(), config);
}

class PrivateVerificationTokensServiceCustomIssuerBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceCustomIssuerBrowserTest() {
    const std::vector<uint8_t> serialized_public_key = {1, 2, 3, 4};
    const std::string encoded_public_key =
        base::Base64Encode(serialized_public_key);
    const std::vector<uint8_t> serialized_proof = {5, 6, 7};
    const std::string encoded_proof = base::Base64Encode(serialized_proof);
    const std::string custom_issuer_json = base::StringPrintf(
        R"({
          "issuerRequestUrl": "https://commandline.example.com/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 7,
          "expiration": "2147483647",
          "redeemers": ["https://s1.commandline.example.com"],
          "deploymentId": "cmd-deployment-id"
        })",
        encoded_public_key.c_str(), encoded_proof.c_str());

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnablePrivateVerificationTokens,
        {{net::features::kPrivateVerificationTokensCustomIssuer.name,
          custom_issuer_json}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceCustomIssuerBrowserTest,
                       GlobalIssuerConfig_CustomIssuerFromCommandLine) {
  PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(nullptr);
  auto config =
      PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig();
  ASSERT_TRUE(config);
  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://commandline.example.com"));
  EXPECT_TRUE(config->config().contains(expected_origin));

  const auto& issuer_config = config->config().at(expected_origin);
  EXPECT_EQ(issuer_config.issuer_request_url,
            GURL("https://commandline.example.com/issue"));
  EXPECT_EQ(issuer_config.batch_size, 7);
  EXPECT_EQ(issuer_config.deployment_id, "cmd-deployment-id");
  EXPECT_THAT(issuer_config.redeemers,
              testing::ElementsAre(url::Origin::Create(
                  GURL("https://s1.commandline.example.com"))));

  const private_verification_tokens::PrivateVerificationTokensPublicKey
      expected_public_key{expected_origin,
                          {1, 2, 3, 4},
                          {5, 6, 7},
                          base::Time::UnixEpoch() + base::Seconds(2147483647),
                          1};
  EXPECT_EQ(issuer_config.public_key, expected_public_key);
}

}  // namespace
