// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/cros_settings.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
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
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
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
        CrosSettings::Get()->PrepareTrustedValues(base::Bind(&NotReached)));

    const base::Value* pref_value = CrosSettings::Get()->GetPref(pref);
    EXPECT_TRUE(pref_value) << "for pref=" << pref;
    if (pref_value) {
      EXPECT_EQ(expected_value, *pref_value) << "for pref=" << pref;
    }
  }

  bool IsWhitelisted(const std::string& username) {
    return CrosSettings::Get()->FindEmailInList(kAccountsPrefUsers, username,
                                                NULL);
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

TEST_F(CrosSettingsTest, SetWhitelist) {
  // Set a non-empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->add_user_whitelist(kOwner);
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();

  StoreDevicePolicy();

  base::ListValue whitelist;
  whitelist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, whitelist);
  // When a non-empty whitelist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps) {
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

  // Add some user to the whitelist.
  oss->AppendToList(kAccountsPrefUsers, base::Value(kUser1));
  ExpectPref(kAccountsPrefUsers, modified_list);
  // When a non-empty whitelist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps2) {
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

  // Remove some user from the whitelist.
  oss->RemoveFromList(kAccountsPrefUsers, base::Value(kUser2));
  ExpectPref(kAccountsPrefUsers, modified_list);
  // When a non-empty whitelist is set, allow_new_user defaults to false.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

// The following tests check that the whitelist / allow_new_users logic in
// DeviceSettings:Provider::DecodeLoginPolicies still works properly at this
// level, the CrosSettings API.
// They do not use OwnerSettingsService since having a local
// OwnerSettingsService constrains the policies in certain ways - see
// OwnerSettingsServiceChromeOS::FixupLocalOwnerPolicy.

TEST_F(CrosSettingsTest, SetEmptyWhitelist) {
  // Set an empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->clear_user_whitelist();
  // Clear allow_new_users, so it is not set to true or false.
  device_policy_.payload().mutable_allow_new_users()->clear_allow_new_users();
  StoreDevicePolicy();

  ExpectPref(kAccountsPrefUsers, base::ListValue());
  // When an empty whitelist is set, allow_new_user defaults to true.
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, SetEmptyWhitelistAndDisallowNewUsers) {
  // Set an empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->clear_user_whitelist();
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - an empty whitelist and no new users allowed.
  ExpectPref(kAccountsPrefUsers, base::ListValue());
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetWhitelistAndDisallowNewUsers) {
  // Set a non-empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->add_user_whitelist(kOwner);
  // Set allow_new_users to false.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(
      false);
  StoreDevicePolicy();

  // Expect the same - a non-empty whitelist and no new users allowed.
  base::ListValue whitelist;
  whitelist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, whitelist);
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(false));
}

TEST_F(CrosSettingsTest, SetEmptyWhitelistAndAllowNewUsers) {
  // Set an empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->clear_user_whitelist();
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - an empty whitelist and new users allowed.
  ExpectPref(kAccountsPrefUsers, base::ListValue());
  ExpectPref(kAccountsPrefAllowNewUser, base::Value(true));
}

TEST_F(CrosSettingsTest, SetWhitelistAndAllowNewUsers) {
  // Set a non-empty whitelist.
  device_policy_.payload().mutable_user_whitelist()->add_user_whitelist(kOwner);
  // Set allow_new_users to true.
  device_policy_.payload().mutable_allow_new_users()->set_allow_new_users(true);
  StoreDevicePolicy();

  // Expect the same - a non-empty whitelist and new users allowed.
  base::ListValue whitelist;
  whitelist.AppendString(kOwner);
  ExpectPref(kAccountsPrefUsers, whitelist);
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

  EXPECT_TRUE(IsWhitelisted("user@example.com"));
  EXPECT_FALSE(IsWhitelisted("us.er@example.com"));
  EXPECT_TRUE(IsWhitelisted("USER@example.com"));
  EXPECT_FALSE(IsWhitelisted("user"));

  EXPECT_TRUE(IsWhitelisted("nodomain"));
  EXPECT_TRUE(IsWhitelisted("nodomain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted("no.domain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted("NO.DOMAIN"));

  EXPECT_TRUE(IsWhitelisted("with.dots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted("withdots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted("WITH.DOTS@gmail.com"));
  EXPECT_TRUE(IsWhitelisted("WITHDOTS"));

  EXPECT_TRUE(IsWhitelisted("Upper@example.com"));
  EXPECT_FALSE(IsWhitelisted("U.pper@example.com"));
  EXPECT_FALSE(IsWhitelisted("Upper"));
  EXPECT_TRUE(IsWhitelisted("upper@example.com"));
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

}  // namespace chromeos
