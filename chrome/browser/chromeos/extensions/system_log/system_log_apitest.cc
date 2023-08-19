// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "components/device_event_log/device_event_log.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

constexpr char kApiExtensionRelativePath[] = "extensions/api_test/system_log";
constexpr char kExtensionPemRelativePath[] =
    "extensions/api_test/system_log.pem";
// ID associated with the .pem.
constexpr char kExtensionId[] = "ghbglelacokpaehlgjbgdfmmggnihdcf";
constexpr char kExtensionIdHash[] = "A3F1DA9186E056F8424437E7B577FCA04BC61B51";

constexpr char kDeviceEventLogEntry[] = "device_event_log";

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

// Imprivata logs go to system logs and DEBUG device event logs.
IN_PROC_BROWSER_TEST_P(SystemLogApitest, AddLogWithImprivata) {
  const std::string test_name = GetParam();
  SetCustomArg(test_name);

  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_debug_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_DEBUG, /*max_events=*/1);
  std::string expected_logs =
      base::StringPrintf("DEBUG: [%s]: Test log message\n", kExtensionId);

  ASSERT_EQ(expected_logs, produced_debug_logs);
}

// Other extension logs go to device event logs with an EVENT log level and are
// added to the feedback report fetched data.
IN_PROC_BROWSER_TEST_P(SystemLogApitest, AddLogWithOtherExtension) {
  // Remove extension from Imprivata behavior feature. kAllowlistedExtensionID
  // is adding it to both the systemLog permission allowlist, and the Imprivata
  // behaviors.
  FeatureProvider provider;
  auto in_session_feature = std::make_unique<SimpleFeature>();
  in_session_feature->set_name("imprivata_in_session_extension");
  in_session_feature->set_blocklist({kExtensionIdHash});
  provider.AddFeature("imprivata_in_session_extension",
                      std::move(in_session_feature));

  auto login_screen_feature = std::make_unique<SimpleFeature>();
  login_screen_feature->set_name("imprivata_login_screen_extension");
  login_screen_feature->set_blocklist({kExtensionIdHash});
  provider.AddFeature("imprivata_login_screen_extension",
                      std::move(login_screen_feature));
  ExtensionAPI::GetSharedInstance()->RegisterDependencyProvider("behavior",
                                                                &provider);

  const std::string test_name = GetParam();
  SetCustomArg(test_name);

  ResultCatcher catcher;
  ForceInstallExtension();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string produced_event_logs = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"extensions",
      /*max_level=*/device_event_log::LOG_LEVEL_EVENT, /*max_events=*/1);
  std::string expected_logs =
      base::StringPrintf("EVENT: [%s]: Test log message\n", kExtensionId);
  ASSERT_EQ(expected_logs, produced_event_logs);

  // Verify that logs are added to feedback report strings.
  auto log_source = std::make_unique<system_logs::DeviceEventLogSource>();
  base::test::TestFuture<std::unique_ptr<system_logs::SystemLogsResponse>>
      future;
  log_source->Fetch(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  system_logs::SystemLogsResponse* response = future.Get().get();
  const auto device_event_log_iter = response->find(kDeviceEventLogEntry);
  EXPECT_NE(device_event_log_iter, response->end());

  std::string expected_feedback_log =
      base::StringPrintf("[%s]: Test log message\n", kExtensionId);
  EXPECT_THAT(device_event_log_iter->second,
              ::testing::HasSubstr(expected_feedback_log));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemLogApitest,
    /*test_name=*/testing::Values("AddLogWithCallback", "AddLogWithPromise"));

}  // namespace extensions
