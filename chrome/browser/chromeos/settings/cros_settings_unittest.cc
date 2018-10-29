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
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class CrosSettingsTest : public testing::Test {
 protected:
  CrosSettingsTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        settings_(DeviceSettingsService::Get(), local_state_.Get()),
        weak_factory_(this) {}

  ~CrosSettingsTest() override {}

  void TearDown() override {
    ASSERT_TRUE(expected_props_.empty());
  }

  void FetchPref(const std::string& pref) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (expected_props_.find(pref) == expected_props_.end())
      return;

    if (CrosSettingsProvider::TRUSTED ==
            settings_.PrepareTrustedValues(
                base::Bind(&CrosSettingsTest::FetchPref,
                           weak_factory_.GetWeakPtr(), pref))) {
      std::unique_ptr<base::Value> expected_value =
          std::move(expected_props_.find(pref)->second);
      const base::Value* pref_value = settings_.GetPref(pref);
      if (expected_value.get()) {
        ASSERT_TRUE(pref_value);
        ASSERT_TRUE(expected_value->Equals(pref_value));
      } else {
        ASSERT_FALSE(pref_value);
      }
      expected_props_.erase(pref);
    }
  }

  void SetPref(const std::string& pref_name, const base::Value* value) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    settings_.Set(pref_name, *value);
  }

  void AddExpectation(const std::string& pref_name,
                      std::unique_ptr<base::Value> value) {
    expected_props_[pref_name] = std::move(value);
  }

  void PrepareEmptyPolicy(em::PolicyData* policy) {
    // Prepare some policy blob.
    em::PolicyFetchResponse response;
    em::ChromeDeviceSettingsProto pol;
    policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
    policy->set_username("me@owner");
    policy->set_policy_value(pol.SerializeAsString());
    // Wipe the signed settings store.
    response.set_policy_data(policy->SerializeAsString());
    response.set_policy_data_signature("false");
  }

  static bool IsWhitelisted(CrosSettings* cs, const std::string& username) {
    return cs->FindEmailInList(kAccountsPrefUsers, username, NULL);
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  ScopedTestingLocalState local_state_;
  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  CrosSettings settings_;

  std::map<std::string, std::unique_ptr<base::Value>> expected_props_;

  base::WeakPtrFactory<CrosSettingsTest> weak_factory_;
};

TEST_F(CrosSettingsTest, SetPref) {
  // Change to something that is not the default.
  AddExpectation(kAccountsPrefAllowGuest, std::make_unique<base::Value>(false));
  SetPref(kAccountsPrefAllowGuest,
          expected_props_[kAccountsPrefAllowGuest].get());
  FetchPref(kAccountsPrefAllowGuest);
  ASSERT_TRUE(expected_props_.empty());
}

TEST_F(CrosSettingsTest, GetPref) {
  // We didn't change the default so look for it.
  AddExpectation(kAccountsPrefAllowGuest, std::make_unique<base::Value>(true));
  FetchPref(kAccountsPrefAllowGuest);
}

TEST_F(CrosSettingsTest, SetWhitelist) {
  // Setting the whitelist should also switch the value of
  // kAccountsPrefAllowNewUser to false.
  base::ListValue whitelist;
  whitelist.AppendString("me@owner");
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(false));
  AddExpectation(kAccountsPrefUsers, whitelist.CreateDeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps) {
  std::unique_ptr<base::ListValue> whitelist =
      std::make_unique<base::ListValue>();
  base::Value hacky_user("h@xxor");
  whitelist->Append(hacky_user.CreateDeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(false));
  AddExpectation(kAccountsPrefUsers, std::move(whitelist));
  // Add some user to the whitelist.
  settings_.AppendToList(kAccountsPrefUsers, &hacky_user);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistWithListOps2) {
  base::ListValue whitelist;
  base::Value hacky_user("h@xxor");
  base::Value lamy_user("l@mer");
  whitelist.Append(hacky_user.CreateDeepCopy());
  std::unique_ptr<base::ListValue> expected_list = whitelist.CreateDeepCopy();
  whitelist.Append(lamy_user.CreateDeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(false));
  AddExpectation(kAccountsPrefUsers, whitelist.CreateDeepCopy());
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
  ASSERT_TRUE(expected_props_.empty());
  // Now try to remove one element from that list.
  AddExpectation(kAccountsPrefUsers, std::move(expected_list));
  settings_.RemoveFromList(kAccountsPrefUsers, &lamy_user);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetEmptyWhitelist) {
  // Setting the whitelist empty should switch the value of
  // kAccountsPrefAllowNewUser to true.
  base::ListValue whitelist;
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(true));
  SetPref(kAccountsPrefUsers, &whitelist);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetEmptyWhitelistAndNoNewUsers) {
  // Setting the whitelist empty and disallowing new users should result in no
  // new users allowed.
  base::ListValue whitelist;
  base::Value disallow_new(false);
  AddExpectation(kAccountsPrefUsers, whitelist.CreateDeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(false));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser, &disallow_new);
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetWhitelistAndNoNewUsers) {
  // Setting the whitelist should allow us to set kAccountsPrefAllowNewUser to
  // false (which is the implicit value too).
  base::ListValue whitelist;
  whitelist.AppendString("me@owner");
  AddExpectation(kAccountsPrefUsers, whitelist.CreateDeepCopy());
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(false));
  SetPref(kAccountsPrefUsers, &whitelist);
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser].get());
  FetchPref(kAccountsPrefAllowNewUser);
  FetchPref(kAccountsPrefUsers);
}

TEST_F(CrosSettingsTest, SetAllowNewUsers) {
  // Setting kAccountsPrefAllowNewUser to true with no whitelist should be ok.
  AddExpectation(kAccountsPrefAllowNewUser,
                 std::make_unique<base::Value>(true));
  SetPref(kAccountsPrefAllowNewUser,
          expected_props_[kAccountsPrefAllowNewUser].get());
  FetchPref(kAccountsPrefAllowNewUser);
}

TEST_F(CrosSettingsTest, SetEphemeralUsersEnabled) {
  base::Value ephemeral_users_enabled(true);
  AddExpectation(kAccountsPrefEphemeralUsersEnabled,
                 std::make_unique<base::Value>(true));
  SetPref(kAccountsPrefEphemeralUsersEnabled, &ephemeral_users_enabled);
  FetchPref(kAccountsPrefEphemeralUsersEnabled);
}

TEST_F(CrosSettingsTest, FindEmailInList) {
  base::ListValue list;
  list.AppendString("user@example.com");
  list.AppendString("nodomain");
  list.AppendString("with.dots@gmail.com");
  list.AppendString("Upper@example.com");

  CrosSettings* cs = &settings_;
  cs->Set(kAccountsPrefUsers, list);

  EXPECT_TRUE(IsWhitelisted(cs, "user@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "us.er@example.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "USER@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "user"));

  EXPECT_TRUE(IsWhitelisted(cs, "nodomain"));
  EXPECT_TRUE(IsWhitelisted(cs, "nodomain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "no.domain@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "NO.DOMAIN"));

  EXPECT_TRUE(IsWhitelisted(cs, "with.dots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "withdots@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "WITH.DOTS@gmail.com"));
  EXPECT_TRUE(IsWhitelisted(cs, "WITHDOTS"));

  EXPECT_TRUE(IsWhitelisted(cs, "Upper@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "U.pper@example.com"));
  EXPECT_FALSE(IsWhitelisted(cs, "Upper"));
  EXPECT_TRUE(IsWhitelisted(cs, "upper@example.com"));
}

TEST_F(CrosSettingsTest, FindEmailInListWildcard) {
  base::ListValue list;
  list.AppendString("user@example.com");
  list.AppendString("*@example.com");

  CrosSettings* cs = &settings_;
  cs->Set(kAccountsPrefUsers, list);

  bool wildcard_match = false;
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "test@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "user@example.com", &wildcard_match));
  EXPECT_FALSE(wildcard_match);
  EXPECT_TRUE(cs->FindEmailInList(
      kAccountsPrefUsers, "*@example.com", &wildcard_match));
  EXPECT_TRUE(wildcard_match);
}

}  // namespace chromeos
