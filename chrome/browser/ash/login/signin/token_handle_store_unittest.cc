// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/signin/token_handle_store_impl.h"
#include "chrome/browser/ui/webui/signin/ash/signin_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "crypto/obsolete/sha1.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakeToken[] = "fake-token";
constexpr char kFakeOtherToken[] = "fake-other-token";
constexpr char kFakeEmail[] = "fake-email@example.com";
constexpr char kFakeOtherEmail[] = "fake-other-email@example.com";
constexpr GaiaId::Literal kFakeGaiaId("fake-gaia-id");
constexpr GaiaId::Literal kFakeOtherGaiaId("fake-other-gaia-id");
constexpr char kFakeAccessToken[] = "fake-access-token";
constexpr char kFakeRefreshTokenHash[] = "fake-refresh-token-hash";

constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";
constexpr char kTokenHandleStatusInvalid[] = "invalid";
constexpr char kTokenHandleStatusValid[] = "valid";
constexpr char kTokenHandleStatusStale[] = "stale";

constexpr char kKnownUsersPref[] = "KnownUsers";

constexpr char kValidTokenInfoResponse[] =
    R"(
      { "email": "%s",
        "user_id": "1234567890",
        "expires_in": %d
      }
   )";
constexpr char kTokenFetchResponse[] =
    R"(
      { "email": "%s",
        "user_id": "1234567890",
        "token_handle": "%s"
      }
   )";

constexpr char kTokenHandleMap[] = "ash.token_handle_map_post_refactoring";

constexpr base::TimeDelta kCacheStatusTime = base::Hours(1);

std::string GetValidTokenInfoResponse(const std::string& email,
                                      int expires_in) {
  return base::StringPrintf(kValidTokenInfoResponse, email, expires_in);
}

std::string GetTokenInfoFetchResponse(const std::string& email,
                                      const std::string& token) {
  return base::StringPrintf(kTokenFetchResponse, email, token);
}

void AssertLastCheckedTimestampWithinTolerance(
    std::unique_ptr<user_manager::KnownUser> known_user,
    const AccountId& account_id,
    base::Time expected_last_checked_timestamp,
    base::TimeDelta tolerance) {
  base::Time actual_last_checked_time =
      base::ValueToTime(
          *known_user->FindPath(account_id, kTokenHandleLastCheckedPref))
          .value();

  EXPECT_NEAR(expected_last_checked_timestamp.InMillisecondsSinceUnixEpoch(),
              actual_last_checked_time.InMillisecondsSinceUnixEpoch(),
              tolerance.InMilliseconds());
}

}  // namespace

class TokenHandleStoreTest : public ::testing::Test {
 public:
  using TokenValidationFuture =
      base::test::TestFuture<const AccountId&, const std::string&, bool>;

  TokenHandleStoreTest() = default;
  ~TokenHandleStoreTest() override = default;

  void SetUp() override {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    local_state_.registry()->RegisterDictionaryPref(kTokenHandlePref);
  }

  std::unique_ptr<TokenHandleStore> CreateTokenHandleStore(
      std::unique_ptr<user_manager::KnownUser> known_user,
      bool user_has_gaia_password = true) {
    auto does_user_have_gaia_password =
        [user_has_gaia_password](
            const AccountId& account_id,
            base::OnceCallback<void(std::optional<bool>)> continuation) {
          std::move(continuation).Run(user_has_gaia_password);
        };
    return std::make_unique<TokenHandleStoreImpl>(
        std::move(known_user),
        base::BindLambdaForTesting(std::move(does_user_have_gaia_password)));
  }

 protected:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::UserManager> user_manager_;
};

TEST_F(TokenHandleStoreTest, HasTokenReturnsTrueWhenTokenIsOnDisk) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, true);
}

TEST_F(TokenHandleStoreTest, HasTokenReturnsFalseWhenTokenIsEmpty) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, std::string());
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, false);
}

TEST_F(TokenHandleStoreTest, HasTokenReturnsFalseWhenNoTokenOnDisk) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool has_token = token_handle_store->HasToken(account_id);

  EXPECT_EQ(has_token, false);
}

TEST_F(TokenHandleStoreTest,
       ShouldObtainHandleReturnsTrueIfTokenExsitsAndIsInvalid) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id, kTokenHandleStatusPref,
                            kTokenHandleStatusInvalid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, true);
}

TEST_F(TokenHandleStoreTest, ShouldObtainHandleReturnsTrueIfTokenDoesNotExist) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, true);
}

TEST_F(TokenHandleStoreTest,
       ShouldObtainHandleReturnsFalseIfTokenExistsAndIsValid) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool should_obtain_handle =
      token_handle_store->ShouldObtainHandle(account_id);

  EXPECT_EQ(should_obtain_handle, false);
}

TEST_F(TokenHandleStoreTest, IsRecentlyCheckedReturnsFalseIfNeverChecked) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, false);
}

TEST_F(TokenHandleStoreTest,
       IsRecentlyCheckedReturnsTrueIfWithinCacheStatusTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                      base::TimeToValue(base::Time::Now()));
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  mock_clock.Advance(kCacheStatusTime - base::Minutes(1));

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, true);
}

TEST_F(TokenHandleStoreTest,
       IsRecentlyCheckedReturnsFalseIfNotWithinCacheStatusTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                      base::TimeToValue(base::Time::Now()));
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  mock_clock.Advance(kCacheStatusTime * 2);

  bool is_recently_checked = token_handle_store->IsRecentlyChecked(account_id);

  EXPECT_EQ(is_recently_checked, false);
}

TEST_F(TokenHandleStoreTest,
       TokenHandleIsStoredWithCorrectStatusAndLastCheckedTime) {
  AccountId account_id = AccountId::FromUserEmail(kFakeEmail);
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  base::ScopedMockClockOverride mock_clock;
  base::Time previous_last_checked = base::Time::Now();
  injected_known_user->SetPath(account_id, kTokenHandleLastCheckedPref,
                               base::TimeToValue(previous_last_checked));
  base::TimeDelta delta = base::Seconds(1);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  mock_clock.Advance(delta);

  token_handle_store->StoreTokenHandle(account_id, kFakeToken);

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_EQ(kFakeToken,
            *known_user->FindStringPath(account_id, kTokenHandlePref));
  EXPECT_EQ(kTokenHandleStatusValid,
            *known_user->FindStringPath(account_id, kTokenHandleStatusPref));
  AssertLastCheckedTimestampWithinTolerance(
      std::move(known_user), account_id,
      /*expected_last_checked_timestamp*/ previous_last_checked + delta,
      /*tolerance=*/delta / 2);
}

class TokenHandleStoreIsReauthRequiredTest : public TokenHandleStoreTest {
 public:
  TokenHandleStoreIsReauthRequiredTest() = default;
  ~TokenHandleStoreIsReauthRequiredTest() override = default;

  void SetUp() override {
    account_id_ = AccountId::FromUserEmailGaiaId(kFakeEmail, kFakeGaiaId);
    other_account_id_ =
        AccountId::FromUserEmailGaiaId(kFakeOtherEmail, kFakeOtherGaiaId);
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    user_manager::TestHelper::RegisterPersistedUser(local_state_, account_id_);
    user_manager::TestHelper::RegisterPersistedUser(local_state_,
                                                    other_account_id_);
    local_state_.registry()->RegisterDictionaryPref(kTokenHandlePref);

    cros_settings_ = std::make_unique<ash::CrosSettings>();
    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        &local_state_, cros_settings_.get());

    user_manager_->Initialize();
  }

  void TearDown() override {
    user_manager_->Destroy();
    user_manager_.reset();
    cros_settings_.reset();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return url_loader_factory_.GetSafeWeakWrapper();
  }

  void AddFakeCheckResponseForStatus(TokenHandleChecker::Status status) {
    switch (status) {
      case TokenHandleChecker::Status::kValid:
        AddFakeResponse(
            GetValidTokenInfoResponse(kFakeEmail, /*expires_in=*/1000),
            net::HTTP_OK);
        return;
      case TokenHandleChecker::Status::kInvalid:
        AddFakeResponse(
            GetValidTokenInfoResponse(kFakeEmail, /*expires_in=*/1000),
            net::HTTP_UNAUTHORIZED);
        return;
      case TokenHandleChecker::Status::kExpired:
        AddFakeResponse(
            GetValidTokenInfoResponse(kFakeEmail, /*expires_in=*/-1),
            net::HTTP_OK);
        return;
      case TokenHandleChecker::Status::kUnknown:
        AddFakeResponse(std::string(), net::HTTP_OK);
        return;
    }
  }

  void AddFakeResponse(const std::string& response,
                       net::HttpStatusCode http_status) {
    url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_info_url().spec(), response,
        http_status);
  }

 protected:
  AccountId account_id_;
  AccountId other_account_id_;
  std::unique_ptr<user_manager::UserManager> user_manager_;
  std::unique_ptr<ash::CrosSettings> cros_settings_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredReturnsFalseIfTokenDoesNotExist) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(std::string(), future.Get<std::string>());
  EXPECT_EQ(false, future.Get<bool>());
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredReturnsTrueIfTokenExistsAndInvalid) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kInvalid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future.Get<std::string>());
  EXPECT_EQ(true, future.Get<bool>());
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredReturnsFalseIfTokenExistsAndValid) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kValid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future.Get<std::string>());
  EXPECT_EQ(false, future.Get<bool>());
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredReturnsFalseIfTokenExistsAndStatusUnknown) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kUnknown);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future.Get<std::string>());
  EXPECT_EQ(false, future.Get<bool>());
}

TEST_F(
    TokenHandleStoreIsReauthRequiredTest,
    IsReauthRequiredReturnsTrueIfUserHasGaiaPasswordAndTokenExistsAndExpired) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kExpired);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future.Get<std::string>());
  EXPECT_EQ(true, future.Get<bool>());
}

TEST_F(
    TokenHandleStoreIsReauthRequiredTest,
    IsReauthRequiredReturnsFalseIfUserHasNoGaiaPasswordAndTokenExistsAndExpired) {
  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                            kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kExpired);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(known_user),
                             /*user_has_gaia_password=*/false);
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_EQ(account_id_, future.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future.Get<std::string>());
  EXPECT_EQ(false, future.Get<bool>());
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredSetsStatusPrefToInvalidWhenTokenInvalid) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kInvalid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(kTokenHandleStatusInvalid,
            *known_user->FindStringPath(account_id_, kTokenHandleStatusPref));
}

TEST_F(
    TokenHandleStoreIsReauthRequiredTest,
    IsReauthRequiredSetsStatusPrefToInvalidWhenTokenExpiredAndUserHasGaiaPassword) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kExpired);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(kTokenHandleStatusInvalid,
            *known_user->FindStringPath(account_id_, kTokenHandleStatusPref));
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       IsReauthRequiredSetsLastCheckedPrefTimestampWhenStatusIsNotUnknown) {
  base::ScopedMockClockOverride mock_clock;
  base::Time previous_last_checked = base::Time::Now();
  base::TimeDelta delta = base::Seconds(1);
  mock_clock.Advance(delta);
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusValid);
  AddFakeCheckResponseForStatus(TokenHandleChecker::Status::kValid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  TokenValidationFuture future;

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future.GetCallback());

  EXPECT_TRUE(future.Wait());
  AssertLastCheckedTimestampWithinTolerance(
      std::make_unique<user_manager::KnownUser>(&local_state_), account_id_,
      /*expected_last_checked_timestamp=*/previous_last_checked + delta,
      /*tolerance=*/delta / 2);
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       ConcurrentIsReauthRequiredRequestsForTheSameAccountIdGetPooled) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusValid);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  TokenValidationFuture future1;
  TokenValidationFuture future2;
  const GURL& url = GaiaUrls::GetInstance()->oauth2_token_info_url();

  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future1.GetCallback());
  url_loader_factory_.WaitForRequest(url);
  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future2.GetCallback());

  // Reply to the second request, which should reply to both pending concurrent
  // requests.
  url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), GetValidTokenInfoResponse(kFakeEmail, /*expires_in=*/1000),
      net::HTTP_OK);

  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
  EXPECT_EQ(account_id_, future1.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future1.Get<std::string>());
  EXPECT_EQ(false, future1.Get<bool>());
  EXPECT_EQ(account_id_, future2.Get<AccountId>());
  EXPECT_EQ(kFakeToken, future2.Get<std::string>());
  EXPECT_EQ(false, future2.Get<bool>());
}

TEST_F(TokenHandleStoreIsReauthRequiredTest,
       ConcurrentIsReauthRequiredRequestsForDifferentAccountIdsDoNotGetPooled) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusValid);
  injected_known_user->SetStringPref(other_account_id_, kTokenHandlePref,
                                     kFakeOtherToken);
  injected_known_user->SetStringPref(other_account_id_, kTokenHandlePref,
                                     kFakeOtherToken);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  TokenValidationFuture future1;
  TokenValidationFuture future2;
  const GURL& url = GaiaUrls::GetInstance()->oauth2_token_info_url();

  token_handle_store->IsReauthRequired(
      other_account_id_, GetSharedURLLoaderFactory(), future1.GetCallback());
  url_loader_factory_.WaitForRequest(url);
  token_handle_store->IsReauthRequired(account_id_, GetSharedURLLoaderFactory(),
                                       future2.GetCallback());

  // Reply to only second request, leaving the first one pending.
  url_loader_factory_.SimulateResponseForPendingRequest(
      url.spec(), GetValidTokenInfoResponse(kFakeEmail, /*expires_in=*/1000),
      net::HTTP_OK, network::TestURLLoaderFactory::kMostRecentMatch);

  EXPECT_TRUE(future2.Wait());
  // As we only replied to the check for `account_id_`, the check for
  // `other_account_id_` should still be pending.
  EXPECT_FALSE(future1.IsReady());
}

class TokenHandleStoreMaybeFetchTokenHandleTest
    : public TokenHandleStoreIsReauthRequiredTest {
 public:
  TokenHandleStoreMaybeFetchTokenHandleTest() = default;
  ~TokenHandleStoreMaybeFetchTokenHandleTest() override = default;

  void SetUp() override {
    TokenHandleStoreIsReauthRequiredTest::SetUp();
    token_handle_mapping_store_.registry()->RegisterDictionaryPref(
        /*path=*/kTokenHandleMap);
  }

 protected:
  TestingPrefServiceSimple token_handle_mapping_store_;
};

TEST_F(TokenHandleStoreMaybeFetchTokenHandleTest,
       MaybeFetchTokenHandleExecutesFetchForUserIfTokenHandleDoesNotExist) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  AddFakeResponse(GetTokenInfoFetchResponse(kFakeEmail, kFakeOtherToken),
                  net::HTTP_OK);

  token_handle_store->MaybeFetchTokenHandle(
      &token_handle_mapping_store_, GetSharedURLLoaderFactory(), account_id_,
      kFakeAccessToken, kFakeRefreshTokenHash);
  local_state_.user_prefs_store()->WaitUntilValueChanges(kKnownUsersPref);

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_EQ(kFakeOtherToken,
            *known_user->FindStringPath(account_id_, kTokenHandlePref));
  EXPECT_EQ(kTokenHandleStatusValid,
            *known_user->FindStringPath(account_id_, kTokenHandleStatusPref));
}

TEST_F(TokenHandleStoreMaybeFetchTokenHandleTest,
       MaybeFetchTokenHandleExecutesFetchForUserIfTokenHandleIsStale) {
  auto injected_known_user =
      std::make_unique<user_manager::KnownUser>(&local_state_);
  injected_known_user->SetStringPref(account_id_, kTokenHandlePref, kFakeToken);
  injected_known_user->SetStringPref(account_id_, kTokenHandleStatusPref,
                                     kTokenHandleStatusStale);
  std::unique_ptr<TokenHandleStore> token_handle_store =
      CreateTokenHandleStore(std::move(injected_known_user));
  AddFakeResponse(GetTokenInfoFetchResponse(kFakeEmail, kFakeOtherToken),
                  net::HTTP_OK);

  token_handle_store->MaybeFetchTokenHandle(
      &token_handle_mapping_store_, GetSharedURLLoaderFactory(), account_id_,
      kFakeAccessToken, kFakeRefreshTokenHash);
  local_state_.user_prefs_store()->WaitUntilValueChanges(kKnownUsersPref);

  auto known_user = std::make_unique<user_manager::KnownUser>(&local_state_);
  EXPECT_EQ(kFakeOtherToken,
            *known_user->FindStringPath(account_id_, kTokenHandlePref));
  EXPECT_EQ(kTokenHandleStatusValid,
            *known_user->FindStringPath(account_id_, kTokenHandleStatusPref));
}

class TokenHandleStoreHistogramTest
    : public TokenHandleStoreMaybeFetchTokenHandleTest {
 public:
  void SetUp() override {
    TokenHandleStoreMaybeFetchTokenHandleTest::SetUp();
    account_manager::AccountManager::RegisterPrefs(local_state_.registry());
  }

  void CreateAndInitializeAccountManager() {
    account_manager_ = std::make_unique<account_manager::AccountManager>();
    EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir());
    account_manager_->Initialize(
        tmp_dir_.GetPath(), GetSharedURLLoaderFactory(),
        base::BindRepeating([](base::OnceClosure closure) -> void {
          std::move(closure).Run();
        }));
    account_manager_->SetPrefService(&local_state_);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(account_manager_->IsInitialized());
  }

  // Returns a Base16 encoded SHA1 digest of `data`.
  std::string Sha1Digest(const std::string& data) {
    return base::HexEncode(
        crypto::obsolete::Sha1::HashForTesting(base::as_byte_span(data)));
  }

 protected:
  const ::account_manager::AccountKey kGaiaAccountKey = {
      "fake-gaia-id", ::account_manager::AccountType::kGaia};
  std::unique_ptr<account_manager::AccountManager> account_manager_;
  base::ScopedTempDir tmp_dir_;
};

TEST_F(TokenHandleStoreHistogramTest,
       DiagnoseTokenHandleMappingRecordsTrueWhenHashesMatch) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TokenHandleStore> token_handle_store = CreateTokenHandleStore(
      std::make_unique<user_manager::KnownUser>(&local_state_));

  CreateAndInitializeAccountManager();
  account_manager_->UpsertAccount(kGaiaAccountKey, kFakeEmail, kFakeToken);

  base::Value::Dict token_handle_map_dict;
  token_handle_map_dict.Set(kFakeToken, Sha1Digest(kFakeToken));
  token_handle_mapping_store_.SetDict(kTokenHandleMap,
                                      std::move(token_handle_map_dict));

  token_handle_store->DiagnoseTokenHandleMapping(&token_handle_mapping_store_,
                                                 account_manager_.get(),
                                                 account_id_, kFakeToken);

  histogram_tester.ExpectUniqueSample(
      "Login.IsTokenHandleInSyncWithRefreshTokenPostRefactoring", true, 1);
}

TEST_F(TokenHandleStoreHistogramTest,
       DiagnoseTokenHandleMappingRecordsFlaseWhenHashesDontMatch) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TokenHandleStore> token_handle_store = CreateTokenHandleStore(
      std::make_unique<user_manager::KnownUser>(&local_state_));

  CreateAndInitializeAccountManager();
  account_manager_->UpsertAccount(kGaiaAccountKey, kFakeEmail, kFakeToken);

  base::Value::Dict token_handle_map_dict;
  token_handle_map_dict.Set(kFakeToken, Sha1Digest(kFakeOtherToken));
  token_handle_mapping_store_.SetDict(kTokenHandleMap,
                                      std::move(token_handle_map_dict));

  token_handle_store->DiagnoseTokenHandleMapping(&token_handle_mapping_store_,
                                                 account_manager_.get(),
                                                 account_id_, kFakeToken);

  histogram_tester.ExpectUniqueSample(
      "Login.IsTokenHandleInSyncWithRefreshTokenPostRefactoring", false, 1);
}

}  // namespace ash
