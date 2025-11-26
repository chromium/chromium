// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"

using signin_util::ProfileSeparationPolicyState;
using signin_util::ProfileSeparationPolicyStateSet;
using signin_util::SignedInState;

namespace {

const char kLegacyPolicyEmpty[] = "";
const char kLegacyPolicyNone[] = "none";
const char kLegacyPolicyPrimaryAccount[] = "primary_account";
const char kLegacyPolicyPrimaryAccountStrict[] = "primary_account_strict";
const char kLegacyPolicyPrimaryAccountStrictKeepExistingData[] =
    "primary_account_strict_keep_existing_data";
const char kLegacyPolicyPrimaryAccountKeepExistingData[] =
    "primary_account_keep_existing_data";

const GaiaId kSignedInGaiaId("signed_in_gaia_id");

}  // namespace

class SigninUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    signin_util::ResetForceSigninForTesting();
  }

  void TearDown() override {
    signin_util::ResetForceSigninForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  bool SeparationEnforcedByExistingProfileExpected(
      const std::string& local_policy) {
    return enforced_by_existing_profile.find(local_policy) !=
           enforced_by_existing_profile.end();
  }
  bool SeparationEnforcedByInterceptedAccountExpected(
      const std::string& intercepted_policy) {
    return enforced_by_intercepted_account.find(intercepted_policy) !=
           enforced_by_intercepted_account.end();
  }
  bool KeepBrowsingDataExpected(const std::string& local_policy,
                                const std::string& intercepted_policy) {
    return keeps_browsing_data.find(local_policy) !=
               keeps_browsing_data.end() &&
           keeps_browsing_data.find(intercepted_policy) !=
               keeps_browsing_data.end();
  }
  bool SeparationEnforcedOnMachineLevelExpected(
      const std::string& local_policy) {
    return enforced_on_machine_level.find(local_policy) !=
           enforced_on_machine_level.end();
  }

 protected:
  std::array<std::string, 6> all_policies{
      kLegacyPolicyEmpty,
      kLegacyPolicyNone,
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };

  base::flat_set<std::string> enforced_by_existing_profile{
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData};

  base::flat_set<std::string> enforced_by_intercepted_account{
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };

  base::flat_set<std::string> keeps_browsing_data{
      kLegacyPolicyEmpty,
      kLegacyPolicyNone,
      kLegacyPolicyPrimaryAccountKeepExistingData,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
  };

  base::flat_set<std::string> enforced_on_machine_level{
      kLegacyPolicyPrimaryAccount,
      kLegacyPolicyPrimaryAccountStrict,
      kLegacyPolicyPrimaryAccountStrictKeepExistingData,
      kLegacyPolicyPrimaryAccountKeepExistingData,
  };
};

TEST_F(SigninUtilTest, GetForceSigninPolicy) {
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());

  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               true);
  signin_util::ResetForceSigninForTesting();
  EXPECT_TRUE(signin_util::IsForceSigninEnabled());
  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               false);
  signin_util::ResetForceSigninForTesting();
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());
}

TEST_F(SigninUtilTest, IsProfileSeparationEnforcedByProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile.get(), /*intercepted_account_email=*/std::string()),
              SeparationEnforcedByExistingProfileExpected(local_policy));
  }

  // Test profile set a machine level.
  profile.get()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);

  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile.get(), /*intercepted_account_email=*/std::string()),
              SeparationEnforcedOnMachineLevelExpected(local_policy));
  }
}

TEST_F(SigninUtilTest, IsProfileSeparationEnforcedByPolicies) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& intercepted_policy : all_policies) {
    EXPECT_EQ(
        signin_util::IsProfileSeparationEnforcedByPolicies(
            policy::ProfileSeparationPolicies(intercepted_policy)),
        SeparationEnforcedByInterceptedAccountExpected(intercepted_policy));
  }
}

TEST_F(
    SigninUtilTest,
    ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfileLegacy) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile.get()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile.get()->GetPrefs()->SetString(
          prefs::kManagedAccountsSigninRestriction, local_policy);
    }

    for (const auto& intercepted_policy : all_policies) {
      EXPECT_EQ(
          signin_util::
              ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                  profile.get(),
                  policy::ProfileSeparationPolicies(intercepted_policy)),
          KeepBrowsingDataExpected(local_policy, intercepted_policy));
    }
  }
}

TEST_F(SigninUtilTest, IsSecondaryAccountAllowed) {
  const std::string consumer_email = "bob@gmail.com";
  const std::string enterprise_email = "bob@example.com";
  const std::string other_enterprise_email = "bob@bob.com";
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), consumer_email));
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), enterprise_email));
  EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
      profile(), other_enterprise_email));

  {
    profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                   base::Value::List());

    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
  {
    base::Value::List profile_separation_exception_list;
    profile_separation_exception_list.Append(base::Value("bob.com"));
    profile()->GetPrefs()->SetList(
        prefs::kProfileSeparationDomainExceptionList,
        std::move(profile_separation_exception_list));

    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
  {
    base::Value::List profile_separation_exception_list;
    profile_separation_exception_list.Append(base::Value("bob.com"));
    profile_separation_exception_list.Append(base::Value("gmail.com"));
    profile()->GetPrefs()->SetList(
        prefs::kProfileSeparationDomainExceptionList,
        std::move(profile_separation_exception_list));

    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_TRUE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
}

TEST_F(SigninUtilTest,
       IsProfileSeparationEnforcedByProfileSecondaryAccountNotAllowed) {
  const std::string consumer_email = "bob@gmail.com";
  const std::string enterprise_email = "bob@example.com";
  const std::string other_enterprise_email = "bob@bob.com";

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(profile(),
                                                                consumer_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), other_enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;
  }

  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 base::Value::List());

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), consumer_email))
        << policy;
    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), enterprise_email))
        << policy;
    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), other_enterprise_email))
        << policy;
  }

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("example.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  for (const auto& policy : all_policies) {
    profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                     policy);

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), consumer_email))
        << policy;

    EXPECT_EQ(signin_util::IsProfileSeparationEnforcedByProfile(
                  profile(), enterprise_email),
              SeparationEnforcedByExistingProfileExpected(policy))
        << policy;

    EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByProfile(
        profile(), other_enterprise_email))
        << policy;
  }
}

TEST_F(SigninUtilTest, IsProfileSeparationEnforced) {
  EXPECT_FALSE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::SUGGESTED, std::nullopt)));

  EXPECT_TRUE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::ENFORCED, std::nullopt)));

  EXPECT_FALSE(signin_util::IsProfileSeparationEnforcedByPolicies(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::DISABLED, std::nullopt)));
}

TEST_F(SigninUtilTest,
       ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile) {
  for (const auto& local_policy : all_policies) {
    if (local_policy.empty()) {
      profile()->GetPrefs()->ClearPref(
          prefs::kManagedAccountsSigninRestriction);
    } else {
      profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                       local_policy);
    }

    EXPECT_EQ(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   USER_OPT_IN)),
        KeepBrowsingDataExpected(local_policy, std::string()))
        << local_policy;

    EXPECT_EQ(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   USER_OPT_OUT)),
        KeepBrowsingDataExpected(local_policy, std::string()))
        << local_policy;

    EXPECT_FALSE(
        signin_util::
            ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                profile(), policy::ProfileSeparationPolicies(
                               policy::ProfileSeparationSettings::ENFORCED,
                               policy::ProfileSeparationDataMigrationSettings::
                                   ALWAYS_SEPARATE)))
        << local_policy;
  }
}
class SigninUtilHistorySyncOptinTest : public SigninUtilTest {
 public:
  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  void Signin(bool managed_account = false) {
    CHECK(profile());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    CHECK(identity_manager);
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithGaiaId(kSignedInGaiaId)
            .Build(managed_account ? "test@managed.com" : "test@gmail.com"));

    account_info =
        AccountInfo::Builder(account_info)
            .SetHostedDomain(managed_account ? "managed.com" : std::string())
            .Build();
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void SignInAndSetUpSyncService(bool managed_account = false) {
    Signin(managed_account);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
    CHECK(test_sync_service());
  }

  void DisableAllSyncedDataTypes() {
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  }

  void SetupForAvatarSyncPromo(bool managed_account = false) {
    SignInAndSetUpSyncService(managed_account);
    DisableAllSyncedDataTypes();

    // Simulate setting enough time passing for the cookie change.
    profile()->GetPrefs()->SetDouble(
        prefs::kGaiaCookieChangedTime,
        (base::Time::Now() -
         (switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam() +
          base::Minutes(1)))
            .InSecondsFSinceUnixEpoch());

    // The rest of the setup should be aligned with the default profile
    // initialization/signin.
  }

 private:
  // Use this flag to simplify test writing and not restrict to Windows only.
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kAvatarButtonSyncPromoForTesting};
};

TEST_F(SigninUtilHistorySyncOptinTest, HistorySyncOptinDisallowedByPolicy) {
  SignInAndSetUpSyncService();

  syncer::UserSelectableTypeSet types({syncer::UserSelectableType::kTabs,
                                       syncer::UserSelectableType::kHistory});

  DisableAllSyncedDataTypes();
  ASSERT_TRUE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), types));

  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  EXPECT_FALSE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), types));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       HistorySyncOptinDisallowedByPolicyPerType) {
  SignInAndSetUpSyncService();

  syncer::UserSelectableTypeSet types({syncer::UserSelectableType::kTabs,
                                       syncer::UserSelectableType::kHistory});

  DisableAllSyncedDataTypes();
  ASSERT_TRUE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), types));

  test_sync_service()->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kHistory, /*managed=*/true);

  EXPECT_FALSE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), types));
  EXPECT_FALSE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), {syncer::UserSelectableType::kHistory}));
  EXPECT_TRUE(signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
      test_sync_service(), {syncer::UserSelectableType::kTabs}));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(SigninUtilHistorySyncOptinTest, HasExplicitlyDisabledHistorySync) {
  SignInAndSetUpSyncService();
  EXPECT_FALSE(signin_util::HasExplicitlyDisabledHistorySync(
      test_sync_service(), IdentityManagerFactory::GetForProfile(profile())));

  test_sync_service()->GetUserSettings()->SetDisabledType(
      syncer::UserSelectableType::kHistory);
  EXPECT_TRUE(signin_util::HasExplicitlyDisabledHistorySync(
      test_sync_service(), IdentityManagerFactory::GetForProfile(profile())));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfNoPrimaryAccount) {
  ASSERT_TRUE(profile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(identity_manager);
  ASSERT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  EXPECT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserNotSignedIn);
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfNoSyncService) {
  Signin();
  ASSERT_FALSE(test_sync_service());
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden);
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfSyncDisabled) {
  SignInAndSetUpSyncService();

  DisableAllSyncedDataTypes();

  ASSERT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);

  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden);
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfUserIsAlreadyOptedIn) {
  SignInAndSetUpSyncService();

  DisableAllSyncedDataTypes();

  ASSERT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);

  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);

  EXPECT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShowHistorySyncOptinScreenIfUserNotOptedInHistory) {
  SignInAndSetUpSyncService();

  ASSERT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);

  // History off.
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShowHistorySyncOptinScreenIfUserNotOptedInTabs) {
  SignInAndSetUpSyncService();

  ASSERT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);

  // Tabs off.
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, false);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShowHistorySyncOptinScreenIfUserNotOptedInTabGroups) {
  SignInAndSetUpSyncService();

  ASSERT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);

  // Tab groups off.
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, false);

  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);
}

TEST_F(SigninUtilHistorySyncOptinTest, EnableHistorySync) {
  SignInAndSetUpSyncService();
  DisableAllSyncedDataTypes();

  signin_util::EnableHistorySync(test_sync_service());

  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  EXPECT_TRUE(test_sync_service()->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kSavedTabGroups));
}

TEST_F(SigninUtilHistorySyncOptinTest, ShouldShowAvatarSyncPromo) {
  SetupForAvatarSyncPromo();
  EXPECT_TRUE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowAvatarSyncPromoManagedAccounts) {
  SetupForAvatarSyncPromo(/*managed_account*/ true);
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldShowAvatarSyncPromoBasedOnProfleAlreadySyncing) {
  SetupForAvatarSyncPromo();
  ASSERT_TRUE(signin_util::ShouldShowAvatarSyncPromo(profile()));

  // Promo should not show if the previously syncing gaia id is different than
  // the current signed in one.
  const GaiaId previously_syncing_gaia_id("syncing_gaia_id");
  ASSERT_NE(previously_syncing_gaia_id, kSignedInGaiaId);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   previously_syncing_gaia_id.ToString());
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));

  // Promo can show if the gaia id match.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kSignedInGaiaId.ToString());
  EXPECT_TRUE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldShowAvatarSyncPromoBasedOnGaiaCookieAge) {
  SetupForAvatarSyncPromo();
  ASSERT_TRUE(signin_util::ShouldShowAvatarSyncPromo(profile()));

  // Promo should not show if the gaia cookie age is too short.
  profile()->GetPrefs()->SetDouble(
      prefs::kGaiaCookieChangedTime,
      (base::Time::Now() -
       (switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam() -
        base::Minutes(1)))
          .InSecondsFSinceUnixEpoch());
  EXPECT_FALSE(signin_util::ShouldShowAvatarSyncPromo(profile()));

  // Promo can show if the gaia cookie age is long enough.
  profile()->GetPrefs()->SetDouble(
      prefs::kGaiaCookieChangedTime,
      (base::Time::Now() -
       (switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam() +
        base::Minutes(1)))
          .InSecondsFSinceUnixEpoch());
  EXPECT_TRUE(signin_util::ShouldShowAvatarSyncPromo(profile()));
}

class SigninUtilHistorySyncOptinForManagedSettingsTest
    : public SigninUtilHistorySyncOptinTest,
      public testing::WithParamInterface<syncer::UserSelectableType> {};

TEST_P(SigninUtilHistorySyncOptinForManagedSettingsTest,
       ShouldNotShowHistorySyncOptinScreenForManagedType) {
  SignInAndSetUpSyncService();

  DisableAllSyncedDataTypes();
  ASSERT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);

  test_sync_service()->GetUserSettings()->SetTypeIsManagedByPolicy(
      GetParam(), /*managed=*/true);

  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden);
}

TEST_P(SigninUtilHistorySyncOptinForManagedSettingsTest,
       ShouldNotShowHistorySyncOptinScreenForSupervisedType) {
  SignInAndSetUpSyncService();

  DisableAllSyncedDataTypes();

  ASSERT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);

  test_sync_service()->GetUserSettings()->SetTypeIsManagedByCustodian(
      GetParam(), /*managed=*/true);

  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninUtilHistorySyncOptinForManagedSettingsTest,
    testing::Values(syncer::UserSelectableType::kHistory,
                    syncer::UserSelectableType::kTabs,
                    syncer::UserSelectableType::kSavedTabGroups),
    [](const auto& info) { return GetUserSelectableTypeName(info.param); });
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(SignedInStatesTest, SignedInStates) {
  base::test::SingleThreadTaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  signin::IdentityManager* identity_manager =
      identity_test_env.identity_manager();

  // No Account present.
  EXPECT_EQ(SignedInState::kSignedOut,
            signin_util::GetSignedInState(identity_manager));

  // Web signed in.
  identity_test_env.MakeAccountAvailable("test@email.com",
                                         {.set_cookie = true});
  EXPECT_EQ(SignedInState::kWebOnlySignedIn,
            signin_util::GetSignedInState(identity_manager));

  // Syncing.
  AccountInfo info = identity_test_env.MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSync);
  EXPECT_EQ(SignedInState::kSyncing,
            signin_util::GetSignedInState(identity_manager));

  // Sync paused state.
  identity_test_env.SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_EQ(SignedInState::kSyncPaused,
            signin_util::GetSignedInState(identity_manager));

  // Remove account.
  identity_test_env.ClearPrimaryAccount();
  EXPECT_EQ(SignedInState::kSignedOut,
            signin_util::GetSignedInState(identity_manager));

  // In incognito mode, there would be no identity manager.
  EXPECT_EQ(SignedInState::kSignedOut, signin_util::GetSignedInState(nullptr));

  // Signed in.
  info = identity_test_env.MakePrimaryAccountAvailable(
      "test@email.com", signin::ConsentLevel::kSignin);
  EXPECT_EQ(SignedInState::kSignedIn,
            signin_util::GetSignedInState(identity_manager));

  // When explicit browser signin is enabled, being signed in with an invalid
  // refresh token is equivalent to the sign in pending state.
  identity_test_env.SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_EQ(SignedInState::kSignInPending,
            signin_util::GetSignedInState(identity_manager));
}
#endif  // !BUILDFLAG(ENABLE_DICE_SUPPORT)
