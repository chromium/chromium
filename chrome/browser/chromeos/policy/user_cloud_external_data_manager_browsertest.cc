// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
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
    fake_gaia_.set_initialize_fake_merge_session(false);

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
  std::unique_ptr<base::DictionaryValue> metadata_;
};

IN_PROC_BROWSER_TEST_F(UserCloudExternalDataManagerTest, FetchExternalData) {
  CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(1000);

  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);

  std::string value;
  ASSERT_TRUE(base::JSONWriter::Write(*metadata_, &value));
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>();
  policy->SetKey(key::kWallpaperImage, base::Value(value));
  user_policy_helper()->SetPolicyAndWait(*policy, base::DictionaryValue(),
                                         profile);

  UserCloudPolicyManagerChromeOS* policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  ASSERT_TRUE(policy_manager);
  ProfilePolicyConnector* policy_connector =
      profile->GetProfilePolicyConnector();
  ASSERT_TRUE(policy_connector);

  {
    base::RunLoop refresh_loop;
    policy_connector->policy_service()->RefreshPolicies(
        refresh_loop.QuitWhenIdleClosure());
    refresh_loop.Run();
  }

  const PolicyMap& policies = policy_connector->policy_service()->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const PolicyMap::Entry* policy_entry = policies.Get(key::kWallpaperImage);
  ASSERT_TRUE(policy_entry);
  EXPECT_EQ(*metadata_, *policy_entry->value);
  ASSERT_TRUE(policy_entry->external_data_fetcher);

  base::RunLoop run_loop;
  std::unique_ptr<std::string> fetched_external_data;
  base::FilePath file_path;
  policy_entry->external_data_fetcher->Fetch(
      base::BindOnce(&test::ExternalDataFetchCallback, &fetched_external_data,
                     &file_path, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(fetched_external_data);
  EXPECT_EQ(external_data_, *fetched_external_data);
}

}  // namespace policy
