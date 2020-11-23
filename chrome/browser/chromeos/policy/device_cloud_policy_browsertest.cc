// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

class DeviceCloudPolicyBrowserTest : public InProcessBrowserTest {
 protected:
  DeviceCloudPolicyBrowserTest()
      : mock_client_(std::make_unique<MockCloudPolicyClient>()) {}

  std::unique_ptr<MockCloudPolicyClient> mock_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyBrowserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DeviceCloudPolicyBrowserTest, Initializer) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Initializer exists at first.
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is deleted when the manager connects.
  connector->GetDeviceCloudPolicyManager()->StartConnection(
      std::move(mock_client_), connector->GetInstallAttributes());
  EXPECT_FALSE(connector->GetDeviceCloudPolicyInitializer());

  // Initializer is restarted when the manager disconnects.
  connector->GetDeviceCloudPolicyManager()->Disconnect();
  EXPECT_TRUE(connector->GetDeviceCloudPolicyInitializer());
}

namespace {

// Tests for the rotation of the signing keys used for the device policy.
//
// The test is performed against a test policy server, which is set up for
// rotating the policy key automatically with each policy fetch.
class KeyRotationDeviceCloudPolicyTest : public DevicePolicyCrosBrowserTest {
 protected:
  const int kInitialPolicyValue = 123;
  const int kSecondPolicyValue = 456;
  const int kThirdPolicyValue = 789;
  const char* const kPolicyKey = key::kDevicePolicyRefreshRate;

  KeyRotationDeviceCloudPolicyTest() {
    UpdateBuiltTestPolicyValue(kInitialPolicyValue);
    local_policy_mixin_.EnableCannedSigningKeys();
    local_policy_mixin_.EnableAutomaticRotationOfSigningKeys();
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    SetFakeDevicePolicy();
    UpdateServedTestPolicy();
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    StartObservingTestPolicy();
  }

  void TearDownOnMainThread() override {
    policy_change_registrar_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  void UpdateBuiltTestPolicyValue(int test_policy_value) {
    device_policy()
        ->payload()
        .mutable_device_policy_refresh_rate()
        ->set_device_policy_refresh_rate(test_policy_value);
    device_policy()->Build();
  }

  void UpdateServedTestPolicy() {
    EXPECT_TRUE(
        local_policy_mixin_.UpdateDevicePolicy(device_policy()->payload()));
  }

  void StartDevicePolicyRefresh() {
    g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceCloudPolicyManager()
        ->RefreshPolicies();
  }

  std::string GetOwnerPublicKey() const {
    return chromeos::DeviceSettingsService::Get()->GetPublicKey()->as_string();
  }

  int GetInstalledPolicyKeyVersion() const {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceCloudPolicyManager()
        ->device_store()
        ->policy()
        ->public_key_version();
  }

  int GetInstalledPolicyValue() {
    PolicyService* const policy_service =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetPolicyService();
    const base::Value* policy_value =
        policy_service
            ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                          std::string() /* component_id */))
            .GetValue(kPolicyKey);
    EXPECT_TRUE(policy_value);
    int refresh_rate = -1;
    EXPECT_TRUE(policy_value->GetAsInteger(&refresh_rate));
    return refresh_rate;
  }

  void WaitForInstalledPolicyValue(int expected_policy_value) {
    if (GetInstalledPolicyValue() == expected_policy_value)
      return;
    awaited_policy_value_ = expected_policy_value;
    // The run loop will be terminated by OnPolicyChanged() once the policy
    // value becomes equal to the awaited value.
    DCHECK(!policy_change_waiting_run_loop_);
    policy_change_waiting_run_loop_ = std::make_unique<base::RunLoop>();
    policy_change_waiting_run_loop_->Run();
    policy_change_waiting_run_loop_.reset();
  }

 private:
  void SetFakeDevicePolicy() {
    device_policy()
        ->payload()
        .mutable_device_policy_refresh_rate()
        ->set_device_policy_refresh_rate(kInitialPolicyValue);
    device_policy()->Build();
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
  }

  void StartObservingTestPolicy() {
    policy_change_registrar_ = std::make_unique<PolicyChangeRegistrar>(
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetPolicyService(),
        PolicyNamespace(POLICY_DOMAIN_CHROME,
                        std::string() /* component_id */));
    policy_change_registrar_->Observe(
        kPolicyKey,
        base::BindRepeating(&KeyRotationDeviceCloudPolicyTest::OnPolicyChanged,
                            base::Unretained(this)));
  }

  void OnPolicyChanged(const base::Value* old_value,
                       const base::Value* new_value) {
    if (policy_change_waiting_run_loop_ &&
        GetInstalledPolicyValue() == awaited_policy_value_) {
      policy_change_waiting_run_loop_->Quit();
    }
  }

  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  int awaited_policy_value_ = -1;
  std::unique_ptr<base::RunLoop> policy_change_waiting_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeyRotationDeviceCloudPolicyTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(KeyRotationDeviceCloudPolicyTest, Basic) {
  // The policy has the initial value from the cache.
  EXPECT_EQ(kInitialPolicyValue, GetInstalledPolicyValue());

  // The server is updated to serve the second policy value, and the client
  // fetches it.
  UpdateBuiltTestPolicyValue(kSecondPolicyValue);
  UpdateServedTestPolicy();
  StartDevicePolicyRefresh();
  WaitForInstalledPolicyValue(kSecondPolicyValue);
  EXPECT_EQ(kSecondPolicyValue, GetInstalledPolicyValue());

  // Remember the key and the version after the fetch of the second value.
  const std::string owner_public_key = GetOwnerPublicKey();
  CHECK(!owner_public_key.empty());
  const int key_version = GetInstalledPolicyKeyVersion();

  // The server is updated to serve the third policy value, and the client
  // fetches it.
  UpdateBuiltTestPolicyValue(kThirdPolicyValue);
  UpdateServedTestPolicy();
  StartDevicePolicyRefresh();
  WaitForInstalledPolicyValue(kThirdPolicyValue);
  EXPECT_EQ(kThirdPolicyValue, GetInstalledPolicyValue());

  // The owner key got rotated on the client, as requested by the server, and
  // the key version got incremented.
  EXPECT_NE(owner_public_key, GetOwnerPublicKey());
  EXPECT_EQ(key_version + 1, GetInstalledPolicyKeyVersion());
}

namespace {

// Tests how component policy is handled for extensions installed on the sign-in
// screen.
class DeviceComponentPolicyBrowserTest : public DevicePolicyCrosBrowserTest {
 public:
  static constexpr const char* kTestExtensionId =
      "hifnmfgfdfhmoaponfpmnlpeahiomjim";
  static constexpr const char* kTestExtensionPath =
      "extensions/signin_screen_managed_storage/extension/";
  static constexpr const char* kTestExtensionPemPath =
      "extensions/signin_screen_managed_storage/extension.pem";
  static constexpr const char* kFakePolicyPath = "/test-policy.json";
  static constexpr const char* kFakePolicy =
      "{\"string-policy\": {\"Value\": \"value\"}}";
  static constexpr int kFakePolicyPublicKeyVersion = 1;

  DeviceComponentPolicyBrowserTest() = default;
  ~DeviceComponentPolicyBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    // The test app has to be allowlisted for sign-in screen.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestExtensionId);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    SetFakeDevicePolicy();

    EXPECT_TRUE(
        local_policy_mixin_.UpdateDevicePolicy(device_policy()->payload()));
    EXPECT_TRUE(local_policy_mixin_.server()->UpdatePolicy(
        dm_protocol::kChromeSigninExtensionPolicyType, kTestExtensionId,
        BuildTestComponentPolicyPayload().SerializeAsString()));
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    StartTestServer();

    extension_result_catcher_ = std::make_unique<extensions::ResultCatcher>();

    extension_force_install_mixin_.InitWithDevicePolicyCrosTestHelper(
        chromeos::ProfileHelper::GetSigninProfile(), policy_helper());
    ASSERT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .Append(kTestExtensionPath),
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .Append(kTestExtensionPemPath),
        ExtensionForceInstallMixin::WaitMode::kNone));

    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->device_management_service()->ScheduleInitialization(0);
  }

  void TearDownOnMainThread() override {
    extension_result_catcher_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  extensions::ResultCatcher* extension_result_catcher() {
    return extension_result_catcher_.get();
  }

 private:
  void StartTestServer() {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &DeviceComponentPolicyBrowserTest::InterceptComponentPolicy,
        base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  // Intercepts the component policy requests.
  std::unique_ptr<net::test_server::HttpResponse> InterceptComponentPolicy(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kFakePolicyPath)
      return nullptr;
    if (!content::IsPreTest()) {
      // Intentionally hang the request in the non-PRE_ test.
      return std::make_unique<net::test_server::HungResponse>();
    }
    // Respond with the fake policy in the PRE_ test.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(kFakePolicy);
    return response;
  }

  void SetFakeDevicePolicy() {
    device_policy()->policy_data().set_public_key_version(
        kFakePolicyPublicKeyVersion);
    device_policy()->Build();
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
  }

  enterprise_management::ExternalPolicyData BuildTestComponentPolicyPayload()
      const {
    ComponentCloudPolicyBuilder builder;
    MakeTestComponentPolicyBuilder(&builder);
    return builder.payload();
  }

  void MakeTestComponentPolicyBuilder(
      ComponentCloudPolicyBuilder* builder) const {
    builder->policy_data().set_policy_type(
        dm_protocol::kChromeSigninExtensionPolicyType);
    builder->policy_data().set_settings_entity_id(kTestExtensionId);
    builder->policy_data().set_public_key_version(kFakePolicyPublicKeyVersion);
    builder->payload().set_download_url(
        embedded_test_server()->GetURL(kFakePolicyPath).spec());
    builder->payload().set_secure_hash(crypto::SHA256HashString(kFakePolicy));
    builder->Build();
  }

  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
  std::unique_ptr<extensions::ResultCatcher> extension_result_catcher_;

  DISALLOW_COPY_AND_ASSIGN(DeviceComponentPolicyBrowserTest);
};

}  // namespace

// The ManagedStorage test is done in two steps:
//  1. Test that fetches the component policy and verifies that the fetched
//     policy is exposed to a test extension installed into the sign-in profile.
//  2. Test that blocks the component policy fetch, and verifies that a test
//     extension installed into the sign-in profile can access the component
//     policy downloaded during the first step.
// PRE_ManagedStorage test handles the first step.
IN_PROC_BROWSER_TEST_F(DeviceComponentPolicyBrowserTest, PRE_ManagedStorage) {
  EXPECT_TRUE(extension_result_catcher()->GetNextResult());
}

// The second step of the ManagedStorage test, which blocks component policy
// download and verifies that a cached component policy is available to the test
// extenion.
// See PRE_ManagedStorage test.
IN_PROC_BROWSER_TEST_F(DeviceComponentPolicyBrowserTest, ManagedStorage) {
  EXPECT_TRUE(extension_result_catcher()->GetNextResult());
}

}  // namespace policy
