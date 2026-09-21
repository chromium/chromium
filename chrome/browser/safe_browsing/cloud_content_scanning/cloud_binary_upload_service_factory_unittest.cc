// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class CloudBinaryUploadServiceFactoryTest : public testing::Test {
 protected:
  CloudBinaryUploadServiceFactoryTest() = default;
  ~CloudBinaryUploadServiceFactoryTest() override = default;
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(CloudBinaryUploadServiceFactoryTest,
       RedirectedToOriginalInIsolatedMode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  TestingProfile* original_profile =
      profile_manager_->CreateTestingProfile("original_profile");
  original_profile->GetPrefs()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(
          enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
  TestingProfile* profile =
      TestingProfile::Builder().BuildIncognito(original_profile);
  ASSERT_TRUE(profile->IsEnterpriseIsolatedModeProfile());

  auto* service = CloudBinaryUploadServiceFactory::GetForProfile(profile);
  EXPECT_NE(service, nullptr);
  EXPECT_EQ(service,
            CloudBinaryUploadServiceFactory::GetForProfile(original_profile));
}

}  // namespace safe_browsing
