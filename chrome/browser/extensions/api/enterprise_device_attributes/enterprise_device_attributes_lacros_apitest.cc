// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/result_catcher.h"

using testing::Invoke;

using StringResult = crosapi::mojom::DeviceAttributesStringResult;
using StringResultCallback =
    base::OnceCallback<void(crosapi::mojom::DeviceAttributesStringResultPtr)>;

namespace {

const char kErrorUserNotAffiliated[] = "Access denied.";

constexpr char kDeviceId[] = "device_id";
constexpr char kSerialNumber[] = "serial_number";
constexpr char kAssetId[] = "asset_id";
constexpr char kAnnotatedLocation[] = "annotated_location";
constexpr char kHostname[] = "hostname";

constexpr char kTestExtensionID[] = "nbiliclbejdndfpchgkbmfoppjplbdok";
constexpr char kExtensionPath[] =
    "extensions/api_test/enterprise_device_attributes/";
constexpr char kExtensionPemPath[] =
    "extensions/api_test/enterprise_device_attributes.pem";

base::Value BuildCustomArg(const std::string& expected_directory_device_id,
                           const std::string& expected_serial_number,
                           const std::string& expected_asset_id,
                           const std::string& expected_annotated_location,
                           const std::string& expected_hostname) {
  base::Value custom_arg(base::Value::Type::DICTIONARY);
  custom_arg.SetKey("expectedDirectoryDeviceId",
                    base::Value(expected_directory_device_id));
  custom_arg.SetKey("expectedSerialNumber",
                    base::Value(expected_serial_number));
  custom_arg.SetKey("expectedAssetId", base::Value(expected_asset_id));
  custom_arg.SetKey("expectedAnnotatedLocation",
                    base::Value(expected_annotated_location));
  custom_arg.SetKey("expectedHostname", base::Value(expected_hostname));
  return custom_arg;
}

class FakeDeviceAttributesAsh : public crosapi::mojom::DeviceAttributes {
 public:
  explicit FakeDeviceAttributesAsh(bool is_affiliated)
      : is_affiliated_(is_affiliated) {}
  ~FakeDeviceAttributesAsh() override = default;

  // crosapi::mojom::DeviceAttributes:
  void GetDirectoryDeviceId(GetDirectoryDeviceIdCallback callback) override {
    RunCallbackWithResultOrError(std::move(callback), kDeviceId);
  }

  void GetDeviceSerialNumber(GetDeviceSerialNumberCallback callback) override {
    RunCallbackWithResultOrError(std::move(callback), kSerialNumber);
  }

  void GetDeviceAssetId(GetDeviceAssetIdCallback callback) override {
    RunCallbackWithResultOrError(std::move(callback), kAssetId);
  }

  void GetDeviceAnnotatedLocation(
      GetDeviceAnnotatedLocationCallback callback) override {
    RunCallbackWithResultOrError(std::move(callback), kAnnotatedLocation);
  }

  void GetDeviceHostname(GetDeviceHostnameCallback callback) override {
    RunCallbackWithResultOrError(std::move(callback), kHostname);
  }

  void GetDeviceTypeForMetrics(
      GetDeviceTypeForMetricsCallback callback) override {}

  void RunCallbackWithResultOrError(StringResultCallback callback,
                                    const std::string& result) {
    std::move(callback).Run(is_affiliated_ ? StringResult::NewContents(result)
                                           : StringResult::NewErrorMessage(
                                                 kErrorUserNotAffiliated));
  }

 private:
  bool is_affiliated_;
};

}  // namespace

namespace extensions {

class EnterpriseDeviceAttributesTest
    : public MixinBasedExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
 public:
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

    extensions::MixinBasedExtensionApiTest::SetUpOnMainThread();

    // Replace the production service with a mock for testing.
    mojo::Remote<crosapi::mojom::DeviceAttributes>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::DeviceAttributes>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  const Extension* ForceInstallExtension(const std::string& extension_path,
                                         const std::string& pem_path) {
    ExtensionId extension_id;
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromSourceDir(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .Append(FILE_PATH_LITERAL(extension_path)),
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .Append(FILE_PATH_LITERAL(pem_path)),
        ExtensionForceInstallMixin::WaitMode::kLoad, &extension_id));

    return extension_force_install_mixin_.GetEnabledExtension(extension_id);
  }

  void TestExtension(Browser* browser,
                     const GURL& page_url,
                     const base::Value& custom_arg_value) {
    DCHECK(page_url.is_valid()) << "page_url must be valid";

    std::string custom_arg;
    base::JSONWriter::Write(custom_arg_value, &custom_arg);
    SetCustomArg(custom_arg);

    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, page_url));
    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

 protected:
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      mock_policy_provider_;
  bool is_affiliated_ = GetParam();

  FakeDeviceAttributesAsh device_attributes_ash_{is_affiliated_};
  mojo::Receiver<crosapi::mojom::DeviceAttributes> receiver_{
      &device_attributes_ash_};
};

IN_PROC_BROWSER_TEST_P(EnterpriseDeviceAttributesTest, Success) {
  const bool is_affiliated = GetParam();

  const Extension* extension =
      ForceInstallExtension(kExtensionPath, kExtensionPemPath);
  const GURL test_url = extension->GetResourceURL("basic.html");

  // Device attributes are available only for affiliated user.
  std::string expected_directory_device_id = is_affiliated ? kDeviceId : "";
  std::string expected_serial_number = is_affiliated ? kSerialNumber : "";
  std::string expected_asset_id = is_affiliated ? kAssetId : "";
  std::string expected_annotated_location =
      is_affiliated ? kAnnotatedLocation : "";
  std::string expected_hostname = is_affiliated ? kHostname : "";
  TestExtension(CreateBrowser(profile()), test_url,
                BuildCustomArg(expected_directory_device_id,
                               expected_serial_number, expected_asset_id,
                               expected_annotated_location, expected_hostname));
}

// Both cases of affiliated and non-affiliated users are tested.
INSTANTIATE_TEST_SUITE_P(AffiliationCheck,
                         EnterpriseDeviceAttributesTest,
                         /* is_affiliated= */ ::testing::Bool());

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.deviceAttributes permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.deviceAttributes namespace.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    EnterpriseDeviceAttributesIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionTest("enterprise_device_attributes",
                               {.extension_url = "api_not_available.html"},
                               {.ignore_manifest_warnings = true}));

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile())
          ->enabled_extensions()
          .GetByID(kTestExtensionID);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.deviceAttributes' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}

}  //  namespace extensions
