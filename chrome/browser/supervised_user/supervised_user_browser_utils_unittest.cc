// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"

#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kChildEmail[] = "name@gmail.com";
constexpr char kChildGivenName[] = "Name";
}  // namespace

class SupervisedUserBrowserUtilsTest : public ::testing::Test {
 public:
  SupervisedUserBrowserUtilsTest();

  signin::IdentityTestEnvironment* GetIdentityTestEnv();

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

SupervisedUserBrowserUtilsTest::SupervisedUserBrowserUtilsTest() {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  profile_ = IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(builder);

  identity_test_env_profile_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

  AccountInfo account_info = GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  supervised_user_test_util::PopulateAccountInfoWithName(account_info,
                                                         kChildGivenName);
  GetIdentityTestEnv()->UpdateAccountInfoForAccount(account_info);
}

signin::IdentityTestEnvironment*
SupervisedUserBrowserUtilsTest::GetIdentityTestEnv() {
  return identity_test_env_profile_adaptor_->identity_test_env();
}

TEST_F(SupervisedUserBrowserUtilsTest, GetAccountGivenName) {
  ASSERT_NE(nullptr, profile());
  EXPECT_EQ(kChildGivenName, supervised_user::GetAccountGivenName(*profile()));
}
