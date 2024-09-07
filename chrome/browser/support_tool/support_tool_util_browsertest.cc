// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Profile;

namespace {

// Support packet details for testing.
constexpr char kCaseId[] = "case-id";
constexpr char kEmail[] = "test@test.com";
constexpr char kIssueDescription[] = "fake issue description";
constexpr char kUploadId[] = "testing_id";

class SupportToolUtilTest : public InProcessBrowserTest {
 public:
  SupportToolUtilTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Set-up policy provider for PolicyDataCollector to work.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)

class SupportToolUtilLoginScreenTest : public ash::LoginManagerTest {
 public:
  SupportToolUtilLoginScreenTest() = default;
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// Verifies that `GetFilepathToExport()` function works.
IN_PROC_BROWSER_TEST_F(SupportToolUtilTest, GetFilepathToExport) {
  // Time for testing.
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("30 April 2011 22:42:09", &time));

  // The returned filename should be in format of
  // <filename_prefix>_<case_id>_UTCYYYYMMDD_HHmm.
  const base::FilePath kExpectedFilepath =
      base::FilePath(FILE_PATH_LITERAL("target_directory"))
          .Append(
              FILE_PATH_LITERAL("test-file-prefix_case-id_UTC20110430_2242"));
  EXPECT_EQ(
      kExpectedFilepath,
      GetFilepathToExport(base::FilePath(FILE_PATH_LITERAL("target_directory")),
                          "test-file-prefix", "case-id", time));

  // Verify that case ID field is not included when an empty string is given.
  const base::FilePath kExpectedFilepathWithoutCaseId =
      base::FilePath(FILE_PATH_LITERAL("target_directory"))
          .Append(FILE_PATH_LITERAL("test-file-prefix_UTC20110430_2242"));
  EXPECT_EQ(
      kExpectedFilepathWithoutCaseId,
      GetFilepathToExport(base::FilePath(FILE_PATH_LITERAL("target_directory")),
                          "test-file-prefix", std::string(), time));
}

// Verifies that all data collectors available are added to
// `SupportToolHandler`.
IN_PROC_BROWSER_TEST_F(SupportToolUtilTest, GetSupportToolHandler) {
  std::vector<support_tool::DataCollectorType> data_collectors =
      GetAllAvailableDataCollectorsOnDevice();

  std::unique_ptr<SupportToolHandler> handler = GetSupportToolHandler(
      kCaseId, kEmail, kIssueDescription, kUploadId, browser()->profile(),
      std::set<support_tool::DataCollectorType>(data_collectors.begin(),
                                                data_collectors.end()));
  EXPECT_EQ(data_collectors.size(),
            handler->GetDataCollectorsForTesting().size());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that all data collectors available on login screen are added to
// `SupportToolHandler`.
IN_PROC_BROWSER_TEST_F(SupportToolUtilLoginScreenTest, GetSupportToolHandler) {
  Profile* signin_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetSigninBrowserContext());
  ASSERT_TRUE(signin_profile);

  std::vector<support_tool::DataCollectorType> all_data_collectors =
      GetAllAvailableDataCollectorsOnDevice();
  // These data collectors shouldn't be included on login screen because they
  // depend on data from user session.
  std::set<support_tool::DataCollectorType> excluded_data_collectors = {
      support_tool::DataCollectorType::CHROMEOS_CHROME_USER_LOGS,
      support_tool::DataCollectorType::SIGN_IN_STATE};

  std::unique_ptr<SupportToolHandler> handler = GetSupportToolHandler(
      kCaseId, kEmail, kIssueDescription, kUploadId, signin_profile,
      std::set<support_tool::DataCollectorType>(all_data_collectors.begin(),
                                                all_data_collectors.end()));
  EXPECT_EQ(all_data_collectors.size() - excluded_data_collectors.size(),
            handler->GetDataCollectorsForTesting().size());

  // Verify that the data collectors are excluded when they're not supported on
  // login screen.
  handler = GetSupportToolHandler(kCaseId, kEmail, kIssueDescription, kUploadId,
                                  signin_profile, excluded_data_collectors);
  EXPECT_EQ(0U, handler->GetDataCollectorsForTesting().size());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
