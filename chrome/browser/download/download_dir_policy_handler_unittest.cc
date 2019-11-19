// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_policy_handler.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace {

const char* kUserIDHash = "deadbeef";

#if defined(OS_CHROMEOS)
const char* kRelativeToDriveRoot = "/home/";
#endif

}  // namespace

class DownloadDirPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    recommended_store_ = new policy::ConfigurationPolicyPrefStore(
        nullptr, policy_service_.get(), &handler_list_,
        policy::POLICY_LEVEL_RECOMMENDED);
    handler_list_.AddHandler(
        base::WrapUnique<policy::ConfigurationPolicyHandler>(
            new DownloadDirPolicyHandler));
  }

  void PopulatePolicyHandlerParameters(
      policy::PolicyHandlerParameters* parameters) override {
    parameters->user_id_hash = kUserIDHash;
  }

 protected:
  scoped_refptr<policy::ConfigurationPolicyPrefStore> recommended_store_;
};

#if !defined(OS_CHROMEOS)
TEST_F(DownloadDirPolicyHandlerTest, SetDownloadDirectory) {
  policy::PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(std::string()), nullptr);
  UpdateProviderPolicy(policy);

  // Setting a DownloadDirectory should disable the PromptForDownload pref.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload, &value));
  ASSERT_TRUE(value);
  bool prompt_for_download = true;
  bool result = value->GetAsBoolean(&prompt_for_download);
  ASSERT_TRUE(result);
  EXPECT_FALSE(prompt_for_download);
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(DownloadDirPolicyHandlerTest, SetDownloadToDrive) {
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));

  policy::PolicyMap policy;
  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(
                 download_dir_util::kDriveNamePolicyVariableName),
             nullptr);
  UpdateProviderPolicy(policy);

  const base::Value* value = NULL;
  bool prompt_for_download;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload, &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&prompt_for_download));
  EXPECT_FALSE(prompt_for_download);

  bool disable_drive;
  EXPECT_TRUE(store_->GetValue(drive::prefs::kDisableDrive, &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&disable_drive));
  EXPECT_FALSE(disable_drive);

  std::string download_directory;
  EXPECT_TRUE(store_->GetValue(prefs::kDownloadDefaultDirectory, &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->GetAsString(&download_directory));
  EXPECT_EQ(download_dir_util::kDriveNamePolicyVariableName,
            download_directory);

  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kUserIDHash), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(recommended_store_->GetValue(drive::prefs::kDisableDrive, NULL));

  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(
                 std::string(download_dir_util::kDriveNamePolicyVariableName) +
                 kRelativeToDriveRoot),
             nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_FALSE(recommended_store_->GetValue(prefs::kPromptForDownload, NULL));
  EXPECT_FALSE(recommended_store_->GetValue(drive::prefs::kDisableDrive, NULL));

  EXPECT_TRUE(
      recommended_store_->GetValue(prefs::kDownloadDefaultDirectory, &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->GetAsString(&download_directory));
  EXPECT_EQ(std::string(download_dir_util::kDriveNamePolicyVariableName) +
                kRelativeToDriveRoot,
            download_directory);

  policy.Set(policy::key::kDownloadDirectory, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kUserIDHash), nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_FALSE(recommended_store_->GetValue(prefs::kPromptForDownload, NULL));
  EXPECT_FALSE(recommended_store_->GetValue(drive::prefs::kDisableDrive, NULL));

  EXPECT_TRUE(
      recommended_store_->GetValue(prefs::kDownloadDefaultDirectory, &value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(value->GetAsString(&download_directory));
  EXPECT_EQ(kUserIDHash, download_directory);
}
#endif
