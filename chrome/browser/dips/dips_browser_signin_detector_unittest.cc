// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_browser_signin_detector.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/dips/dips_browser_signin_detector_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

namespace {
constexpr char kIdentityProviderDomain[] = "google.com";

// Simple wrapper to serves as a POD for the test accounts.
struct TestAccount {
  const std::string email;
  const std::string host_domain;
};

std::unique_ptr<TestingProfile> BuildTestingProfile(
    network::TestURLLoaderFactory& test_url_loader_factory) {
  TestingProfile::Builder builder;
  builder.SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &test_url_loader_factory)}}));

  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(builder);
}
}  // namespace

class BrowserSigninDetectorServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_ = BuildTestingProfile(test_url_loader_factory_);

    identity_test_environment_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  // This initialization of the DIPS service will instantiate a copy of the
  // detector under test.
  void InitDIPSService() {
    DIPSBrowserSigninDetectorFactory::GetInstance()
        ->EnableWaitForServiceForTesting();
    dips_service_ = DIPSService::Get(profile_.get());
    EXPECT_NE(dips_service_, nullptr);
    DIPSBrowserSigninDetectorFactory::GetInstance()->WaitForServiceForTesting(
        profile_.get());
  }

  TestingProfile* profile() { return profile_.get(); }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_environment_profile_adaptor_->identity_test_env();
  }

  DIPSService* dips_service() { return dips_service_; }

  GURL GetURL(std::string_view domain) {
    return GURL(base::StrCat({"http://", domain}));
  }

  void SimulateSuccessfulFetchOfAccountInfo(const TestAccount* test_account,
                                            const AccountInfo* account_info) {
    identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
        account_info->account_id, account_info->email, account_info->gaia,
        test_account->host_domain,
        base::StrCat({"full_name-", test_account->email}),
        base::StrCat({"given_name-", test_account->email}),
        base::StrCat({"local-", test_account->email}),
        base::StrCat({"full_name-", test_account->email}));
  }

  bool DidSiteHaveInteraction(std::string_view domain) {
    base::test::TestFuture<bool> future;
    dips_service()->DidSiteHaveInteractionSince(
        GetURL(domain), base::Time::Min(), future.GetCallback());
    return future.Get();
  }

  content::BrowserTaskEnvironment task_environment_;

  const TestAccount kNonEnterpriseAccount = {"foo@bar.com", ""};
  const TestAccount kEnterpriseAccount = {"foo@enterprise.com",
                                          "enterprise.com"};
  const TestAccount kEnterpriseIdentityProviderDomainAccount = {
      base::StrCat({"foo@", kIdentityProviderDomain}), kIdentityProviderDomain};

 private:
  ScopedInitFeature feature_{features::kDIPS,
                             /*enable:*/ true,
                             /*params:*/ {{"persist_database", "true"}}};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_profile_adaptor_;
  raw_ptr<DIPSService> dips_service_;
};

TEST_F(BrowserSigninDetectorServiceTest, AccountWithNoExtendedAccountInfo) {
  InitDIPSService();

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kNonEnterpriseAccount.email, signin::ConsentLevel::kSignin);

  // Makes sure there are no domain available for the created account.
  base::MockCallback<base::OnceClosure> outcome_determined;
  signin::AccountManagedStatusFinder finder(identity_manager(), account_info,
                                            outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(),
            signin::AccountManagedStatusFinder::Outcome::kPending);

  // There should be no recorded interactions.
  ASSERT_FALSE(DidSiteHaveInteraction(kIdentityProviderDomain));
}

TEST_F(BrowserSigninDetectorServiceTest, NonEnterpriseAccount) {
  InitDIPSService();

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kNonEnterpriseAccount.email, signin::ConsentLevel::kSignin);

  SimulateSuccessfulFetchOfAccountInfo(&kNonEnterpriseAccount, &account_info);

  // Makes sure we have a non-enterprise account created based on the test
  // account provided.
  signin::AccountManagedStatusFinder finder(identity_manager(), account_info,
                                            base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            signin::AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown);

  // There should be a recorded interaction for the`kIdentityProviderDomain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kIdentityProviderDomain));
}

TEST_F(BrowserSigninDetectorServiceTest, EnterpriseAccount) {
  InitDIPSService();

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEnterpriseAccount.email, signin::ConsentLevel::kSignin);

  SimulateSuccessfulFetchOfAccountInfo(&kEnterpriseAccount, &account_info);

  // Makes sure we have an enterprise account created based on the test account
  // provided.
  signin::AccountManagedStatusFinder finder(identity_manager(), account_info,
                                            base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            signin::AccountManagedStatusFinder::Outcome::kEnterprise);

  // There should be a recorded interaction for the`kIdentityProviderDomain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kIdentityProviderDomain));

  // There should be a recorded interaction for the
  // `kEnterpriseAccount.host_domain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kEnterpriseAccount.host_domain));
}

TEST_F(BrowserSigninDetectorServiceTest,
       EnterpriseIdentityProviderDomainAccount) {
  InitDIPSService();

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kEnterpriseIdentityProviderDomainAccount.email,
      signin::ConsentLevel::kSignin);

  SimulateSuccessfulFetchOfAccountInfo(
      &kEnterpriseIdentityProviderDomainAccount, &account_info);

  // Makes sure we have an enterprise account created based on the test account
  // provided.
  signin::AccountManagedStatusFinder finder(identity_manager(), account_info,
                                            base::DoNothing());
  EXPECT_EQ(
      finder.GetOutcome(),
      signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom);

  // There should be a recorded interaction for the `kIdentityProviderDomain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kIdentityProviderDomain));
}

TEST_F(BrowserSigninDetectorServiceTest, LateObservation) {
  AccountInfo account_info_1 = identity_test_env()->MakePrimaryAccountAvailable(
      kNonEnterpriseAccount.email, signin::ConsentLevel::kSignin);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kEnterpriseAccount.email);

  identity_test_env()->SetCookieAccounts(
      {{account_info_1.email, account_info_1.gaia},
       {account_info_2.email, account_info_2.gaia}});

  SimulateSuccessfulFetchOfAccountInfo(&kNonEnterpriseAccount, &account_info_1);
  SimulateSuccessfulFetchOfAccountInfo(&kEnterpriseAccount, &account_info_2);

  // The initialization will instantiate a detector at this moment to simulate a
  // late detection.
  InitDIPSService();

  // There should be a recorded interaction for the `kIdentityProviderDomain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kIdentityProviderDomain));

  // There should be a recorded interaction for the
  // `kEnterpriseAccount.host_domain`.
  EXPECT_TRUE(DidSiteHaveInteraction(kEnterpriseAccount.host_domain));
}
