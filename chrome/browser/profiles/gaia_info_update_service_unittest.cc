// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/state.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::Return;

namespace {

AccountInfo GetValidAccountInfo(std::string email,
                                CoreAccountId account_id,
                                std::string given_name,
                                std::string full_name,
                                std::string hosted_domain) {
  AccountInfo account_info;
  account_info.email = email;
  account_info.gaia = account_id.ToString();
  account_info.account_id = account_id;
  account_info.given_name = given_name;
  account_info.full_name = full_name;
  account_info.hosted_domain = hosted_domain;
  account_info.locale = email;
  account_info.picture_url = "example.com";
  return account_info;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kChromiumOrgDomain[] = "chromium.org";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

class GAIAInfoUpdateServiceTest : public testing::Test {
 protected:
  GAIAInfoUpdateServiceTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        identity_test_env_(
            /*test_url_loader_factory=*/nullptr,
            /*pref_service=*/nullptr,
            /*test_signin_client=*/nullptr) {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  GAIAInfoUpdateServiceTest(const GAIAInfoUpdateServiceTest&) = delete;
  GAIAInfoUpdateServiceTest& operator=(const GAIAInfoUpdateServiceTest&) =
      delete;

  ~GAIAInfoUpdateServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    RecreateGAIAInfoUpdateService();
  }

  void RecreateGAIAInfoUpdateService() {
    if (service_)
      service_->Shutdown();

    service_ = std::make_unique<GAIAInfoUpdateService>(
        identity_test_env_.identity_manager(),
        testing_profile_manager_.profile_attributes_storage(), pref_service_,
        profile()->GetPath());
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
  }

  TestingProfile* profile() {
    if (!profile_)
      CreateProfile("Person 1");
    return profile_;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  ProfileAttributesStorage* storage() {
    return testing_profile_manager_.profile_attributes_storage();
  }

  GAIAInfoUpdateService* service() { return service_.get(); }

  void CreateProfile(const std::string& name) {
    profile_ = testing_profile_manager_.CreateTestingProfile(
        name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(name), 0, TestingProfile::TestingFactories());
  }

  bool HasAccountPrefs(const std::string& gaia_id) {
    return SigninPrefs(pref_service_).HasAccountPrefs(gaia_id);
  }

  void InitializeAccountPref(const std::string& gaia_id) {
    // Set any pref value to create the pref container.
    SigninPrefs(pref_service_)
        .SetChromeSigninInterceptionUserChoice(gaia_id,
                                               ChromeSigninUserChoice::kSignin);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<GAIAInfoUpdateService> service_;
};

TEST_F(GAIAInfoUpdateServiceTest, SyncOnSyncOff) {
  AccountInfo info =
      identity_test_env()->MakeAccountAvailable("pat@example.com");
  base::RunLoop().RunUntilIdle();
  identity_test_env()->SetPrimaryAccount(info.email,
                                         signin::ConsentLevel::kSync);
  info = GetValidAccountInfo(info.email, info.account_id, "Pat", "Pat Foo",
                             kNoHostedDomainFound);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  EXPECT_EQ(entry->GetGAIAGivenName(), u"Pat");
  EXPECT_EQ(entry->GetGAIAName(), u"Pat Foo");
  EXPECT_EQ(entry->GetHostedDomain(), kNoHostedDomainFound);

  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  signin::SimulateAccountImageFetch(identity_test_env()->identity_manager(),
                                    info.account_id, "GAIA_IMAGE_URL_WITH_SIZE",
                                    gaia_picture);
  // Set a fake picture URL.
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_picture, entry->GetAvatarIcon()));
  // Log out.
  identity_test_env()->ClearPrimaryAccount();
  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_TRUE(entry->GetHostedDomain().empty());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(GAIAInfoUpdateServiceTest, RevokeSyncConsent) {
  AccountInfo info =
      identity_test_env()->MakeAccountAvailable("pat@example.com");
  base::RunLoop().RunUntilIdle();
  identity_test_env()->SetPrimaryAccount(info.email,
                                         signin::ConsentLevel::kSync);
  info = GetValidAccountInfo(info.email, info.account_id, "Pat", "Pat Foo",
                             kNoHostedDomainFound);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  signin::SimulateAccountImageFetch(identity_test_env()->identity_manager(),
                                    info.account_id, "GAIA_IMAGE_URL_WITH_SIZE",
                                    gaia_picture);
  // Revoke sync consent (stay signed in with the primary account).
  identity_test_env()->RevokeSyncConsent();
  ASSERT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  // Verify that the GAIA name and picture, and picture URL are not cleared
  // as unconsented primary account still exists.
  EXPECT_EQ(entry->GetGAIAGivenName(), u"Pat");
  EXPECT_EQ(entry->GetGAIAName(), u"Pat Foo");
  EXPECT_EQ(entry->GetHostedDomain(), kNoHostedDomainFound);
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_picture, entry->GetAvatarIcon()));
}

TEST_F(GAIAInfoUpdateServiceTest, LogInLogOut) {
  std::string email = "pat@example.com";
  AccountInfo info = identity_test_env()->MakePrimaryAccountAvailable(
      email, signin::ConsentLevel::kSignin);
  EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  info = GetValidAccountInfo(info.email, info.account_id, "Pat", "Pat Foo",
                             kNoHostedDomainFound);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  EXPECT_EQ(entry->GetGAIAGivenName(), u"Pat");
  EXPECT_EQ(entry->GetGAIAName(), u"Pat Foo");
  EXPECT_EQ(entry->GetHostedDomain(), kNoHostedDomainFound);

  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  signin::SimulateAccountImageFetch(identity_test_env()->identity_manager(),
                                    info.account_id, "GAIA_IMAGE_URL_WITH_SIZE",
                                    gaia_picture);
  // Set a fake picture URL.
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_picture, entry->GetAvatarIcon()));
  // Log out.
  identity_test_env()->ClearPrimaryAccount();
  base::RunLoop().RunUntilIdle();

  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_TRUE(entry->GetHostedDomain().empty());
}

TEST_F(GAIAInfoUpdateServiceTest, LogInLogOutLogIn) {
  std::string email1 = "pat1@example.com";
  AccountInfo info1 =
      identity_test_env()->MakeAccountAvailable(email1, {.set_cookie = true});
  base::RunLoop().RunUntilIdle();
  info1 = GetValidAccountInfo(info1.email, info1.account_id, "Pat 1",
                              "Pat Foo The First", kNoHostedDomainFound);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info1);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();

  // Test correct histogram recording for all accounts info that has no getters.
  base::HistogramTester tester;
  entry->RecordAccountNamesMetric();
  tester.ExpectBucketCount(
      "Profile.AllAccounts.Names",
      /*sample=*/profile_metrics::AllAccountsNames::kLikelySingleName,
      /*expected_count=*/1);

  // Log out and record the metric again, sign-out wipes previous info in the
  // entry so again the default values get reported.
  identity_test_env()->SetCookieAccounts({});
  entry->RecordAccountNamesMetric();
  tester.ExpectBucketCount(
      "Profile.AllAccounts.Names",
      /*sample=*/profile_metrics::AllAccountsNames::kLikelySingleName,
      /*expected_count=*/2);

  std::string email2 = "pat2@example.com";
  AccountInfo info2 =
      identity_test_env()->MakeAccountAvailable(email2, {.set_cookie = true});
  base::RunLoop().RunUntilIdle();
  info2 = GetValidAccountInfo(info2.email, info2.account_id, "Pat 2",
                              "Pat Foo The Second", kChromiumOrgDomain);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info2);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());

  // Because due to the complete sign-out, the info about the previous account
  // got wiped. Thus the same default metrics get recorded again, despite the
  // second account has a different gaia name than the first one.
  entry->RecordAccountNamesMetric();
  tester.ExpectBucketCount(
      "Profile.AllAccounts.Names",
      /*sample=*/profile_metrics::AllAccountsNames::kLikelySingleName,
      /*expected_count=*/3);
  tester.ExpectTotalCount("Profile.AllAccounts.Names", /*expected_count=*/3);
}

TEST_F(GAIAInfoUpdateServiceTest, MultiLoginAndLogOut) {
  // Make two accounts available with both refresh token and cookies.
  AccountInfo info1 =
      identity_test_env()->MakeAccountAvailable("pat@example.com");
  AccountInfo info2 =
      identity_test_env()->MakeAccountAvailable("pat2@example.com");
  identity_test_env()->SetCookieAccounts(
      {{info1.email, info1.gaia}, {info2.email, info2.gaia}});
  base::RunLoop().RunUntilIdle();
  info1 = GetValidAccountInfo(info1.email, info1.account_id, "Pat 1",
                              "Pat Foo The First", kNoHostedDomainFound);
  // Make the second account an enterprise account by setting a hosted domain.
  info2 = GetValidAccountInfo(info2.email, info2.account_id, "Pat 2",
                              "Pat Foo The Second", kChromiumOrgDomain);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info1);
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info2);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();

  // Test correct histogram recording for all accounts info that has no getters.
  // The two accounts have different gaia names.
  base::HistogramTester tester;
  entry->RecordAccountNamesMetric();
  tester.ExpectBucketCount(
      "Profile.AllAccounts.Names",
      /*sample=*/profile_metrics::AllAccountsNames::kMultipleNamesWithoutSync,
      /*expected_count=*/1);

  // Log out and record the metric again, sign-out wipes previous info in the
  // entry so the default values get reported.
  identity_test_env()->SetCookieAccounts({});
  entry->RecordAccountNamesMetric();
  tester.ExpectBucketCount(
      "Profile.AllAccounts.Names",
      /*sample=*/profile_metrics::AllAccountsNames::kLikelySingleName,
      /*expected_count=*/1);
  tester.ExpectTotalCount("Profile.AllAccounts.Names", /*expected_count=*/2);
}
#endif  // !BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(GAIAInfoUpdateServiceTest, ClearGaiaInfoOnStartup) {
  // Simulate a state where the profile entry has GAIA related information
  // when there is not primary account set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_EQ(1u, storage()->GetNumberOfProfiles());
  ProfileAttributesEntry* entry = storage()->GetAllProfilesAttributes().front();
  entry->SetGAIAName(u"foo");
  entry->SetGAIAGivenName(u"Pat Foo");
  gfx::Image gaia_picture = gfx::test::CreateImage(256, 256);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE", gaia_picture);
  entry->SetHostedDomain(kNoHostedDomainFound);

  // Verify that creating the GAIAInfoUpdateService resets the GAIA related
  // profile attributes if the profile no longer has a primary account and that
  // the profile info cache observer wass notified about profile name and
  // avatar changes.
  RecreateGAIAInfoUpdateService();

  EXPECT_TRUE(entry->GetGAIAName().empty());
  EXPECT_TRUE(entry->GetGAIAGivenName().empty());
  EXPECT_FALSE(entry->GetGAIAPicture());
  EXPECT_TRUE(entry->GetHostedDomain().empty());
}

TEST_F(GAIAInfoUpdateServiceTest,
       SigninPrefsWithSignedInAccountAndSecondaryAccount) {
  base::HistogramTester histogram_tester;
  const std::string primary_gaia_id = "primary_gaia_id";
  ASSERT_FALSE(HasAccountPrefs(primary_gaia_id));

  AccountInfo primary_info = identity_test_env()->MakeAccountAvailable(
      "primary@example.com",
      {.primary_account_consent_level = signin::ConsentLevel::kSignin,
       .set_cookie = true,
       .gaia_id = primary_gaia_id});
  ASSERT_EQ(primary_gaia_id, primary_info.gaia);
  InitializeAccountPref(primary_gaia_id);
  EXPECT_TRUE(HasAccountPrefs(primary_gaia_id));

  // Add a secondary account.
  const std::string secondary_gaia_id = "secondary_gaia_id";
  ASSERT_FALSE(HasAccountPrefs(secondary_gaia_id));
  AccountInfo secondary_info = identity_test_env()->MakeAccountAvailable(
      "secondary@gmail.com",
      {.set_cookie = true, .gaia_id = secondary_gaia_id});
  ASSERT_EQ(secondary_gaia_id, secondary_info.gaia);
  InitializeAccountPref(secondary_gaia_id);
  EXPECT_TRUE(HasAccountPrefs(secondary_gaia_id));

  // Set the accounts as signed out.
  identity_test_env()->SetCookieAccounts(
      {{primary_info.email, primary_info.gaia, /*signed_out=*/true},
       {secondary_info.email, secondary_info.gaia, /*signed_out=*/true}});
  // Prefs should remain as the cookies are not cleared yet.
  EXPECT_TRUE(HasAccountPrefs(primary_gaia_id));
  EXPECT_TRUE(HasAccountPrefs(secondary_gaia_id));

  // Clear all cookies.
  identity_test_env()->SetCookieAccounts({});
  ASSERT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  // Primary account prefs should remain since the account is still signed in.
  EXPECT_TRUE(HasAccountPrefs(primary_gaia_id));
  // Secondary account prefs should be cleared.
  EXPECT_FALSE(HasAccountPrefs(secondary_gaia_id));

  histogram_tester.ExpectUniqueSample("Signin.AccountPref.RemovedCount",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountPref.RemovedCount.SignedIn",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("Signin.AccountPref.RemovedCount.SignedOut",
                                    /*expected_count=*/0);

  // Clearing primary account should now clear the account prefs as well
  // since the cookie is already cleared.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(HasAccountPrefs(primary_gaia_id));

  histogram_tester.ExpectUniqueSample("Signin.AccountPref.RemovedCount",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountPref.RemovedCount.SignedIn",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.AccountPref.RemovedCount.SignedOut",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
}

TEST_F(GAIAInfoUpdateServiceTest, SigninPrefsWithSignedInWebOnly) {
  const std::string gaia_id = "gaia_id";
  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  AccountInfo info = identity_test_env()->MakeAccountAvailable(
      "test@gmail.com", {.set_cookie = true, .gaia_id = gaia_id});
  ASSERT_EQ(gaia_id, info.gaia);
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  InitializeAccountPref(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));

  // Web sign out keeps the prefs.
  identity_test_env()->SetCookieAccounts(
      {{info.email, info.gaia, /*signed_out=*/true}});
  EXPECT_TRUE(HasAccountPrefs(gaia_id));

  // Clearing the cookie removes the prefs.
  identity_test_env()->SetCookieAccounts({});
  EXPECT_FALSE(HasAccountPrefs(gaia_id));
}

TEST_F(GAIAInfoUpdateServiceTest, SigninPrefsWithGaiaIdNotInChrome) {
  // Use an account in Chrome.
  const std::string gaia_id = "gaia_id";
  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  AccountInfo info = identity_test_env()->MakeAccountAvailable(
      "test@gmail.com", {.set_cookie = true, .gaia_id = gaia_id});
  ASSERT_EQ(gaia_id, info.gaia);
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  InitializeAccountPref(gaia_id);
  ASSERT_TRUE(HasAccountPrefs(gaia_id));

  // Use an account that is not in Chrome.
  const std::string gaia_id_not_in_chrome = "gaia_id_not_in_chrome";
  ASSERT_FALSE(HasAccountPrefs(gaia_id_not_in_chrome));

  // This is possible even if the account is not in Chrome.
  InitializeAccountPref(gaia_id_not_in_chrome);
  EXPECT_TRUE(HasAccountPrefs(gaia_id_not_in_chrome));

  // Refreshing the cookie jar should remove the account not in Chrome.
  identity_test_env()->TriggerListAccount();

  // Prefs for the Account in Chrome remains, not for the account not in Chrome.
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_FALSE(HasAccountPrefs(gaia_id_not_in_chrome));
}
