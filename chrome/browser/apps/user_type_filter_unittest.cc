// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/user_type_filter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

// Helper that simulates Json file with embedded user type filter.
base::Value::Dict CreateJsonWithFilter(
    const std::vector<std::string>& user_types) {
  base::Value::List filter;
  for (const auto& user_type : user_types)
    filter.Append(base::Value(user_type));
  base::Value::Dict root;
  root.Set(kKeyUserType, std::move(filter));
  return root;
}

}  // namespace

class UserTypeFilterTest : public testing::Test {
 public:
  UserTypeFilterTest() = default;
  UserTypeFilterTest(const UserTypeFilterTest&) = delete;
  UserTypeFilterTest& operator=(const UserTypeFilterTest&) = delete;
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
             const base::Value::Dict& json_root) {
    return UserTypeMatchesJsonUserType(DetermineUserType(profile.get()),
                                       std::string() /* app_id */, json_root,
                                       nullptr /* default_user_types */);
  }

  bool MatchDefault(const std::unique_ptr<TestingProfile>& profile,
                    const base::Value::List& default_user_types) {
    base::Value::Dict json_root;
    return UserTypeMatchesJsonUserType(DetermineUserType(profile.get()),
                                       std::string() /* app_id */, json_root,
                                       &default_user_types);
  }

 private:
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserTypeFilterTest, ChildUser) {
  const auto profile = CreateProfile();
  profile->SetIsSupervisedProfile();
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeUnmanaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeChild})));
  EXPECT_TRUE(Match(
      profile, CreateJsonWithFilter({kUserTypeUnmanaged, kUserTypeChild})));
}

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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(UserTypeFilterTest, ManagedGuestUser) {
  profiles::testing::ScopedTestManagedGuestSession test_managed_guest_session;
  const auto profile = CreateProfile();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(Match(profile, CreateJsonWithFilter({kUserTypeManaged})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter({kUserTypeManagedGuest})));
  EXPECT_TRUE(Match(profile, CreateJsonWithFilter(
                                 {kUserTypeUnmanaged, kUserTypeManagedGuest})));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(UserTypeFilterTest, UnmanagedUser) {
  EXPECT_TRUE(
      Match(CreateProfile(), CreateJsonWithFilter({kUserTypeUnmanaged})));
}

TEST_F(UserTypeFilterTest, EmptyFilter) {
  EXPECT_FALSE(Match(CreateProfile(), CreateJsonWithFilter({})));
}

TEST_F(UserTypeFilterTest, DefaultFilter) {
  auto profile = CreateProfile();
  base::Value::List default_filter;
  default_filter.Append(base::Value(kUserTypeUnmanaged));
  default_filter.Append(base::Value(kUserTypeGuest));

  // Unmanaged user.
  EXPECT_TRUE(MatchDefault(profile, default_filter));
  // Guest user.
  EXPECT_TRUE(MatchDefault(CreateGuestProfile(), default_filter));
  // Child user.
  profile->SetIsSupervisedProfile();
  EXPECT_FALSE(MatchDefault(profile, default_filter));
  // Managed user.
  profile = CreateProfile();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(MatchDefault(profile, default_filter));
}

}  // namespace apps
