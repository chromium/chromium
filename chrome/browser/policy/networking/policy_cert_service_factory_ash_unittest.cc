// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/policy_cert_service_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUserEmail[] = "test@example.com";

class PolicyCertServiceFactoryMigrationTest : public testing::Test {
 public:
  PolicyCertServiceFactoryMigrationTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        profile_manager_(TestingBrowserProcess::GetGlobal(), &local_state_) {}
  ~PolicyCertServiceFactoryMigrationTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kUserEmail);
    ASSERT_TRUE(profile_);
  }

  void SetLocalState(std::vector<std::string> values) {
    auto local_pref_value = std::make_unique<base::ListValue>();
    for (auto&& value : values) {
      local_pref_value->Append(base::Value(std::move(value)));
    }
    local_state_.Get()->SetUserPref(prefs::kUsedPolicyCertificates,
                                    std::move(local_pref_value));
  }

  bool LocalStateContains(const char* value) {
    return base::Contains(
        local_state_.Get()->GetValueList(prefs::kUsedPolicyCertificates),
        base::Value(value));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;
};

TEST_F(PolicyCertServiceFactoryMigrationTest, ExistingPrefMigrated) {
  SetLocalState({kUserEmail});
  ASSERT_TRUE(LocalStateContains(kUserEmail));

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::MigrateLocalStatePrefIntoProfilePref(
          kUserEmail, profile_));

  auto policy_cert_service =
      policy::PolicyCertService::CreateForTesting(profile_);
  EXPECT_TRUE(policy_cert_service->UsedPolicyCertificates());

  EXPECT_FALSE(LocalStateContains(kUserEmail));
}

TEST_F(PolicyCertServiceFactoryMigrationTest, ManyUsersOneMigrated) {
  constexpr char email_1[] = "email_1@example.com";
  constexpr char email_2[] = "email_2@example.com";

  SetLocalState({email_1, kUserEmail, email_2});
  ASSERT_TRUE(LocalStateContains(email_1));
  ASSERT_TRUE(LocalStateContains(kUserEmail));
  ASSERT_TRUE(LocalStateContains(email_2));

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::MigrateLocalStatePrefIntoProfilePref(
          kUserEmail, profile_));

  auto policy_cert_service =
      policy::PolicyCertService::CreateForTesting(profile_);
  EXPECT_TRUE(policy_cert_service->UsedPolicyCertificates());

  ASSERT_TRUE(LocalStateContains(email_1));
  ASSERT_FALSE(LocalStateContains(kUserEmail));
  ASSERT_TRUE(LocalStateContains(email_2));
}

TEST_F(PolicyCertServiceFactoryMigrationTest, LocalStatePrefEmpty) {
  ASSERT_FALSE(LocalStateContains(kUserEmail));

  // Returns false if the user is not found in the local-state
  // kUsedPolicyCertificates preference.
  ASSERT_FALSE(
      policy::PolicyCertServiceFactory::MigrateLocalStatePrefIntoProfilePref(
          kUserEmail, profile_));

  auto policy_cert_service =
      policy::PolicyCertService::CreateForTesting(profile_);
  EXPECT_FALSE(policy_cert_service->UsedPolicyCertificates());
}

TEST_F(PolicyCertServiceFactoryMigrationTest, UserNotInLocalStatePref) {
  constexpr char email_1[] = "email_1@example.com";
  SetLocalState({email_1});
  ASSERT_TRUE(LocalStateContains(email_1));
  ASSERT_FALSE(LocalStateContains(kUserEmail));

  // Returns false if the user is not found in the local-state
  // kUsedPolicyCertificates preference.
  ASSERT_FALSE(
      policy::PolicyCertServiceFactory::MigrateLocalStatePrefIntoProfilePref(
          kUserEmail, profile_));

  auto policy_cert_service =
      policy::PolicyCertService::CreateForTesting(profile_);
  EXPECT_FALSE(policy_cert_service->UsedPolicyCertificates());

  ASSERT_TRUE(LocalStateContains(email_1));
  ASSERT_FALSE(LocalStateContains(kUserEmail));
}

}  // namespace
