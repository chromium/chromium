// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/webview_auth_handler.h"

#include <string>

#include "ash/webui/boca_ui/webview_auth_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
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
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::boca {

namespace {

constexpr char kTestAccountEmail[] = "user@test.com";
constexpr char kWebviewHostName[] = "boca";

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
  };
}

}  // namespace

// WebviewAuthDelegate for tests that allows to inject identity manager and
// storage partition.
class TestWebviewAuthDelegate : public WebviewAuthDelegate {
 public:
  explicit TestWebviewAuthDelegate(
      signin::IdentityManager* identity_manager,
      network::TestURLLoaderFactory* url_loader_factory)
      : test_identity_manager_(identity_manager) {
    test_storage_partition_.set_cookie_manager_for_browser_process(
        &test_cookie_manager_);
    test_storage_partition_.set_url_loader_factory_for_browser_process(
        url_loader_factory);
  }

  ~TestWebviewAuthDelegate() override = default;

  // WebviewAuthDelegateImpl:
  signin::IdentityManager* GetIdentityManager() override {
    return test_identity_manager_;
  }

  content::StoragePartition* GetStoragePartition(
      content::BrowserContext* context,
      const content::StoragePartitionConfig& storage_partition_config)
      override {
    test_storage_partition_.set_config(storage_partition_config);
    return &test_storage_partition_;
  }

 private:
  content::TestStoragePartition test_storage_partition_;
  network::TestCookieManager test_cookie_manager_;

  const raw_ptr<signin::IdentityManager> test_identity_manager_;
};

class BocaWebviewAuthHandlerTest : public testing::Test {
 public:
  BocaWebviewAuthHandlerTest() = default;
  BocaWebviewAuthHandlerTest(const BocaWebviewAuthHandlerTest&) = delete;
  BocaWebviewAuthHandlerTest& operator=(const BocaWebviewAuthHandlerTest&) =
      delete;

  ~BocaWebviewAuthHandlerTest() override {}

 protected:
  WebviewAuthHandler& auth_handler() { return auth_handler_; }

  void QueueResponseForResult(signin::SetAccountsInCookieResult result) {
    test_signin_client_.GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()
            ->oauth_multilogin_url()
            .Resolve(base::StringPrintf("?source=%s&reuseCookies=0",
                                        GaiaConstants::kChromeOSSource))
            .spec(),
        GetResponseFromResult(result));
  }

  void SetUp() override {
    testing::Test::SetUp();

    identity_test_env_.MakePrimaryAccountAvailable(
        kTestAccountEmail, signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext test_context_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient test_signin_client_{&prefs_};
  signin::IdentityTestEnvironment identity_test_env_{
      /*test_url_loader_factory=*/nullptr, &prefs_, &test_signin_client_};

  WebviewAuthHandler auth_handler_{
      std::make_unique<TestWebviewAuthDelegate>(
          identity_test_env_.identity_manager(),
          test_signin_client_.GetTestURLLoaderFactory()),
      &test_context_, kWebviewHostName};
};

TEST_F(BocaWebviewAuthHandlerTest, AuthSuccess) {
  base::RunLoop run_loop;

  QueueResponseForResult(signin::SetAccountsInCookieResult::kSuccess);

  auth_handler().AuthenticateWebview(
      base::BindLambdaForTesting([&](bool is_success) -> void {
        EXPECT_TRUE(is_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BocaWebviewAuthHandlerTest, AuthPersistentFailure) {
  base::RunLoop run_loop;

  QueueResponseForResult(signin::SetAccountsInCookieResult::kPersistentError);

  auth_handler().AuthenticateWebview(
      base::BindLambdaForTesting([&](bool is_success) -> void {
        EXPECT_FALSE(is_success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace ash::boca
