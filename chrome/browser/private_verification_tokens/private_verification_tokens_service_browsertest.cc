// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/private_verification_tokens/common/private_verification_tokens_database.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "components/private_verification_tokens/mojom/private_verification_tokens_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
      base::Time::Min(), base::Time::Max(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))},
      delete_future.GetCallback());
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
      base::Time::Min(), base::Time::Max(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))},
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
      base::Time::Min(), base::Time::Max(),
      std::vector<url::Origin>{url::Origin::Create(GURL("https://a.com"))},
      delete_future.GetCallback());

  EXPECT_FALSE(delete_future.IsReady());

  // Shut down the service before initialization; this should clear the
  // callbacks.
  service->Shutdown();

  // Verify that the callback is run even though we couldn't have deleted the
  // tokens without an initialized store.
  EXPECT_TRUE(delete_future.Wait());
}

}  // namespace
