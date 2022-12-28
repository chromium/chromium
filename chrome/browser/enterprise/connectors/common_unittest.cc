// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com/";

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void EnableFeatures() { scoped_feature_list_.Reset(); }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

}  // namespace

class EnterpriseConnectorsResultShouldAllowDataUseTest
    : public BaseTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseConnectorsResultShouldAllowDataUseTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();

    // Settings can't be returned if no DM token exists.
    SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  }

  bool allowed() const { return !GetParam(); }
  const char* bool_setting() const { return GetParam() ? "true" : "false"; }

  AnalysisSettings settings() {
    absl::optional<AnalysisSettings> settings =
        ConnectorsServiceFactory::GetForBrowserContext(profile())
            ->GetAnalysisSettings(GURL(kTestUrl), FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseConnectorsResultShouldAllowDataUseTest,
                         testing::Bool());

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockLargeFile) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_large_files": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(
                settings(),
                safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE));
}

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest,
       BlockPasswordProtected) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_password_protected": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(
                settings(),
                safe_browsing::BinaryUploadService::Result::FILE_ENCRYPTED));
}

TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest,
       BlockUnsupportedFileTypes) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_unsupported_file_types": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile()->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(
                settings(), safe_browsing::BinaryUploadService::Result::
                                DLP_SCAN_UNSUPPORTED_FILE_TYPE));
}

}  // namespace enterprise_connectors
