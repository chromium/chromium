// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/user_type_filter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

// Helper that simulates Json file with embedded user type filter.
std::unique_ptr<base::DictionaryValue> CreateJsonWithFilter(
    const std::vector<std::string>& user_types) {
  auto root = std::make_unique<base::DictionaryValue>();
  base::ListValue filter;
  for (const auto& user_type : user_types)
    filter.Append(base::Value(user_type));
  root->SetKey(kKeyUserType, std::move(filter));
  return root;
}

}  // namespace

class UserTypeFilterTest : public testing::Test {
 public:
  UserTypeFilterTest() = default;
  ~UserTypeFilterTest() override = default;

 protected:
  // Helper that creates simple test profile.
  std::unique_ptr<TestingProfile> CreateProfile() {
    TestingProfile::Builder profile_builder;
    return profile_builder.Build();
  }

  // Helper that creates simple test guest profile.
  std::unique_ptr<TestingProfile> CreateGuestProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetGuestSession();
    return profile_builder.Build();
  }

  bool Match(const std::unique_ptr<TestingProfile>& profile,
             const std::unique_ptr<base::Value>& json_root) {
    return UserTypeMatchesJsonUserType(
        DetermineUserType(profile.get()), std::string() /* app_id */,
        json_root.get(), nullptr /* default_user_types */);
  }

  bool MatchDefault(const std::unique_ptr<TestingProfile>& profile,
                    const base::ListValue& default_user_types) {
    base::DictionaryValue json_root;
    return UserTypeMatchesJsonUserType(DetermineUserType(profile.get()),
                                       std::string() /* app_id */, &json_root,
                                       &default_user_types);
  }

 private:
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(UserTypeFilterTest);
};

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(UserTypeFilterTest, ChildUser) {
  const auto profile = CreateProfile();
  profile->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeUnmanaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeChild})));
  EXPECT_TRUE(Match(
      profile, CreateJsonWithFilter({kUserTypeUnmanaged, kUserTypeChild})));
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

TEST_F(UserTypeFilterTest, GuestUser) {
  auto profile = CreateGuestProfile();
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeUnmanaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeGuest})));
  EXPECT_TRUE(Match(
      profile, CreateJsonWithFilter({kUserTypeUnmanaged, kUserTypeGuest})));
}

TEST_F(UserTypeFilterTest, ManagedUser) {
  const auto profile = CreateProfile();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeUnmanaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeManaged})));
  EXPECT_TRUE(Match(
      profile, CreateJsonWithFilter({kUserTypeUnmanaged, kUserTypeManaged})));
}

TEST_F(UserTypeFilterTest, SupervisedUser) {
  const auto profile = CreateProfile();
  profile->SetSupervisedUserId("asdf");
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeUnmanaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeSupervised})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter(
                                 {kUserTypeUnmanaged, kUserTypeSupervised})));
}

TEST_F(UserTypeFilterTest, UnmanagedUser) {
  EXPECT_TRUE(
      Match(CreateProfile(), CreateJsonWithFilter({kUserTypeUnmanaged})));
}

TEST_F(UserTypeFilterTest, EmptyFilter) {
  EXPECT_FALSE(Match(CreateProfile(), CreateJsonWithFilter({})));
}

TEST_F(UserTypeFilterTest, DefaultFilter) {
  auto profile = CreateProfile();
  base::ListValue default_filter;
  default_filter.Append(base::Value(kUserTypeUnmanaged));
  default_filter.Append(base::Value(kUserTypeGuest));

  // Unmanaged user.
  EXPECT_TRUE(MatchDefault(profile, default_filter));
  // Guest user.
  EXPECT_TRUE(MatchDefault(CreateGuestProfile(), default_filter));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Child user.
  profile->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  EXPECT_FALSE(MatchDefault(profile, default_filter));
  // Supervised user.
  // TODO(crbug.com/971311): Remove the next assert test once legacy supervised
  // user code has been fully removed.
  profile->SetSupervisedUserId("asdf");
  EXPECT_FALSE(MatchDefault(profile, default_filter));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Managed user.
  profile = CreateProfile();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(MatchDefault(profile, default_filter));
}

}  // namespace apps
