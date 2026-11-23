// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include <string_view>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/signin/accounts_policy_manager_factory.h"
#endif
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

}  // namespace

class SigninUtilTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("TestProfile");
    signin_util::ResetForceSigninForTesting();
  }

  void TearDown() override {
    signin_util::ResetForceSigninForTesting();
  }

  TestingProfile* profile() { return profile_; }

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
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;

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
                                   base::ListValue());

    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), consumer_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), enterprise_email));
    EXPECT_FALSE(signin_util::IsAccountExemptedFromEnterpriseProfileSeparation(
        profile(), other_enterprise_email));
  }
  {
    base::ListValue profile_separation_exception_list;
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
    base::ListValue profile_separation_exception_list;
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
                                 base::ListValue());

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

  base::ListValue profile_separation_exception_list;
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
            .WithGaiaId(GaiaId("signed_in_gaia_id"))
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
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfNoSyncService) {
  Signin();
  ASSERT_FALSE(test_sync_service());
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kSkipSyncForbidden);
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

  EXPECT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);
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
  EXPECT_EQ(signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
            signin_util::ShouldShowHistorySyncOptinResult::kShow);
}

TEST_F(SigninUtilHistorySyncOptinTest,
       ShouldNotShowHistorySyncOptinScreenIfUserNotOptedInTabGroups) {
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

  EXPECT_EQ(
      signin_util::ShouldShowHistorySyncOptinScreen(*profile()),
      signin_util::ShouldShowHistorySyncOptinResult::kSkipUserAlreadyOptedIn);
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

INSTANTIATE_TEST_SUITE_P(All,
                         SigninUtilHistorySyncOptinForManagedSettingsTest,
                         testing::Values(syncer::UserSelectableType::kHistory,
                                         syncer::UserSelectableType::kTabs),
                         [](const auto& info) {
                           return GetUserSelectableTypeName(info.param);
                         });
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

using signin_util::SwitchPrimaryAccountWithinProfile;
using signin_util::SwitchPrimaryAccountWithinProfileResult;

namespace {

class PrimaryAccountEventRecorder : public signin::IdentityManager::Observer {
 public:
  explicit PrimaryAccountEventRecorder(
      signin::IdentityManager* identity_manager) {
    observation_.Observe(identity_manager);
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    events_.push_back(event);
  }

  const std::vector<signin::PrimaryAccountChangeEvent>& events() const {
    return events_;
  }

 private:
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};
  std::vector<signin::PrimaryAccountChangeEvent> events_;
};

}  // namespace

class SigninUtilSwitchPrimaryAccountTest : public testing::Test {
 public:
  struct AccountPair {
    AccountInfo primary;
    AccountInfo secondary;
  };

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    signin_util::ResetForceSigninForTesting();
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    profile_ = profile_manager_.CreateTestingProfile(
        "test_profile",
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        u"test_profile", 0, std::move(factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    signin_util::ResetForceSigninForTesting();
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }
  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }
  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  TestingProfile* CreateProfileWithoutAccountsPolicyManager(
      const std::string& profile_name) {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    factories.emplace_back(
        AccountsPolicyManagerFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return nullptr;
            }));
#endif
    return profile_manager_.CreateTestingProfile(
        profile_name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(profile_name), 0, std::move(factories));
  }

  AccountPair SetUpTwoAccounts(
      std::string_view primary_email = "account_a@gmail.com",
      std::string_view secondary_email = "account_b@gmail.com",
      bool set_cookies = false) {
    AccountInfo primary = identity_test_env()->MakePrimaryAccountAvailable(
        primary_email, signin::ConsentLevel::kSignin);
    AccountInfo secondary =
        identity_test_env()->MakeAccountAvailable(secondary_email);
    if (set_cookies) {
      signin::SetCookieAccounts(
          identity_manager(), &test_url_loader_factory_,
          {{std::string(primary.GetEmail()), primary.GetGaiaId()},
           {std::string(secondary.GetEmail()), secondary.GetGaiaId()}});
    }
    return {primary, secondary};
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(SigninUtilSwitchPrimaryAccountTest, SuccessAndToggleBack) {
  auto [account_a, account_b] = SetUpTwoAccounts(
      "account_a@gmail.com", "account_b@gmail.com", /*set_cookies=*/true);

  PrimaryAccountEventRecorder recorder(identity_manager());
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kMenu;
  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(profile(), local_state(),
                                        account_b.GetAccountId(), access_point),
      SwitchPrimaryAccountWithinProfileResult::kSuccess);

  // Primary account transitioned to B at ConsentLevel::kSignin.
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_b.GetAccountId());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Previous primary account A stays signed in on the web (refresh token and
  // cookies preserved), alongside B.
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_a.GetAccountId()));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_a.GetAccountId()));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_b.GetAccountId()));
  signin::AccountsInCookieJarInfo cookies =
      identity_manager()->GetAccountsInCookieJar();
  ASSERT_EQ(cookies.GetValidSignedInAccounts().size(), 2u);
  EXPECT_EQ(cookies.GetValidSignedInAccounts()[0].gaia_id,
            account_a.GetGaiaId());
  EXPECT_EQ(cookies.GetValidSignedInAccounts()[1].gaia_id,
            account_b.GetGaiaId());

  // Two-step primary account change event sequence: kCleared(A) -> kSet(B).
  ASSERT_EQ(recorder.events().size(), 2u);
  EXPECT_EQ(recorder.events()[0].GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kCleared);
  EXPECT_EQ(recorder.events()[0].GetPreviousState().primary_account.account_id,
            account_a.GetAccountId());
  EXPECT_EQ(recorder.events()[0].GetClearPrimaryAccountSource(),
            signin_metrics::ProfileSignout::kSignoutForAccountSwitching);

  EXPECT_EQ(recorder.events()[1].GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kSet);
  EXPECT_EQ(recorder.events()[1].GetCurrentState().primary_account.account_id,
            account_b.GetAccountId());
  EXPECT_EQ(recorder.events()[1].GetSetPrimaryAccountAccessPoint(),
            access_point);

  // Signout and signin metrics recorded with expected sources.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignoutProfile",
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SignOut.Completed",
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching, 1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point, 1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed", access_point,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("Signin_Signin_FromMenu"), 1);

  // Switch back B -> A cleanly.
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_a.GetAccountId(),
                signin_metrics::AccessPoint::kSettings),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_b.GetAccountId()));
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       PreservesSigninPrefsAndUpdatesProfileAttributes) {
  auto [account_a, account_b] = SetUpTwoAccounts(
      "account_a@gmail.com", "account_b@gmail.com", /*set_cookies=*/true);
  account_a = AccountInfo::Builder(account_a)
                  .SetFullName("Account A")
                  .SetGivenName("Alice")
                  .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                  .SetAvatarUrl("https://example.com/a")
                  .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_a);

  account_b = AccountInfo::Builder(account_b)
                  .SetFullName("Account B")
                  .SetGivenName("Bob")
                  .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                  .SetAvatarUrl("https://example.com/b")
                  .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_b);

  // Instantiate GAIAInfoUpdateService (whose factory returns null in testing)
  // so sign-out pref cleanup (`RemoveAllAccountPrefsExcept`) runs during the
  // switch. `SigninProfileAttributesUpdater` is already created automatically
  // for `profile()` by `SigninProfileAttributesUpdaterFactory`.
  GAIAInfoUpdateService gaia_info_update_service(
      profile(), identity_manager(),
      profile_manager()->profile_attributes_storage(), *profile()->GetPrefs(),
      profile()->GetPath());

  const base::Time past_signin_time = base::Time::Now() - base::Days(30);
  SigninPrefs signin_prefs(*profile()->GetPrefs());
  signin_prefs.SetChromeLastSigninTime(account_a.GetGaiaId(), past_signin_time);
  signin_prefs.SetChromeSigninInterceptionUserChoice(
      account_a.GetGaiaId(), ChromeSigninUserChoice::kAlwaysAsk);
  signin_prefs.IncrementChromeSigninBubbleRepromptCount(account_a.GetGaiaId());
  signin_prefs.IncrementChromeSigninBubbleRepromptCount(account_b.GetGaiaId());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);

  // SigninPrefs for previous primary account A are preserved (no account pref
  // removal occurred because A kept its refresh token and cookie).
  EXPECT_EQ(signin_prefs.GetChromeLastSigninTime(account_a.GetGaiaId()),
            past_signin_time);
  EXPECT_EQ(
      signin_prefs.GetChromeSigninInterceptionUserChoice(account_a.GetGaiaId()),
      ChromeSigninUserChoice::kAlwaysAsk);
  EXPECT_EQ(
      signin_prefs.GetChromeSigninBubbleRepromptCount(account_a.GetGaiaId()),
      1);
  EXPECT_TRUE(
      signin_prefs.GetChromeLastSignoutTime(account_a.GetGaiaId()).has_value());

  // SigninPrefs for new primary account B are preserved and updated.
  EXPECT_EQ(
      signin_prefs.GetChromeSigninBubbleRepromptCount(account_b.GetGaiaId()),
      1);
  EXPECT_TRUE(
      signin_prefs.GetChromeLastSigninTime(account_b.GetGaiaId()).has_value());
  histogram_tester.ExpectTotalCount("Signin.AccountPref.RemovedCount", 0);

  // ProfileAttributesEntry transitioned cleanly from A to B.
  ProfileAttributesEntry* entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile()->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetGAIAId(), account_b.GetGaiaId());
  EXPECT_EQ(entry->GetUserName(), base::UTF8ToUTF16(account_b.GetEmail()));
  EXPECT_EQ(entry->GetGAIAName(), u"Account B");
  EXPECT_EQ(entry->GetGAIAGivenName(), u"Bob");

  gaia_info_update_service.Shutdown();
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       LocksAccountReconcilorDuringTransientSignedOutState) {
  auto [account_a, account_b] = SetUpTwoAccounts();

  // Instantiate AccountReconcilor with fresh cookies that only list account A
  // (missing B). Without an AccountReconcilor::Lock across the two-step switch,
  // clearing A would transiently put DiceAccountReconcilorDelegate into
  // cookie-based consistency mode and revoke B's refresh token before
  // SetPrimaryAccount(B) runs.
  ASSERT_NE(AccountReconcilorFactory::GetForProfile(profile()), nullptr);
  signin::SetCookieAccounts(
      identity_manager(), &test_url_loader_factory_,
      {{std::string(account_a.GetEmail()), account_a.GetGaiaId()}});

  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_b.GetAccountId());
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_a.GetAccountId()));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_b.GetAccountId()));
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsForInvalidProfile) {
  const CoreAccountId dummy_id = CoreAccountId::FromGaiaId(GaiaId("gaia_id"));
  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(nullptr, local_state(), dummy_id,
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kInvalidProfile);

  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(otr_profile, local_state(), dummy_id,
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kInvalidProfile);

  TestingProfile* guest_profile = profile_manager()->CreateGuestProfile();
  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(guest_profile, local_state(), dummy_id,
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kInvalidProfile);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenNoPrimaryAccount) {
  AccountInfo account_b =
      identity_test_env()->MakeAccountAvailable("account_b@gmail.com");
  base::HistogramTester histogram_tester;

  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kNoPrimaryAccount);
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester.ExpectTotalCount("Signin.SignoutProfile", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenAlreadyPrimaryAccount) {
  AccountInfo account_a = identity_test_env()->MakePrimaryAccountAvailable(
      "account_a@gmail.com", signin::ConsentLevel::kSignin);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_a.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kAlreadyPrimaryAccount);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
  histogram_tester.ExpectTotalCount("Signin.SignoutProfile", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenRefreshTokensNotLoaded) {
  auto [account_a, account_b] = SetUpTwoAccounts();
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();

  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kRefreshTokensNotLoaded);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsForInvalidTargetAccount) {
  auto [account_a, account_b] = SetUpTwoAccounts();
  base::HistogramTester histogram_tester;

  // Empty target account ID.
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), CoreAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kEmptyTargetAccountId);

  // Unknown target account (missing refresh token).
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(),
                CoreAccountId::FromGaiaId(GaiaId("unknown_gaia")),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::
                kTargetAccountMissingRefreshToken);

  // Target account with persistent auth error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_b.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::
                kTargetAccountRefreshTokenInError);

  // Target account with refresh token removed completely.
  identity_test_env()->RemoveRefreshTokenForAccount(account_b.GetAccountId());
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::
                kTargetAccountMissingRefreshToken);

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
  histogram_tester.ExpectTotalCount("Signin.SignoutProfile", 0);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenSigninNotAllowed) {
  auto [account_a, account_b] = SetUpTwoAccounts();

  // Blocked sign-in cookies disallow switching.
  CookieSettingsFactory::GetForProfile(profile())->SetDefaultCookieSetting(
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSigninCookiesDisallowed);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());

  // Disable AccountsPolicyManager on a dedicated profile so toggling
  // `prefs::kSigninAllowed` does not immediately sign out `account_a`,
  // allowing verification that `SwitchPrimaryAccountWithinProfile()` rejects
  // before mutating primary-account state.
  TestingProfile* test_profile =
      CreateProfileWithoutAccountsPolicyManager("signin_disallowed_profile");
  IdentityTestEnvironmentProfileAdaptor adaptor(test_profile);

  AccountInfo profile_a =
      adaptor.identity_test_env()->MakePrimaryAccountAvailable(
          "account_a@gmail.com", signin::ConsentLevel::kSignin);
  AccountInfo profile_b =
      adaptor.identity_test_env()->MakeAccountAvailable("account_b@gmail.com");

  test_profile->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  base::HistogramTester histogram_tester;

  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                test_profile, local_state(), profile_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSigninNotAllowed);
  EXPECT_EQ(
      adaptor.identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      profile_a.GetAccountId());
  histogram_tester.ExpectTotalCount("Signin.SignoutProfile", 0);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       FailsWhenTargetDisallowedByUsernamePattern) {
  auto [account_a, account_b] =
      SetUpTwoAccounts("account_a@gmail.com", "account_b@googlemail.com");

  local_state()->SetString(prefs::kGoogleServicesUsernamePattern,
                           R"(.*@gmail\.com)");

  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(profile(), local_state(),
                                        account_b.GetAccountId(),
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kUsernameNotAllowedByPattern);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());

  // Passing a null `local_state` skips username pattern enforcement.
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), /*local_state=*/nullptr, account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenSignoutDisallowed) {
  auto [account_a, account_b] = SetUpTwoAccounts();
  base::HistogramTester histogram_tester;

  // Managed profile where user accepted account management.
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSignoutDisallowed);
  enterprise_util::SetUserAcceptedAccountManagement(profile(), false);

  // Force sign-in enabled.
  {
    signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
    EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                  profile(), local_state(), account_b.GetAccountId(),
                  signin_metrics::AccessPoint::kMenu),
              SwitchPrimaryAccountWithinProfileResult::kForceSigninEnabled);
  }

  // Signout disallowed directly by ChromeSigninClient.
  ChromeSigninClientFactory::GetForProfile(profile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSignoutDisallowed);

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
  histogram_tester.ExpectTotalCount("Signin.SignoutProfile", 0);
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       FailsWhenPrimaryAccountHasSyncConsent) {
  AccountInfo account_a = identity_test_env()->MakePrimaryAccountAvailable(
      "account_a@gmail.com", signin::ConsentLevel::kSync);
  AccountInfo account_b =
      identity_test_env()->MakeAccountAvailable("account_b@gmail.com");

  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(profile(), local_state(),
                                        account_b.GetAccountId(),
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kPrimaryAccountHasSyncConsent);
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
      account_a.GetAccountId());
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       FailsWhenTargetAccountIsManagedOrUnknownEnterprise) {
  auto [account_a, account_b] =
      SetUpTwoAccounts("account_a@gmail.com", "account_b@example.com");
  AccountInfo managed_account =
      identity_test_env()->MakeAccountAvailable("managed@corp.com");
  managed_account =
      AccountInfo::Builder(managed_account).SetHostedDomain("corp.com").Build();
  identity_test_env()->UpdateAccountInfoForAccount(managed_account);

  // Explicit managed hosted domain.
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), managed_account.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kTargetAccountManaged);

  // Unknown management status on a potentially enterprise domain fails closed.
  ASSERT_EQ(account_b.IsManaged(), signin::Tribool::kUnknown);
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::
                kTargetAccountManagementStatusUnknown);

  // Flex Org account: no hosted domain (`IsManaged() == kFalse`), but subject
  // to enterprise features (`IsMemberOfFlexOrg() == true`).
  account_b = AccountInfo::Builder(account_b)
                  .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                  .Build();
  AccountCapabilitiesTestMutator(&account_b)
      .set_is_subject_to_enterprise_features(true);
  identity_test_env()->UpdateAccountInfoForAccount(account_b);
  ASSERT_EQ(identity_manager()
                ->FindExtendedAccountInfoByAccountId(account_b.GetAccountId())
                .IsManaged(),
            signin::Tribool::kFalse);
  ASSERT_TRUE(identity_manager()
                  ->FindExtendedAccountInfoByAccountId(account_b.GetAccountId())
                  .IsMemberOfFlexOrg());
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kTargetAccountManaged);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());

  // Confirmed consumer non-well-known account is allowed to switch.
  AccountCapabilitiesTestMutator(&account_b)
      .set_is_subject_to_enterprise_features(false);
  identity_test_env()->UpdateAccountInfoForAccount(account_b);
  ASSERT_FALSE(
      identity_manager()
          ->FindExtendedAccountInfoByAccountId(account_b.GetAccountId())
          .IsMemberOfFlexOrg());
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                profile(), local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_b.GetAccountId());
}

TEST_F(SigninUtilSwitchPrimaryAccountTest, FailsWhenProfileSeparationRequired) {
  // Disable AccountsPolicyManager so configuring
  // `prefs::kProfileSeparationDomainExceptionList` does not retroactively
  // remove `account_b` before `SwitchPrimaryAccountWithinProfile()` runs.
  TestingProfile* test_profile =
      CreateProfileWithoutAccountsPolicyManager("profile_separation_profile");
  IdentityTestEnvironmentProfileAdaptor adaptor(test_profile);
  AccountInfo account_a =
      adaptor.identity_test_env()->MakePrimaryAccountAvailable(
          "account_a@gmail.com", signin::ConsentLevel::kSignin);
  AccountInfo account_b =
      adaptor.identity_test_env()->MakeAccountAvailable("account_b@gmail.com");

  // Configuring an exception list without "gmail.com" requires separation even
  // for consumer accounts.
  test_profile->GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList, base::ListValue());
  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(test_profile, local_state(),
                                        account_b.GetAccountId(),
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kProfileSeparationRequired);
  EXPECT_EQ(
      adaptor.identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      account_a.GetAccountId());

  // Exempting "gmail.com" allows the switch.
  base::ListValue exception_list;
  exception_list.Append("gmail.com");
  test_profile->GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList, std::move(exception_list));
  EXPECT_EQ(SwitchPrimaryAccountWithinProfile(
                test_profile, local_state(), account_b.GetAccountId(),
                signin_metrics::AccessPoint::kMenu),
            SwitchPrimaryAccountWithinProfileResult::kSuccess);
  EXPECT_EQ(
      adaptor.identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      account_b.GetAccountId());
}

TEST_F(SigninUtilSwitchPrimaryAccountTest,
       FailsWhenTargetSignedInOnOtherProfile) {
  auto [account_a, account_b] = SetUpTwoAccounts();

  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("other_profile");
  ProfileAttributesEntry* other_entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(other_profile->GetPath());
  ASSERT_NE(other_entry, nullptr);
  other_entry->SetAuthInfo(account_b.GetGaiaId(),
                           base::UTF8ToUTF16(account_b.GetEmail()),
                           /*is_consented_primary_account=*/false);

  EXPECT_EQ(
      SwitchPrimaryAccountWithinProfile(profile(), local_state(),
                                        account_b.GetAccountId(),
                                        signin_metrics::AccessPoint::kMenu),
      SwitchPrimaryAccountWithinProfileResult::kAccountSignedInOnOtherProfile);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      account_a.GetAccountId());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
