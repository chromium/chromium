// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/signin_helper_lacros.h"

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/consistency_cookie_manager.h"
#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockCookieManager
    : public testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD4(SetCanonicalCookie,
               void(const net::CanonicalCookie& cookie,
                    const GURL& source_url,
                    const net::CookieOptions& cookie_options,
                    SetCanonicalCookieCallback callback));

  MOCK_METHOD4(GetCookieList,
               void(const GURL& url,
                    const net::CookieOptions& cookie_options,
                    const net::CookiePartitionKeyCollection&
                        cookie_partition_key_collection,
                    GetCookieListCallback callback));
};

class TestAccountReconcilor : public AccountReconcilor {
 public:
  TestAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      account_manager::AccountManagerFacade* account_manager_facade)
      : AccountReconcilor(
            identity_manager,
            client,
            account_manager_facade,
            std::make_unique<signin::MirrorLandingAccountReconcilorDelegate>(
                identity_manager,
                client->GetInitialPrimaryAccount().has_value())) {}

  void SimulateSetCookiesFinished(
      const std::vector<CoreAccountId>& accounts_to_send) {
    OnSetAccountsInCookieCompleted(accounts_to_send,
                                   signin::SetAccountsInCookieResult::kSuccess);
  }
};

}  // namespace

class SigninHelperLacrosTest : public testing::Test {
 public:
  SigninHelperLacrosTest() {
    CHECK(testing_profile_manager_.SetUp());

    profile_path_ =
        testing_profile_manager_.profile_manager()->user_data_dir().AppendASCII(
            "Default");
    ProfileAttributesInitParams params;
    params.profile_path = profile_path();
    params.profile_name = u"ProfileName";
    storage()->AddProfile(std::move(params));

    // No account in the facade.
    EXPECT_CALL(mock_facade_, GetAccounts(testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            [](base::OnceCallback<void(
                   const std::vector<account_manager::Account>&)> callback) {
              std::move(callback).Run({});
            });

    testing_profile_manager_.SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(
            &mock_facade_, storage(),
            testing_profile_manager_.local_state()->Get()));

    // Configure the cookie manager.
    std::unique_ptr<MockCookieManager> mock_cookie_manager =
        std::make_unique<MockCookieManager>();
    cookie_manager_ = mock_cookie_manager.get();
    std::unique_ptr<net::CanonicalCookie> cookie =
        signin::ConsistencyCookieManager::CreateConsistencyCookie("dummy");
    net::CookieAccessResultList cookie_list = {
        {*cookie, net::CookieAccessResult()}};
    ON_CALL(*cookie_manager_, GetCookieList(GaiaUrls::GetInstance()->gaia_url(),
                                            testing::_, testing::_, testing::_))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [cookie_list](
                network::mojom::CookieManager::GetCookieListCallback callback) {
              std::move(callback).Run(cookie_list, {});
            })));

    signin_client_.set_cookie_manager(std::move(mock_cookie_manager));
    identity_test_env_.WaitForRefreshTokensLoaded();
    identity_test_env_.SetCookieAccounts({});
    ExpectCookieSet("Updating");
    ExpectCookieSet("Consistent");
    reconcilor_.Initialize(/*start_reconcile_if_tokens_available=*/true);
    WaitForConsistentReconcilorState();
  }

  ~SigninHelperLacrosTest() override { reconcilor_.Shutdown(); }

  // Returns a `SigninHelperLacros` instance. The `AccountProfileMapper` and
  // the `IdentityManager` are not connected to each other, and must be managed
  // separately.
  std::unique_ptr<SigninHelperLacros> CreateSigninHelperLacros(
      base::OnceCallback<void(const CoreAccountId&)> callback) {
    return std::make_unique<SigninHelperLacros>(
        profile_path_,
        testing_profile_manager_.profile_manager()->GetAccountProfileMapper(),
        identity_test_env_.identity_manager(),
        reconcilor_.GetConsistencyCookieManager(),
        account_manager::AccountManagerFacade::AccountAdditionSource::
            kOgbAddAccount,
        std::move(callback));
  }

  void ExpectCookieSet(const std::string& value) {
    EXPECT_CALL(*cookie_manager_, GetCookieList).Times(testing::AnyNumber());
    EXPECT_CALL(
        *cookie_manager_,
        SetCanonicalCookie(
            testing::AllOf(
                testing::Property(&net::CanonicalCookie::Name,
                                  "CHROME_ID_CONSISTENCY_STATE"),
                testing::Property(&net::CanonicalCookie::Value, value)),
            GaiaUrls::GetInstance()->gaia_url(), testing::_, testing::_))
        .Times(1);
  }

  MockCookieManager* cookie_manager() { return cookie_manager_; }

  account_manager::MockAccountManagerFacade* mock_facade() {
    return &mock_facade_;
  }

  const base::FilePath& profile_path() const { return profile_path_; }

  void WaitForConsistentReconcilorState() {
    while (reconcilor_.GetState() !=
           signin_metrics::AccountReconcilorState::kOk) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void SimulateSetCookiesFinished(
      const std::vector<CoreAccountId>& accounts_to_send) {
    reconcilor_.SimulateSetCookiesFinished(accounts_to_send);
  }

  ProfileAttributesStorage* storage() {
    return &testing_profile_manager_.profile_manager()
                ->GetProfileAttributesStorage();
  }

  AccountProfileMapper* mapper() {
    return testing_profile_manager_.profile_manager()
        ->GetAccountProfileMapper();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  content::BrowserTaskEnvironment task_environment;
  account_manager::MockAccountManagerFacade mock_facade_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::FilePath profile_path_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  TestSigninClient signin_client_{&prefs_, &test_url_loader_factory_};

  signin::IdentityTestEnvironment identity_test_env_{
      /*test_url_loader_factory=*/nullptr, &prefs_, &signin_client_};

  TestAccountReconcilor reconcilor_{identity_test_env_.identity_manager(),
                                    &signin_client_, &mock_facade_};

  raw_ptr<MockCookieManager> cookie_manager_ =
      nullptr;  // Owned by `signin_client_`.
};

// Checks that creating a deleting the helper updates the cookie and that the
// destruction callback is run.
TEST_F(SigninHelperLacrosTest, DeletionCallback) {
  testing::StrictMock<base::MockOnceCallback<void(const CoreAccountId&)>>
      helper_deleted;

  // Create the helper.
  ExpectCookieSet("Updating");
  std::unique_ptr<SigninHelperLacros> web_signin_helper =
      CreateSigninHelperLacros(helper_deleted.Get());

  // Delete the helper.
  EXPECT_CALL(helper_deleted, Run(CoreAccountId())).Times(1);
  ExpectCookieSet("Consistent");
  web_signin_helper.reset();
}

// Checks that an account can be added through the OS when there is no available
// account.
TEST_F(SigninHelperLacrosTest, NoAccountAvailable) {
  testing::StrictMock<base::MockOnceCallback<void(const CoreAccountId&)>>
      helper_complete;

  const std::string email("testemail");
  std::string gaia_id = signin::GetTestGaiaIdForEmail(email);
  account_manager::Account new_account{
      {gaia_id, account_manager::AccountType::kGaia}, email};

  // Create the helper, this should trigger a call to `ShowAddAccountDialog()`.
  EXPECT_CALL(*mock_facade(),
              ShowAddAccountDialog(account_manager::AccountManagerFacade::
                                       AccountAdditionSource::kOgbAddAccount,
                                   testing::_))
      .Times(1)
      .WillRepeatedly(
          [new_account](
              account_manager::AccountManagerFacade::AccountAdditionSource,
              base::OnceCallback<void(
                  const account_manager::AccountUpsertionResult& result)>
                  callback) {
            std::move(callback).Run(
                account_manager::AccountUpsertionResult::FromAccount(
                    new_account));
          });
  ExpectCookieSet("Updating");
  std::unique_ptr<SigninHelperLacros> web_signin_helper =
      CreateSigninHelperLacros(helper_complete.Get());
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  testing::Mock::VerifyAndClearExpectations(mock_facade());

  // Simmulate the mapper adding the account to the profile.
  identity_test_env()->MakeAccountAvailable(email);

  EXPECT_CALL(helper_complete, Run(CoreAccountId::FromGaiaId(gaia_id)))
      .Times(1);

  // `AccountProfileMapper` expects a call of `OnAccountUpserted()` before
  // completing the account addition.
  EXPECT_CALL(*mock_facade(), GetAccounts(testing::_))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(
          [new_account](
              base::OnceCallback<void(
                  const std::vector<account_manager::Account>&)> callback) {
            std::move(callback).Run({new_account});
          });
  mapper()->OnAccountUpserted(new_account);

  // Account mapping is complete, check that the account was added to the right
  // profile.
  EXPECT_EQ(
      storage()->GetProfileAttributesWithPath(profile_path())->GetGaiaIds(),
      base::flat_set<std::string>{gaia_id});

  // The `AccountReconcilor` stops running, cookie is reset.
  ExpectCookieSet("Consistent");
  SimulateSetCookiesFinished(
      /*accounts_to_send=*/std::vector<CoreAccountId>{
          CoreAccountId::FromGaiaId(gaia_id)});

  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  testing::Mock::VerifyAndClearExpectations(&helper_complete);

  // Delete the helper, nothing happens.
  web_signin_helper.reset();
}
