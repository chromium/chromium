// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

constexpr char kApiExtensionRelativePath[] = "extensions/api_test/system_log";
constexpr char kExtensionPemRelativePath[] =
    "extensions/api_test/system_log.pem";
// ID associated with the .pem.
constexpr char kExtensionId[] = "ghbglelacokpaehlgjbgdfmmggnihdcf";

}  // namespace

class SystemLogApitest : public MixinBasedExtensionApiTest,
                         public ::testing::WithParamInterface<std::string> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedExtensionApiTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    kExtensionId);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

    mock_policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    mock_policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &mock_policy_provider_);
  }

  void SetUpOnMainThread() override {
    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), &mock_policy_provider_);

    MixinBasedExtensionApiTest::SetUpOnMainThread();
  }

  void ForceInstallExtension() {
    base::FilePath test_dir_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_dir_path.AppendASCII(kApiExtensionRelativePath),
        test_dir_path.AppendASCII(kExtensionPemRelativePath),
        ExtensionForceInstallMixin::WaitMode::kLoad));
  }

 private:
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
};

IN_PROC_BROWSER_TEST_P(SystemLogApitest, AddLog) {
  const std::string test_name = GetParam();
  SetCustomArg(test_name);

  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_DEBUG, /*max_events=*/1);

  std::string expected_logs =
      "DEBUG: [" + std::string(kExtensionId) + "]: Test log message\n";

  ASSERT_EQ(expected_logs, produced_logs);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemLogApitest,
    /*test_name=*/testing::Values("AddLogWithCallback", "AddLogWithPromise"));

}  // namespace extensions
