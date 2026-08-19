// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

namespace policy {

class UserCloudPolicyStatusProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test_profile");
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
};

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(UserCloudPolicyStatusProviderTest, GetStatusMojoUnmanaged) {
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  UserCloudPolicyStatusProvider provider(&policy_manager, profile_);

  auto status = provider.GetStatusMojo();
  EXPECT_FALSE(status->error);
  EXPECT_TRUE(status->status.empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(UserCloudPolicyStatusProviderTest,
       GetStatusMojoManagedNoEnrollmentToken) {
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  auto* store_ptr = store.get();
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  policy_manager.core()->Connect(std::make_unique<MockCloudPolicyClient>());
  UserCloudPolicyStatusProvider provider(&policy_manager, profile_);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
  policy_data->set_username("user@example.com");
  store_ptr->set_policy_data_for_testing(std::move(policy_data));

  auto status = provider.GetStatusMojo();
  EXPECT_FALSE(status->error);
  EXPECT_FALSE(status->status.empty());
  EXPECT_EQ(status->username, "user@example.com");
  EXPECT_EQ(status->domain, "example.com");
  EXPECT_EQ(status->policy_description_key, "statusUser");
  EXPECT_TRUE(status->enrollment_token.empty());
}

TEST_F(UserCloudPolicyStatusProviderTest,
       GetStatusMojoManagedWithEnrollmentToken) {
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  auto* store_ptr = store.get();
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  policy_manager.core()->Connect(std::make_unique<MockCloudPolicyClient>());
  UserCloudPolicyStatusProvider provider(&policy_manager, profile_);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
  policy_data->set_username("user@example.com");
  store_ptr->set_policy_data_for_testing(std::move(policy_data));

  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_TRUE(entry);
  entry->SetProfileManagementEnrollmentToken("test_token_123");

  auto status = provider.GetStatusMojo();
  EXPECT_FALSE(status->error);
  EXPECT_FALSE(status->status.empty());
  EXPECT_EQ(status->enrollment_token, "test_token_123");
  EXPECT_EQ(status->domain, "example.com");
  EXPECT_FALSE(status->username.has_value());
  EXPECT_EQ(status->policy_description_key, "statusUser");
}

}  // namespace policy
