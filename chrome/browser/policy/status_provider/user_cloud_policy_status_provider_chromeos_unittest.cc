// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class UserCloudPolicyStatusProviderChromeOSTest : public testing::Test {
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
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(UserCloudPolicyStatusProviderChromeOSTest, GetStatusMojoUnmanaged) {
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  UserCloudPolicyStatusProviderChromeOS provider(&policy_manager, profile_);

  auto status = provider.GetStatusMojo();
  EXPECT_TRUE(status);
}

TEST_F(UserCloudPolicyStatusProviderChromeOSTest, GetStatusMojoManaged) {
  auto store = std::make_unique<MockCloudPolicyStore>(
      dm_protocol::GetChromeUserPolicyType());
  auto* store_ptr = store.get();
  MockCloudPolicyManager policy_manager(
      std::move(store), task_environment_.GetMainThreadTaskRunner());
  policy_manager.core()->Connect(std::make_unique<MockCloudPolicyClient>());
  UserCloudPolicyStatusProviderChromeOS provider(&policy_manager, profile_);

  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
  policy_data->set_username("user@example.com");
  store_ptr->set_policy_data_for_testing(std::move(policy_data));

  auto status = provider.GetStatusMojo();
  ASSERT_TRUE(status);
  EXPECT_FALSE(status->error);
  EXPECT_FALSE(status->status.empty());
  EXPECT_EQ(status->username, "user@example.com");
  EXPECT_EQ(status->domain, "example.com");
  EXPECT_EQ(status->policy_description_key, "statusUser");
}

}  // namespace policy
