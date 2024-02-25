// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kExternalDataPath[] = "policy/blank.html";

}  // namespace

class UserCloudExternalDataManagerTest : public LoginPolicyTestBase {
 protected:
  void SetUp() override {
    fake_gaia_.set_initialize_configuration(false);

    LoginPolicyTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    LoginPolicyTestBase::SetUpOnMainThread();
    const GURL url =
        embedded_test_server()->GetURL(std::string("/") + kExternalDataPath);

    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    ASSERT_TRUE(base::ReadFileToString(test_dir.AppendASCII(kExternalDataPath),
                                       &external_data_));
    ASSERT_FALSE(external_data_.empty());

    metadata_ =
        test::ConstructExternalDataReference(url.spec(), external_data_);
  }

  std::string external_data_;
  base::Value::Dict metadata_;
};

IN_PROC_BROWSER_TEST_F(UserCloudExternalDataManagerTest, FetchExternalData) {
  CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(1000);

  SkipToLoginScreen();
  LogIn();

  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);

  std::string value;
  ASSERT_TRUE(base::JSONWriter::Write(metadata_, &value));
  enterprise_management::CloudPolicySettings policy;
  policy.mutable_wallpaperimage()->set_value(value);
  user_policy_helper()->SetPolicyAndWait(policy, profile);

  UserCloudPolicyManagerAsh* policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  ASSERT_TRUE(policy_manager);
  ProfilePolicyConnector* policy_connector =
      profile->GetProfilePolicyConnector();
  ASSERT_TRUE(policy_connector);

  {
    base::test::TestFuture<void> refresh_policy_future;
    policy_connector->policy_service()->RefreshPolicies(
        refresh_policy_future.GetCallback(), PolicyFetchReason::kTest);
    ASSERT_TRUE(refresh_policy_future.Wait())
        << "RefreshPolicies did not invoke the finished callback.";
  }

  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const PolicyMap::Entry* policy_entry = policies.Get(key::kWallpaperImage);
  ASSERT_TRUE(policy_entry);
  EXPECT_EQ(metadata_, *policy_entry->value(base::Value::Type::DICT));
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  base::test::TestFuture<std::unique_ptr<std::string>, const base::FilePath&>
      fetch_data_future;
  policy_entry->external_data_fetcher->Fetch(fetch_data_future.GetCallback());
  ASSERT_TRUE(fetch_data_future.Get<std::unique_ptr<std::string>>());
  EXPECT_EQ(external_data_,
            *fetch_data_future.Get<std::unique_ptr<std::string>>());
}

}  // namespace policy
