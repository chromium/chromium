// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/cros_settings.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_key_loader.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace ash {

namespace {
// For a user to be recognized as an owner, it needs to be the author of the
// device settings. So use the default user name that DevicePolicyBuilder uses.
const char* const kOwner = policy::PolicyBuilder::kFakeUsername;
constexpr char kUser1[] = "h@xxor";

void NotReached() {
  NOTREACHED_IN_MIGRATION()
      << "This should not be called: cros settings should already be trusted";
}

}  // namespace

class CrosSettingsTest : public testing::Test {
 protected:
  CrosSettingsTest() = default;
  ~CrosSettingsTest() override = default;

  void SetUp() override {
    // Disable owner key migration.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
        /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

    device_policy_.Build();

    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

    // Initialize ProfileHelper including BrowserContextHelper.
    ProfileHelper::Get();

    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    owner_key_util_->ImportPrivateKeyAndSetPublicKey(
        device_policy_.GetSigningKey());
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    DeviceSettingsService::Get()->SetSessionManager(
        &fake_session_manager_client_, owner_key_util_);
    DeviceSettingsService::Get()->Load();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    DeviceSettingsService::Get()->UnsetSessionManager();
  }

  // Some tests below use an OwnerSettingsService so they can change settings
  // partway through the test - this sets one up for those tests that need it.
  // Other tests below cannot use an OwnerSettingsService, since they change
  // |device_policy_| to something that would not be allowed by
  // OwnerSettingsServiceAsh::FixupLocalOwnerPolicy.
  OwnerSettingsServiceAsh* CreateOwnerSettingsService(
      const std::string& owner_email) {
    const AccountId account_id = AccountId::FromUserEmail(owner_email);
    profile_ = std::make_unique<TestingProfile>();
    profile_->set_profile_name(account_id.GetUserEmail());
    ash::AnnotatedAccountId::Set(profile_.get(), account_id);

    FakeNssService::InitializeForBrowserContext(profile_.get(),
                                                /*enable_system_slot=*/false);
    OwnerSettingsServiceAsh* service =
        OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
    DCHECK(service);

    service->OnTPMTokenReady();
    task_environment_.RunUntilIdle();
    DCHECK(service->IsOwner());
    return service;
  }

  void StoreDevicePolicy() {
    device_policy_.Build();
    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());
    DeviceSettingsService::Get()->OwnerKeySet(true);
    task_environment_.RunUntilIdle();
  }

  void ExpectPref(const std::string& pref, const base::Value& expected_value) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // RunUntilIdle ensures that any changes recently made to CrosSettings will
    // be complete by the time that we make the assertions below.
    task_environment_.RunUntilIdle();

    // ExpectPref checks that the given pref has the given value, and that the
    // value is TRUSTED - that means is not just a best-effort value that is
    // being returned until we can load the real value.
    // Calling RunUntilIdle() above ensures that there is enough time to find
    // the TRUSTED values.
    EXPECT_EQ(
        CrosSettingsProvider::TRUSTED,
        CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(&NotReached)));

    const base::Value* pref_value = CrosSettings::Get()->GetPref(pref);
    EXPECT_TRUE(pref_value) << "for pref=" << pref;
    if (pref_value) {
      EXPECT_EQ(expected_value, *pref_value) << "for pref=" << pref;
    }
  }

  bool IsAllowlisted(const std::string& username) {
    return CrosSettings::Get()->FindEmailInList(kAccountsPrefUsers, username,
                                                nullptr);
  }

  bool IsUserAllowed(const std::string& username,
                     const std::optional<user_manager::UserType>& user_type) {
    return CrosSettings::Get()->IsUserAllowlisted(username, nullptr, user_type);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  ScopedStubInstallAttributes scoped_install_attributes_;
  ScopedTestDeviceSettingsService scoped_test_device_settings_;
  CrosSettingsHolder cros_settings_holder_{ash::DeviceSettingsService::Get(),
                                           local_state_.Get()};

  FakeSessionManagerClient fake_session_manager_client_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  policy::DevicePolicyBuilder device_policy_;
  std::unique_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CrosSettingsTest, GetAndSetPref) {
  // False is the expected default value:
  ExpectPref(kDevicePeripheralDataAccessEnabled, base::Value(false));

  // Make sure we can set the value to true:
  auto* oss = CreateOwnerSettingsService(kOwner);
  oss->Set(kDevicePeripheralDataAccessEnabled, base::Value(true));
  ExpectPref(kDevicePeripheralDataAccessEnabled, base::Value(true));
}

TEST_F(CrosSettingsTest, SetAllowlistWithListOps) {
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  StoreDevicePolicy();

  auto* oss = CreateOwnerSettingsService(kOwner);

  base::Value::List original_list;
  original_list.Append(kOwner);
  oss->Set(kAccountsPrefUsers, base::Value(std::move(original_list)));
  task_environment_.RunUntilIdle();

  base::Value::List modified_list;
  modified_list.Append(kOwner);
  modified_list.Append(kUser1);

  // Add some user to the allowlist.
  oss->AppendToList(kAccountsPrefUsers, base::Value(kUser1));
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(modified_list)));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

// The following tests check that the allowlist / allow_new_users logic in
// DeviceSettings:Provider::DecodeLoginPolicies still works properly at this
// level, the CrosSettings API.
// They do not use OwnerSettingsService since having a local
// OwnerSettingsService constrains the policies in certain ways - see
// OwnerSettingsServiceAsh::FixupLocalOwnerPolicy.

TEST_F(CrosSettingsTest, AllowAnyUserToSignIn) {
  // Set an empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - an empty allowlist and new users allowed.
  ExpectPref(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, RestrictSignInToAListOfUsers) {
  // Set a non-empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - a non-empty allowlist and no new users allowed.
  base::Value::List allowlist;
  allowlist.Append(kOwner);
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, DoNotAllowAnyUserToSignIn) {
  // Set an empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - an empty allowlist and no new users allowed.
  ExpectPref(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, DefaultPolicyValues) {
  // Set an empty allowlist.
  device_policy_.payload().clear_user_allowlist();
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();
  StoreDevicePolicy();

  ExpectPref(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  // When an empty allowlist is set, allow_new_user defaults to true.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

// This case is not a valid DM server combination, but it is possible
// for consumer devices, it should be semantically equivalent to
// allowing all users to sign in
TEST_F(CrosSettingsTest, ConsumerOwnedDefaultState) {
  // Set a non-empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - a non-empty allowlist and new users allowed.
  base::Value::List allowlist;
  allowlist.Append(kOwner);
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

// It's possible for the user_allowlist to be not present, and for the
// user_whitelist to be present instead. This test simulates this by
// doing something similar to the "RestrictSignInToAListOfUsers" test
// but using user_whitelist instead
TEST_F(CrosSettingsTest, WhitelistUsedWhenAllowlistNotPresent) {
  // clear user_allowlist
  device_policy_.payload().clear_user_allowlist();
  // set non-empty user_whitelist
  device_policy_.payload().mutable_user_whitelist()->add_user_whitelist(kOwner);
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  histogram_tester_.ExpectUniqueSample(kAllowlistCOILFallbackHistogram, true,
                                       1);

  // Expect the same - a non-empty allowlist and no new users allowed.
  base::Value::List allowlist;
  allowlist.Append(kOwner);
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

// In cases where both the user_allowlist and the user_whitelist are present,
// we should use the user_allowlist. This test simulates this by
// doing something similar to the "RestrictSignInToAListOfUsers" test
// but providing both user_allowlist and user_whitelist, and asserting that
// user_allowlist is being used.
TEST_F(CrosSettingsTest, AllowlistUsedWhenAllowlistAndWhitelistPresent) {
  // clear user_allowlist
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kUser1);
  // set non-empty user_whitelist
  device_policy_.payload().mutable_user_whitelist()->add_user_whitelist(kOwner);
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  histogram_tester_.ExpectUniqueSample(kAllowlistCOILFallbackHistogram, false,
                                       1);

  // Expect the same - a non-empty allowlist and no new users allowed.
  base::Value::List allowlist;
  allowlist.Append(kUser1);
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, FindEmailInList) {
  auto* oss = CreateOwnerSettingsService(kOwner);

  base::Value::List list;
  list.Append("user@example.com");
  list.Append("nodomain");
  list.Append("with.dots@gmail.com");
  list.Append("Upper@example.com");

  oss->Set(kAccountsPrefUsers, base::Value(std::move(list)));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(IsAllowlisted("user@example.com"));
  EXPECT_FALSE(IsAllowlisted("us.er@example.com"));
  EXPECT_TRUE(IsAllowlisted("USER@example.com"));
  EXPECT_FALSE(IsAllowlisted("user"));

  EXPECT_TRUE(IsAllowlisted("nodomain"));
  EXPECT_TRUE(IsAllowlisted("nodomain@gmail.com"));
  EXPECT_TRUE(IsAllowlisted("no.domain@gmail.com"));
  EXPECT_TRUE(IsAllowlisted("NO.DOMAIN"));

  EXPECT_TRUE(IsAllowlisted("with.dots@gmail.com"));
  EXPECT_TRUE(IsAllowlisted("withdots@gmail.com"));
  EXPECT_TRUE(IsAllowlisted("WITH.DOTS@gmail.com"));
  EXPECT_TRUE(IsAllowlisted("WITHDOTS"));

  EXPECT_TRUE(IsAllowlisted("Upper@example.com"));
  EXPECT_FALSE(IsAllowlisted("U.pper@example.com"));
  EXPECT_FALSE(IsAllowlisted("Upper"));
  EXPECT_TRUE(IsAllowlisted("upper@example.com"));
}

TEST_F(CrosSettingsTest, FindEmailInListWildcard) {
  auto* oss = CreateOwnerSettingsService(kOwner);

  base::Value::List list;
  list.Append("user@example.com");
  list.Append("*@example.com");

  oss->Set(kAccountsPrefUsers, base::Value(std::move(list)));
  task_environment_.RunUntilIdle();

  bool wildcard_match = false;
  EXPECT_TRUE(CrosSettings::Get()->FindEmailInList(
      kAccountsPrefUsers, "test@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
  EXPECT_TRUE(CrosSettings::Get()->FindEmailInList(
      kAccountsPrefUsers, "user@example.com", &wildcard_match));
  EXPECT_FALSE(wildcard_match);
  EXPECT_TRUE(CrosSettings::Get()->FindEmailInList(
      kAccountsPrefUsers, "*@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
}

// DeviceFamilyLinkAccountsAllowed should not have any effect if allowlist is
// not set.
TEST_F(CrosSettingsTest, AllowFamilyLinkAccountsWithEmptyAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(false));

  EXPECT_FALSE(IsUserAllowed(kUser1, std::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::UserType::kChild));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::UserType::kRegular));
}

// DeviceFamilyLinkAccountsAllowed should not have any effect if the feature is
// disabled.
TEST_F(CrosSettingsTest, AllowFamilyLinkAccountsWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  base::Value::List allowlist;
  allowlist.Append(kOwner);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(false));

  EXPECT_TRUE(IsUserAllowed(kOwner, std::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, std::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::UserType::kChild));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::UserType::kRegular));
}

TEST_F(CrosSettingsTest, AllowFamilyLinkAccountsWithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  base::Value::List allowlist;
  allowlist.Append(kOwner);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, base::Value(std::move(allowlist)));
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(true));

  EXPECT_TRUE(IsUserAllowed(kOwner, std::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, std::nullopt));
  EXPECT_TRUE(IsUserAllowed(kUser1, user_manager::UserType::kChild));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::UserType::kRegular));
}

}  // namespace ash
