// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"

#include <deque>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/token_handle_store.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestEmail[] = "test@example.com";
constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kFakeToken[] = "fake-token";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleStatusValid[] = "valid";
constexpr char kTokenInfoResponse[] =
    R"({
      "email": "%s",
      "user_id": "1234567890",
      "expires_in": %d
    })";

class CountingFakeUserDataAuthClient : public FakeUserDataAuthClient {
 public:
  CountingFakeUserDataAuthClient() = default;
  ~CountingFakeUserDataAuthClient() override = default;

  int list_auth_factors_count() const { return list_auth_factors_count_; }

  void ListAuthFactors(const ::user_data_auth::ListAuthFactorsRequest& request,
                       ListAuthFactorsCallback callback) override {
    list_auth_factors_count_++;
    pending_callbacks_.push_back(std::move(callback));
    pending_requests_.push_back(request);
  }

  void CompleteNextListAuthFactors() {
    CHECK(!pending_callbacks_.empty());
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop_front();
    auto request = pending_requests_.front();
    pending_requests_.pop_front();
    FakeUserDataAuthClient::ListAuthFactors(request, std::move(callback));
  }

 private:
  int list_auth_factors_count_ = 0;
  std::deque<ListAuthFactorsCallback> pending_callbacks_;
  std::deque<::user_data_auth::ListAuthFactorsRequest> pending_requests_;
};

}  // namespace

class TokenHandleStoreFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kUseTokenHandleStore);
    auto counting_client = std::make_unique<CountingFakeUserDataAuthClient>();
    counting_client_ = counting_client.get();
    FakeUserDataAuthClient::TestApi::OverrideGlobalInstance(
        std::move(counting_client));
    FakeUserDataAuthClient::InitializeFake();
    session_manager_.emplace(
        std::make_unique<session_manager::FakeSessionManagerDelegate>());
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_ = std::make_unique<TestingProfile>();

    token_handle_store_ = TokenHandleStoreFactory::Get()->GetTokenHandleStore();
  }

  void TearDown() override {
    token_handle_store_ = nullptr;
    TokenHandleStoreFactory::Get()->DestroyTokenHandleStore();
    profile_.reset();
    session_manager_.reset();
    fake_user_manager_.Reset();
    counting_client_ = nullptr;
    FakeUserDataAuthClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<session_manager::SessionManager> session_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<TokenHandleStore> token_handle_store_ = nullptr;
  network::TestURLLoaderFactory url_loader_factory_;
  raw_ptr<CountingFakeUserDataAuthClient> counting_client_ = nullptr;
};

TEST_F(TokenHandleStoreFactoryTest,
       GracefulHandlingWhenUserMissingDuringCheck) {
  const AccountId account_id = AccountId::FromUserEmail(kTestEmail);
  fake_user_manager_->AddUser(account_id);

  user_manager::KnownUser known_user(
      TestingBrowserProcess::GetGlobal()->local_state());
  known_user.SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                           kTokenHandleStatusValid);

  base::test::TestFuture<const AccountId&, const std::string&, bool> future;
  token_handle_store_->IsReauthRequired(
      account_id, url_loader_factory_.GetSafeWeakWrapper(),
      future.GetCallback());

  fake_user_manager_->RemoveUserFromList(account_id);
  const GURL& url = GaiaUrls::GetInstance()->oauth2_token_info_url();

  // Simulate the URL loader response to trigger the callback chain into
  // DoesUserHaveGaiaPassword::Run.
  url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), base::StringPrintf(kTokenInfoResponse, kTestEmail, 1000),
      net::HTTP_OK, network::TestURLLoaderFactory::kMostRecentMatch);

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(account_id, future.Get<AccountId>());
}

TEST_F(TokenHandleStoreFactoryTest, ConcurrentReauthRequestsArePooled) {
  const AccountId account_id = AccountId::FromUserEmail(kTestEmail);
  fake_user_manager_->AddUser(account_id);
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));

  user_manager::KnownUser known_user(
      TestingBrowserProcess::GetGlobal()->local_state());
  known_user.SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user.SetStringPref(account_id, kTokenHandleStatusPref,
                           kTokenHandleStatusValid);

  base::test::TestFuture<const AccountId&, const std::string&, bool> future1;
  base::test::TestFuture<const AccountId&, const std::string&, bool> future2;

  // First reauth check.
  token_handle_store_->IsReauthRequired(
      account_id, url_loader_factory_.GetSafeWeakWrapper(),
      future1.GetCallback());

  // Complete the first network request. This triggers the async DBus call
  // to check Gaia password.
  const GURL& url = GaiaUrls::GetInstance()->oauth2_token_info_url();
  url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), base::StringPrintf(kTokenInfoResponse, kTestEmail, 1000),
      net::HTTP_OK, network::TestURLLoaderFactory::kMostRecentMatch);

  // At this point, the DBus call for check 1 is in flight.

  // Start second reauth check. This will cancel the (already completed)
  // check 1 in TokenHandleStoreImpl, and start check 2.
  token_handle_store_->IsReauthRequired(
      account_id, url_loader_factory_.GetSafeWeakWrapper(),
      future2.GetCallback());

  // Complete the second network request. This triggers another call to
  // DoesUserHaveGaiaPassword::Run. Since the DBus call from step 2 is still
  // in flight, this second call should be pooled.
  url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), base::StringPrintf(kTokenInfoResponse, kTestEmail, 1000),
      net::HTTP_OK, network::TestURLLoaderFactory::kMostRecentMatch);

  // Now run the loop to allow the DBus response to propagate.
  // This should reply to both pooled requests.
  counting_client_->CompleteNextListAuthFactors();
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(account_id, future1.Get<AccountId>());
  EXPECT_EQ(account_id, future2.Get<AccountId>());

  // Verify that ListAuthFactors was only called ONCE for the pooled requests.
  EXPECT_EQ(1, counting_client_->list_auth_factors_count());
}

}  // namespace ash
