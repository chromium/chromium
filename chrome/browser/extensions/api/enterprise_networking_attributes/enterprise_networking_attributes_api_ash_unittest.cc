// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/extensions/api/enterprise_networking_attributes/enterprise_networking_attributes_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_networking_attributes.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kAccountEmail[] = "test_1@example.com";
constexpr char kSecondaryAccountEmail[] = "test_2@example.com";

// Default network properties configured by `ash::NetworkHandlerTestHelper`
// (via `ash::FakeShillManagerClient::SetupDefaultEnvironment()`).
constexpr char kFormattedMacAddress[] = "01:23:45:67:89:AB";
constexpr char kIpv4Address[] = "100.0.0.1";
constexpr char kIpv6Address[] = "0:0:0:0:100:0:0:1";

enum class TestProfileChoice {
  kSigninProfile,
  kNonAffiliatedProfile,
  kAffiliatedProfile,
  kNonAffiliatedSecondaryProfile,
  kAffiliatedSecondaryProfile,
};

std::string ParamToString(
    const testing::TestParamInfo<TestProfileChoice>& info) {
  switch (info.param) {
    case TestProfileChoice::kSigninProfile:
      return "SigninProfile";
    case TestProfileChoice::kNonAffiliatedProfile:
      return "NonAffiliatedProfile";
    case TestProfileChoice::kAffiliatedProfile:
      return "AffiliatedProfile";
    case TestProfileChoice::kNonAffiliatedSecondaryProfile:
      return "NonAffiliatedSecondaryProfile";
    case TestProfileChoice::kAffiliatedSecondaryProfile:
      return "AffiliatedSecondaryProfile";
  }
}

}  // namespace

class EnterpriseNetworkingAttributesApiAshTest
    : public ash::DeviceSettingsTestBase,
      public testing::WithParamInterface<TestProfileChoice> {
 protected:
  EnterpriseNetworkingAttributesApiAshTest()
      : ash::DeviceSettingsTestBase(/*profile_creation_enabled=*/false),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    ash::DeviceSettingsTestBase::SetUp();

    switch (GetParam()) {
      case TestProfileChoice::kSigninProfile:
        testing_profile_ = ash::ProfileHelper::GetSigninProfile();
        break;
      case TestProfileChoice::kNonAffiliatedProfile:
        testing_profile_ = AddUser(kAccountEmail, /*is_affiliated=*/false);
        break;
      case TestProfileChoice::kAffiliatedProfile:
        testing_profile_ = AddUser(kAccountEmail, /*is_affiliated=*/true);
        break;
      case TestProfileChoice::kNonAffiliatedSecondaryProfile:
        AddUser(kAccountEmail, /*is_affiliated=*/true);
        testing_profile_ =
            AddUser(kSecondaryAccountEmail, /*is_affiliated=*/false);
        break;
      case TestProfileChoice::kAffiliatedSecondaryProfile:
        AddUser(kAccountEmail, /*is_affiliated=*/false);
        testing_profile_ =
            AddUser(kSecondaryAccountEmail, /*is_affiliated=*/true);
        break;
    }
  }

  Profile* AddUser(std::string_view account_email, bool is_affiliated) {
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(std::string(account_email));
    AccountId account_id = AccountId::FromUserEmail(account_email);
    user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    user_manager_->LoginUser(account_id);
    return profile;
  }

  bool IsSigninProfileOrBelongsToAffiliatedUser() const {
    switch (GetParam()) {
      case TestProfileChoice::kSigninProfile:
      case TestProfileChoice::kAffiliatedProfile:
      case TestProfileChoice::kAffiliatedSecondaryProfile:
        return true;
      case TestProfileChoice::kNonAffiliatedProfile:
      case TestProfileChoice::kNonAffiliatedSecondaryProfile:
        return false;
    }
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfileManager profile_manager_;
  raw_ptr<Profile> testing_profile_ = nullptr;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_P(EnterpriseNetworkingAttributesApiAshTest, GetNetworkDetailsFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseNetworkingAttributesGetNetworkDetailsFunction>();

  if (IsSigninProfileOrBelongsToAffiliatedUser()) {
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), /*args=*/"[]", testing_profile_);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_dict());
    api::enterprise_networking_attributes::NetworkDetails expected_details;
    expected_details.mac_address = kFormattedMacAddress;
    expected_details.ipv4 = kIpv4Address;
    expected_details.ipv6 = kIpv6Address;
    EXPECT_EQ(expected_details.ToValue(), result->GetDict());
  } else {
    EXPECT_FALSE(api_test_utils::RunFunction(function.get(), /*args=*/"[]",
                                             testing_profile_));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EnterpriseNetworkingAttributesApiAshTest,
    testing::Values(TestProfileChoice::kSigninProfile,
                    TestProfileChoice::kAffiliatedProfile,
                    TestProfileChoice::kNonAffiliatedProfile,
                    TestProfileChoice::kNonAffiliatedSecondaryProfile,
                    TestProfileChoice::kAffiliatedSecondaryProfile),
    &ParamToString);

}  // namespace extensions
