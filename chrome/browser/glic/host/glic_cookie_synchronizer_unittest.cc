// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

namespace {

constexpr char kTestAccountEmail[] = "user@test.com";

constexpr char kCookieResponseSuccess[] = R"(
    { "status": "OK",
      "cookies":[
        {
          "name":"CookieName",
          "value":"CookieValue",
          "domain":".google.com",
          "path":"/"
        }
      ]
    })";
constexpr char kCookieResponseRetry[] = R"(
    { "status": "RETRY",
      "cookies":[]
    })";
constexpr char kCookieResponseError[] = R"(
    { "status": "ERROR",
      "cookies":[]
    })";

// Returns HTTP request response that will lead to the given `result`.
std::string GetResponseFromResult(signin::SetAccountsInCookieResult result) {
  switch (result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      return kCookieResponseSuccess;
    case signin::SetAccountsInCookieResult::kTransientError:
      return kCookieResponseRetry;
    case signin::SetAccountsInCookieResult::kPersistentError:
      return kCookieResponseError;
  }
}

class GlicCookieSynchronizerWithTestPartition : public GlicCookieSynchronizer {
 public:
  GlicCookieSynchronizerWithTestPartition(
      content::BrowserContext* context,
      signin::IdentityManager* identity_manager,
      content::TestStoragePartition* test_storage_partition)
      : GlicCookieSynchronizer(context,
                               identity_manager,
                               /*use_for_fre=*/false),
        test_storage_partition_(test_storage_partition) {}

  content::TestStoragePartition* GetStoragePartition() override {
    return test_storage_partition_;
  }

 private:
  raw_ptr<content::TestStoragePartition> test_storage_partition_;
};

}  // namespace

class GlicCookieSynchronizerTest : public testing::Test {
 public:
  GlicCookieSynchronizerTest() = default;
  ~GlicCookieSynchronizerTest() override = default;

 protected:
  GlicCookieSynchronizer& cookie_synchronizer() { return cookie_synchronizer_; }

  // Sets the network response to the given result. Applies to all subsequent
  // network requests.
  void SetResponseForResult(signin::SetAccountsInCookieResult result) {
    test_signin_client_.GetTestURLLoaderFactory()->AddResponse(
        RequestURL().spec(), GetResponseFromResult(result));
  }

  GURL RequestURL() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().Resolve(
        base::StringPrintf("?source=%s&reuseCookies=0", "ChromiumGlic"));
  }

  void SetUp() override {
    testing::Test::SetUp();

    test_storage_partition_.set_cookie_manager_for_browser_process(
        &test_cookie_manager_);
    test_storage_partition_.set_url_loader_factory_for_browser_process(
        test_signin_client_.GetTestURLLoaderFactory());

    identity_test_env_.MakePrimaryAccountAvailable(
        kTestAccountEmail, signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  base::TimeDelta kCookieSyncDefaultTimeout =
      GlicCookieSynchronizer::kCookieSyncDefaultTimeout;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingProfile test_profile_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient test_signin_client_{&prefs_};
  signin::IdentityTestEnvironment identity_test_env_{
      /*test_url_loader_factory=*/nullptr, &prefs_, &test_signin_client_};

  content::TestStoragePartition test_storage_partition_;
  network::TestCookieManager test_cookie_manager_;

  GlicCookieSynchronizerWithTestPartition cookie_synchronizer_{
      &test_profile_, identity_test_env_.identity_manager(),
      &test_storage_partition_};
};

TEST_F(GlicCookieSynchronizerTest, AuthSuccess) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kSuccess);

  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  EXPECT_TRUE(result.Get());
}

TEST_F(GlicCookieSynchronizerTest, MultipleRequestsAtOnce) {
  base::test::TestFuture<bool> result, result2;
  SetResponseForResult(signin::SetAccountsInCookieResult::kSuccess);

  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result2.GetCallback());
  EXPECT_TRUE(result.Get());
  EXPECT_TRUE(result2.Get());
}

TEST_F(GlicCookieSynchronizerTest, AuthPersistentFailure) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kPersistentError);

  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  EXPECT_FALSE(result.Get());
}

TEST_F(GlicCookieSynchronizerTest, AuthTransientSuccessOnRetry) {
  // This test verifies that OAuthMultiloginHelper performs retries for us.
  base::test::TestFuture<bool> result;

  int request_count = 0;
  test_signin_client_.GetTestURLLoaderFactory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url != RequestURL()) {
          return;
        }
        SetResponseForResult(
            request_count++ == 0
                ? signin::SetAccountsInCookieResult::kTransientError
                : signin::SetAccountsInCookieResult::kSuccess);
      }));

  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  EXPECT_TRUE(result.Get());
}

TEST_F(GlicCookieSynchronizerTest, AuthTransientFailure_MaxRetry) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kTransientError);

  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  EXPECT_FALSE(result.Get());
}

TEST_F(GlicCookieSynchronizerTest, FailsOnTimeOut) {
  base::test::TestFuture<bool> result;
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  task_environment_.FastForwardBy(kCookieSyncDefaultTimeout -
                                  base::Milliseconds(10));
  EXPECT_FALSE(result.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_FALSE(result.Get());
}

TEST_F(GlicCookieSynchronizerTest, FailsMultipleOnTimeOut) {
  base::test::TestFuture<bool> result;
  base::test::TestFuture<bool> result2;
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result2.GetCallback());

  task_environment_.FastForwardBy(kCookieSyncDefaultTimeout);
  EXPECT_FALSE(result.Get());
  EXPECT_FALSE(result2.Get());
}

TEST_F(GlicCookieSynchronizerTest, WorksAfterTimeout) {
  base::test::TestFuture<bool> result;
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());

  task_environment_.FastForwardBy(kCookieSyncDefaultTimeout);
  EXPECT_FALSE(result.Get());

  result.Clear();
  SetResponseForResult(signin::SetAccountsInCookieResult::kSuccess);
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition(
      result.GetCallback());

  EXPECT_TRUE(result.Get());
}

}  // namespace glic
