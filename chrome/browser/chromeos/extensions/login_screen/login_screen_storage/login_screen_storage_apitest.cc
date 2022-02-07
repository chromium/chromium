// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/logging.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace extensions {

namespace {

constexpr char kInSessionExtensionCrxPath[] =
    "extensions/api_test/login_screen_apis/in_session_extension.crx";
constexpr char kInSessionExtensionId[] = "ofcpkomnogjenhfajfjadjmjppbegnad";
constexpr char kListenerMessage[] = "Waiting for test name";

bool IsLoginScreenStorageCrosapiAvailable() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::LoginScreenStorage>()) {
    LOG(WARNING) << "Unsupported ash version.";
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
}

}  // namespace

class LoginScreenStorageExtensionApiTest
    : public extensions::MixinBasedExtensionApiTest {
 public:
  LoginScreenStorageExtensionApiTest() {}

  LoginScreenStorageExtensionApiTest(
      const LoginScreenStorageExtensionApiTest&) = delete;
  LoginScreenStorageExtensionApiTest& operator=(
      const LoginScreenStorageExtensionApiTest&) = delete;

  ~LoginScreenStorageExtensionApiTest() override {
    catcher_.reset();
    listener_.reset();
  }

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    extensions::MixinBasedExtensionApiTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

    mock_policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    mock_policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &mock_policy_provider_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    policy::ProfilePolicyConnector* const connector =
        profile()->GetProfilePolicyConnector();
    connector->OverrideIsManagedForTesting(true);

    extension_force_install_mixin_.InitWithMockPolicyProvider(
        profile(), &mock_policy_provider_);

    extensions::MixinBasedExtensionApiTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    extensions::MixinBasedExtensionApiTest::TearDownOnMainThread();

    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  void RunTest(const std::string& test_name) {
    catcher_ = std::make_unique<extensions::ResultCatcher>();
    listener_ =
        std::make_unique<ExtensionTestMessageListener>(kListenerMessage,
                                                       /* will_reply= */ true);

    base::FilePath extension_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .Append(FILE_PATH_LITERAL(kInSessionExtensionCrxPath));

    extensions::ExtensionId extension_id;
    ASSERT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
        extension_dir, ExtensionForceInstallMixin::WaitMode::kLoad,
        &extension_id));
    ASSERT_EQ(kInSessionExtensionId, extension_id);

    ASSERT_TRUE(listener_->WaitUntilSatisfied());
    listener_->Reply(test_name);
    ASSERT_TRUE(catcher_->GetNextResult());
  }

 protected:
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};

  std::unique_ptr<extensions::ResultCatcher> catcher_;
  std::unique_ptr<ExtensionTestMessageListener> listener_;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
};

IN_PROC_BROWSER_TEST_F(LoginScreenStorageExtensionApiTest,
                       StorePersistentData) {
  if (!IsLoginScreenStorageCrosapiAvailable())
    return;

  RunTest("InSessionLoginScreenStorageStorePersistentData");
}

IN_PROC_BROWSER_TEST_F(LoginScreenStorageExtensionApiTest,
                       RetrievePersistentData) {
  if (!IsLoginScreenStorageCrosapiAvailable())
    return;

  RunTest("InSessionLoginScreenStorageRetrievePersistentData");
}

IN_PROC_BROWSER_TEST_F(LoginScreenStorageExtensionApiTest, StoreCredentials) {
  if (!IsLoginScreenStorageCrosapiAvailable())
    return;

  RunTest("InSessionLoginScreenStorageStoreCredentials");
}

IN_PROC_BROWSER_TEST_F(LoginScreenStorageExtensionApiTest,
                       RetrieveCredentials) {
  if (!IsLoginScreenStorageCrosapiAvailable())
    return;

  RunTest("InSessionLoginScreenStorageRetrieveCredentials");
}

}  // namespace extensions
