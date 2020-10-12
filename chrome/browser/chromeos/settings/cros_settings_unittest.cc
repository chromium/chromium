// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/cros_settings.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
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

namespace chromeos {

namespace {
constexpr char kOwner[] = "me@owner";
constexpr char kUser1[] = "h@xxor";
constexpr char kUser2[] = "l@mer";

void NotReached() {
  NOTREACHED()
      << "This should not be called: cros settings should already be trusted";
}

}  // namespace

class CrosSettingsTest : public testing::Test {
 protected:
  CrosSettingsTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        scoped_test_cros_settings_(local_state_.Get()) {}

  ~CrosSettingsTest() override {}

  void SetUp() override {
    device_policy_.Build();

    fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
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
  // OwnerSettingsServiceChromeOS::FixupLocalOwnerPolicy.
  OwnerSettingsServiceChromeOS* CreateOwnerSettingsService(
      const std::string& owner_email) {
    const AccountId account_id = AccountId::FromUserEmail(owner_email);
    user_manager_.AddUser(account_id);
    profile_ = std::make_unique<TestingProfile>();
    profile_->set_profile_name(account_id.GetUserEmail());

    OwnerSettingsServiceChromeOS* service =
        OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
            profile_.get());
    DCHECK(service);

    service->OnTPMTokenReady(true);
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
                                                NULL);
  }

  bool IsUserAllowed(const std::string& username,
                     const base::Optional<user_manager::UserType>& user_type) {
    return CrosSettings::Get()->IsUserAllowlisted(username, nullptr, user_type);
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};

  ScopedTestingLocalState local_state_;
  ScopedStubInstallAttributes scoped_install_attributes_;
  ScopedTestDeviceSettingsService scoped_test_device_settings_;
  ScopedTestCrosSettings scoped_test_cros_settings_;

  FakeChromeUserManager user_manager_;
  FakeSessionManagerClient fake_session_manager_client_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  policy::DevicePolicyBuilder device_policy_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(CrosSettingsTest, GetAndSetPref) {
  // False is the expected default value:
  ExpectPref(kAccountsPrefEphemeralUsersEnabled, base::Value(false));

  // Make sure we can set the value to true:
  auto* oss = CreateOwnerSettingsService(kOwner);
  oss->Set(kAccountsPrefEphemeralUsersEnabled, base::Value(true));
  ExpectPref(kAccountsPrefEphemeralUsersEnabled, base::Value(true));
}

TEST_F(CrosSettingsTest, SetAllowlist) {
  // Set a non-empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();

  StoreDevicePolicy();

  base::ListValue allowlist;
  allowlist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, allowlist);
  // When a non-empty allowlist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetAllowlistWithListOps) {
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();
  StoreDevicePolicy();

  auto* oss = CreateOwnerSettingsService(kOwner);

  base::ListValue original_list;
  original_list.AppendString(kOwner);
  oss->Set(kAccountsPrefUsers, original_list);
  task_environment_.RunUntilIdle();

  base::ListValue modified_list;
  modified_list.AppendString(kOwner);
  modified_list.AppendString(kUser1);

  // Add some user to the allowlist.
  oss->AppendToList(kAccountsPrefUsers, base::Value(kUser1));
  ExpectPref(kAccountsPrefUsers, modified_list);
  // When a non-empty allowlist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetAllowlistWithListOps2) {
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();
  StoreDevicePolicy();

  auto* oss = CreateOwnerSettingsService(kOwner);

  base::ListValue original_list;
  original_list.AppendString(kOwner);
  original_list.AppendString(kUser1);
  original_list.AppendString(kUser2);
  oss->Set(kAccountsPrefUsers, original_list);
  task_environment_.RunUntilIdle();

  base::ListValue modified_list;
  modified_list.AppendString(kOwner);
  modified_list.AppendString(kUser1);

  // Remove some user from the allowlist.
  oss->RemoveFromList(kAccountsPrefUsers, base::Value(kUser2));
  ExpectPref(kAccountsPrefUsers, modified_list);
  // When a non-empty allowlist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

// The following tests check that the allowlist / allow_new_users logic in
// DeviceSettings:Provider::DecodeLoginPolicies still works properly at this
// level, the CrosSettings API.
// They do not use OwnerSettingsService since having a local
// OwnerSettingsService constrains the policies in certain ways - see
// OwnerSettingsServiceChromeOS::FixupLocalOwnerPolicy.

TEST_F(CrosSettingsTest, SetEmptyAllowlist) {
  // Set an empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();
  StoreDevicePolicy();

  ExpectPref(kAccountsPrefUsers, base::ListValue());
  // When an empty allowlist is set, allow_new_user defaults to true.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, SetEmptyAllowlistAndDisallowNewUsers) {
  // Set an empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - an empty allowlist and no new users allowed.
  ExpectPref(kAccountsPrefUsers, base::ListValue());
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetAllowlistAndDisallowNewUsers) {
  // Set a non-empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - a non-empty allowlist and no new users allowed.
  base::ListValue allowlist;
  allowlist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, allowlist);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetEmptyAllowlistAndAllowNewUsers) {
  // Set an empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - an empty allowlist and new users allowed.
  ExpectPref(kAccountsPrefUsers, base::ListValue());
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, SetAllowlistAndAllowNewUsers) {
  // Set a non-empty allowlist.
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - a non-empty allowlist and new users allowed.
  base::ListValue allowlist;
  allowlist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, allowlist);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, FindEmailInList) {
  auto* oss = CreateOwnerSettingsService(kOwner);

  base::ListValue list;
  list.AppendString("user@example.com");
  list.AppendString("nodomain");
  list.AppendString("with.dots@gmail.com");
  list.AppendString("Upper@example.com");

  oss->Set(kAccountsPrefUsers, list);
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

  base::ListValue list;
  list.AppendString("user@example.com");
  list.AppendString("*@example.com");

  oss->Set(kAccountsPrefUsers, list);
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
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->clear_user_allowlist();
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, base::ListValue());
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(false));

  EXPECT_FALSE(IsUserAllowed(kUser1, base::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::USER_TYPE_CHILD));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::USER_TYPE_REGULAR));
}

// DeviceFamilyLinkAccountsAllowed should not have any effect if the feature is
// disabled.
TEST_F(CrosSettingsTest, AllowFamilyLinkAccountsWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chromeos::features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  base::ListValue allowlist;
  allowlist.AppendString(kOwner);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, allowlist);
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(false));

  EXPECT_TRUE(IsUserAllowed(kOwner, base::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, base::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::USER_TYPE_CHILD));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::USER_TYPE_REGULAR));
}

TEST_F(CrosSettingsTest, AllowFamilyLinkAccountsWithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kFamilyLinkOnSchoolDevice);

  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  device_policy_.payload().mutable_user_allowlist()->add_user_allowlist(kOwner);
  device_policy_.payload()
      .mutable_family_link_accounts_allowed()
      ->set_family_link_accounts_allowed(true);

  StoreDevicePolicy();

  base::ListValue allowlist;
  allowlist.AppendString(kOwner);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
  ExpectPref(kAccountsPrefUsers, allowlist);
  ExpectPref(kAccountsPrefFamilyLinkAccountsAllowed, base::Value(true));

  EXPECT_TRUE(IsUserAllowed(kOwner, base::nullopt));
  EXPECT_FALSE(IsUserAllowed(kUser1, base::nullopt));
  EXPECT_TRUE(IsUserAllowed(kUser1, user_manager::USER_TYPE_CHILD));
  EXPECT_FALSE(IsUserAllowed(kUser1, user_manager::USER_TYPE_REGULAR));
}

}  // namespace chromeos
