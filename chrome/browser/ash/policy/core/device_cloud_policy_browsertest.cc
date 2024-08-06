// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
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

// Tests for the rotation of the signing keys used for the device policy.
//
// The test is performed against a test policy server, which is set up for
// rotating the policy key automatically with each policy fetch.
struct FeaturesTestParam {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};

class KeyRotationDeviceCloudPolicyTest
    : public DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<FeaturesTestParam> {
 public:
  KeyRotationDeviceCloudPolicyTest(const KeyRotationDeviceCloudPolicyTest&) =
      delete;
  KeyRotationDeviceCloudPolicyTest& operator=(
      const KeyRotationDeviceCloudPolicyTest&) = delete;

 protected:
  const int kInitialPolicyValue = 123;
  const int kSecondPolicyValue = 456;
  const int kThirdPolicyValue = 789;
  const char* const kPolicyKey = key::kDevicePolicyRefreshRate;

  KeyRotationDeviceCloudPolicyTest() {
    UpdateBuiltTestPolicyValue(kInitialPolicyValue);
    const FeaturesTestParam& features_test_param = GetParam();
    scoped_feature_list_.InitWithFeatures(
        features_test_param.enabled_features,
        features_test_param.disabled_features);
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The verification key was replaced from the original to the
    // testing key by the super class. However this class uses the
    // policy data provided by signature_provider.cc which still
    // gives data validated by the original verification key. Thus
    // the flag needs to be removed so that these tests use the
    // original verification key.
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kPolicyVerificationKey);
  }

  void UpdateBuiltTestPolicyValue(int test_policy_value) {
    device_policy()
        ->payload()
        .mutable_device_policy_refresh_rate()
        ->set_device_policy_refresh_rate(test_policy_value);
    device_policy()->Build();
  }

  void UpdateServedTestPolicy() {
    policy_test_server_mixin_.UpdateDevicePolicy(device_policy()->payload());
  }

  void StartDevicePolicyRefresh() {
    g_browser_process->platform_part()
        ->browser_policy_connector_ash()
        ->GetDeviceCloudPolicyManager()
        ->RefreshPolicies(PolicyFetchReason::kTest);
  }

  std::string GetOwnerPublicKey() const {
    return ash::DeviceSettingsService::Get()->GetPublicKey()->as_string();
  }

  int GetInstalledPolicyKeyVersion() const {
    return g_browser_process->platform_part()
        ->browser_policy_connector_ash()
        ->GetDeviceCloudPolicyManager()
        ->device_store()
        ->policy()
        ->public_key_version();
  }

  int GetInstalledPolicyValue() {
    PolicyService* const policy_service = g_browser_process->platform_part()
                                              ->browser_policy_connector_ash()
                                              ->GetPolicyService();
    const base::Value* policy_value =
        policy_service
            ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                          std::string() /* component_id */))
            .GetValue(kPolicyKey, base::Value::Type::INTEGER);
    EXPECT_TRUE(policy_value);
    EXPECT_EQ(policy_value->type(), base::Value::Type::INTEGER);
    return policy_value->GetIfInt().value_or(-1);
  }

  void WaitForInstalledPolicyValue(int expected_policy_value) {
    if (GetInstalledPolicyValue() == expected_policy_value) {
      return;
    }
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
            ->browser_policy_connector_ash()
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

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{
      &mixin_host_,
      {ash::EmbeddedPolicyTestServerMixin::ENABLE_CANNED_SIGNING_KEYS,
       ash::EmbeddedPolicyTestServerMixin::
           ENABLE_AUTOMATIC_ROTATION_OF_SIGNINGKEYS}};
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  int awaited_policy_value_ = -1;
  std::unique_ptr<base::RunLoop> policy_change_waiting_run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(KeyRotationDeviceCloudPolicyTest, Basic) {
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

INSTANTIATE_TEST_SUITE_P(
    KeyRotationDeviceCloudPolicyTest,
    KeyRotationDeviceCloudPolicyTest,
    ::testing::Values(
        FeaturesTestParam{.enabled_features = {policy::kPolicyFetchWithSha256}},
        FeaturesTestParam{
            .disabled_features = {policy::kPolicyFetchWithSha256}}));

namespace {

// Tests how component policy is handled for extensions installed on the sign-in
// screen.
class SigninExtensionsDeviceCloudPolicyBrowserTest
    : public DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<FeaturesTestParam> {
 public:
  static constexpr const char* kTestExtensionId =
      "hifnmfgfdfhmoaponfpmnlpeahiomjim";
  static constexpr const char* kTestExtensionPath =
      "extensions/signin_screen_managed_storage/extension.crx";
  static constexpr const char* kTestExtensionUpdateManifestPath =
      "/extensions/signin_screen_managed_storage/update_manifest.xml";
  static constexpr const char* kFakePolicyPath = "/test-policy.json";
  static constexpr const char* kFakePolicy =
      "{\"string-policy\": {\"Value\": \"value\"}}";
  static constexpr int kFakePolicyPublicKeyVersion = 1;

  SigninExtensionsDeviceCloudPolicyBrowserTest() {
    const FeaturesTestParam& features_test_param = GetParam();
    scoped_feature_list_.InitWithFeatures(
        features_test_param.enabled_features,
        features_test_param.disabled_features);
  }

  SigninExtensionsDeviceCloudPolicyBrowserTest(
      const SigninExtensionsDeviceCloudPolicyBrowserTest&) = delete;
  SigninExtensionsDeviceCloudPolicyBrowserTest& operator=(
      const SigninExtensionsDeviceCloudPolicyBrowserTest&) = delete;

  ~SigninExtensionsDeviceCloudPolicyBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    // The test app has to be allowlisted for sign-in screen.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestExtensionId);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    SetFakeDevicePolicy();

    policy_test_server_mixin_.UpdateDevicePolicy(device_policy()->payload());
    policy_test_server_mixin_.UpdatePolicy(
        dm_protocol::kChromeSigninExtensionPolicyType, kTestExtensionId,
        BuildTestComponentPolicyPayload().SerializeAsString());
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    connector->device_management_service()->ScheduleInitialization(0);
  }

  // |hang_component_policy_fetch| - whether requests for the component policy
  // download should be hung indefinitely.
  void StartTestServer(bool hang_component_policy_fetch) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SigninExtensionsDeviceCloudPolicyBrowserTest::InterceptComponentPolicy,
        base::Unretained(this), hang_component_policy_fetch));
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SigninExtensionsDeviceCloudPolicyBrowserTest::InterceptUpdateManifest,
        base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  // Intercepts the request for the test extension update manifest.
  std::unique_ptr<net::test_server::HttpResponse> InterceptUpdateManifest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != kTestExtensionUpdateManifestPath) {
      return nullptr;
    }

    // Create update manifest for the test extension, setting the extension URL
    // with a test server URL pointing to the extension under the test data
    // path.
    std::string manifest_response = extensions::CreateUpdateManifest(
        {extensions::UpdateManifestItem(kTestExtensionId)
             .version("1.0")
             .codebase(base::ReplaceStringPlaceholders(
                 "http://$1/$2",
                 {embedded_test_server()->host_port_pair().ToString(),
                  kTestExtensionPath},
                 nullptr))});

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/xml");
    response->set_content(manifest_response);
    return response;
  }

  // Intercepts the component policy requests.
  // |hang| - if set, this will return a hung response, thus preventing the
  //     policy download. Otherwise, the response will contain the test policy.
  std::unique_ptr<net::test_server::HttpResponse> InterceptComponentPolicy(
      bool hang,
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kFakePolicyPath) {
      return nullptr;
    }

    if (hang) {
      return std::make_unique<net::test_server::HungResponse>();
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(kFakePolicy);
    return response;
  }

  void SetFakeDevicePolicy() {
    device_policy()->policy_data().set_public_key_version(
        kFakePolicyPublicKeyVersion);

    const GURL update_manifest_url =
        embedded_test_server()->GetURL(kTestExtensionUpdateManifestPath);
    const std::string policy_item_value = base::ReplaceStringPlaceholders(
        "$1;$2", {kTestExtensionId, update_manifest_url.spec()}, nullptr);

    device_policy()
        ->payload()
        .mutable_device_login_screen_extensions()
        ->add_device_login_screen_extensions(policy_item_value);

    device_policy()->Build();
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
  }

  enterprise_management::ExternalPolicyData BuildTestComponentPolicyPayload() {
    ComponentCloudPolicyBuilder builder;
    MakeTestComponentPolicyBuilder(&builder);
    return builder.payload();
  }

  void MakeTestComponentPolicyBuilder(ComponentCloudPolicyBuilder* builder) {
    builder->policy_data().set_policy_type(
        dm_protocol::kChromeSigninExtensionPolicyType);
    builder->policy_data().set_settings_entity_id(kTestExtensionId);
    builder->policy_data().set_public_key_version(kFakePolicyPublicKeyVersion);
    builder->payload().set_download_url(
        embedded_test_server()->GetURL(kFakePolicyPath).spec());
    builder->payload().set_secure_hash(crypto::SHA256HashString(kFakePolicy));
    builder->Build();
  }

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// The ManagedStorage test is done in two steps:
//  1. Test that fetches the component policy and verifies that the fetched
//     policy is exposed to a test extension installed into the sign-in profile.
//  2. Test that blocks the component policy fetch, and verifies that a test
//     extension installed into the sign-in profile can access the component
//     policy downloaded during the first step.
// PRE_ManagedStorage test handles the first step.
IN_PROC_BROWSER_TEST_P(SigninExtensionsDeviceCloudPolicyBrowserTest,
                       PRE_ManagedStorage) {
  // The test app will be installed via policy, at which point its
  // background page will be loaded.
  extensions::ResultCatcher result_catcher;
  StartTestServer(false /*hang_component_policy_fetch*/);
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// The second step of the ManagedStorage test, which blocks component policy
// download and verifies that a cached component policy is available to the test
// extenion.
// See PRE_ManagedStorage test.
IN_PROC_BROWSER_TEST_P(SigninExtensionsDeviceCloudPolicyBrowserTest,
                       ManagedStorage) {
  // The test app will be installed via policy, at which point its
  // background page will be loaded. Note that the app will not be installed
  // before the test server is started, even if the app is installed from the
  // extension cache - the server will be pinged at least to check whether the
  // cached app version is the latest.
  extensions::ResultCatcher result_catcher;
  StartTestServer(true /*hang_component_policy_fetch*/);
  EXPECT_TRUE(result_catcher.GetNextResult());
}

INSTANTIATE_TEST_SUITE_P(
    SigninExtensionsDeviceCloudPolicyBrowserTest,
    SigninExtensionsDeviceCloudPolicyBrowserTest,
    ::testing::Values(
        FeaturesTestParam{.enabled_features = {policy::kPolicyFetchWithSha256}},
        FeaturesTestParam{
            .disabled_features = {policy::kPolicyFetchWithSha256}}));

}  // namespace policy
