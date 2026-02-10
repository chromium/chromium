// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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

namespace contextual_tasks {

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

class ContextualTasksCookieSynchronizerForTest
    : public ContextualTasksCookieSynchronizer {
 public:
  ContextualTasksCookieSynchronizerForTest(
      content::BrowserContext* context,
      signin::IdentityManager* identity_manager,
      content::TestStoragePartition* test_storage_partition)
      : ContextualTasksCookieSynchronizer(context, identity_manager),
        test_storage_partition_(test_storage_partition) {}

  content::StoragePartition* GetStoragePartition() override {
    return test_storage_partition_;
  }

  void SetCallback(base::OnceCallback<void(bool)> callback) {
    callback_ = std::move(callback);
  }

 protected:
  void CompleteAuth(bool is_success) override {
    ContextualTasksCookieSynchronizer::CompleteAuth(is_success);
    if (callback_) {
      std::move(callback_).Run(is_success);
    }
  }

 private:
  raw_ptr<content::TestStoragePartition> test_storage_partition_;
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

class ContextualTasksCookieSynchronizerTest : public testing::Test {
 public:
  ContextualTasksCookieSynchronizerTest() = default;
  ~ContextualTasksCookieSynchronizerTest() override = default;

 protected:
  ContextualTasksCookieSynchronizerForTest& cookie_synchronizer() {
    return cookie_synchronizer_;
  }

  // Sets the network response to the given result. Applies to all subsequent
  // network requests.
  void SetResponseForResult(signin::SetAccountsInCookieResult result) {
    test_signin_client_.GetTestURLLoaderFactory()->AddResponse(
        RequestURL().spec(), GetResponseFromResult(result));
  }

  GURL RequestURL() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().Resolve(
        base::StringPrintf("?source=%s&reuseCookies=0",
                           "ChromiumBrowsercontextual-tasks"));
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

    test_signin_client_.GetTestURLLoaderFactory()->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              LOG(ERROR) << "Requested URL: " << request.url.spec();
            }));
  }

  base::TimeDelta kCookieSyncDefaultTimeout =
      ContextualTasksCookieSynchronizer::kCookieSyncDefaultTimeout;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingProfile test_profile_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient test_signin_client_{&prefs_};
  signin::IdentityTestEnvironment identity_test_env_{
      /*test_url_loader_factory=*/nullptr, &prefs_, &test_signin_client_};

  content::TestStoragePartition test_storage_partition_;
  network::TestCookieManager test_cookie_manager_;

  ContextualTasksCookieSynchronizerForTest cookie_synchronizer_{
      &test_profile_, identity_test_env_.identity_manager(),
      &test_storage_partition_};
};

TEST_F(ContextualTasksCookieSynchronizerTest, AuthSuccess) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kSuccess);

  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();
  EXPECT_TRUE(result.Get());
}

TEST_F(ContextualTasksCookieSynchronizerTest, AuthPersistentFailure) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kPersistentError);

  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();
  EXPECT_FALSE(result.Get());
}

TEST_F(ContextualTasksCookieSynchronizerTest, AuthTransientSuccessOnRetry) {
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

  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();
  EXPECT_TRUE(result.Get());
}

TEST_F(ContextualTasksCookieSynchronizerTest, AuthTransientFailure_MaxRetry) {
  base::test::TestFuture<bool> result;
  SetResponseForResult(signin::SetAccountsInCookieResult::kTransientError);

  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();
  EXPECT_FALSE(result.Get());
}

TEST_F(ContextualTasksCookieSynchronizerTest, FailsOnTimeOut) {
  base::test::TestFuture<bool> result;
  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();

  task_environment_.FastForwardBy(kCookieSyncDefaultTimeout -
                                  base::Milliseconds(10));
  EXPECT_FALSE(result.IsReady());
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_FALSE(result.Get());
}

TEST_F(ContextualTasksCookieSynchronizerTest, WorksAfterTimeout) {
  base::test::TestFuture<bool> result;
  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();

  task_environment_.FastForwardBy(kCookieSyncDefaultTimeout);
  EXPECT_FALSE(result.Get());

  result.Clear();
  SetResponseForResult(signin::SetAccountsInCookieResult::kSuccess);

  cookie_synchronizer().SetCallback(result.GetCallback());
  cookie_synchronizer().CopyCookiesToWebviewStoragePartition();

  EXPECT_TRUE(result.Get());
}

}  // namespace contextual_tasks
