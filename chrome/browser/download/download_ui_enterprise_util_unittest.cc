// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_enterprise_util.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

constexpr char kDownloadConnectorEnabledNonBlockingPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["malware"]}
    ]
  }
])";

constexpr char kDownloadConnectorEnabledBlockingPref[] = R"([
  {
    "service_provider": "google",
    "block_until_verdict":1,
    "enable": [
      {"url_list": ["*"], "tags": ["malware"]}
    ]
  }
])";

class DownloadUiEnterpriseUtilTest : public ::testing::Test {
 public:
  DownloadUiEnterpriseUtilTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadUiEnterpriseUtilTest(const DownloadUiEnterpriseUtilTest&) = delete;
  DownloadUiEnterpriseUtilTest& operator=(const DownloadUiEnterpriseUtilTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
    profile_->GetPrefs()->SetInteger(
        AnalysisConnectorScopePref(
            enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED),
        policy::POLICY_SCOPE_MACHINE);
  }

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadUiEnterpriseUtilTest, DoesDownloadConnectorBlock) {
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
  profile_->GetPrefs()->Set(
      enterprise_connectors::AnalysisConnectorPref(
          enterprise_connectors::FILE_DOWNLOADED),
      *base::JSONReader::Read(kDownloadConnectorEnabledNonBlockingPref,
                              base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
  profile_->GetPrefs()->Set(
      enterprise_connectors::AnalysisConnectorPref(
          enterprise_connectors::FILE_DOWNLOADED),
      *base::JSONReader::Read(kDownloadConnectorEnabledBlockingPref,
                              base::JSON_PARSE_CHROMIUM_EXTENSIONS));
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  EXPECT_TRUE(DoesDownloadConnectorBlock(profile_, GURL()));
#else
  EXPECT_FALSE(DoesDownloadConnectorBlock(profile_, GURL()));
#endif
}

}  // namespace
}  // namespace download
