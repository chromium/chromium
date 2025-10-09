// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr char kDeviceAnnotatedAssetId[] = "iwa_test_asset_id";
constexpr char kDeviceAnnotatedLocation[] = "iwa_test_location";
constexpr char kDeviceDirectoryApiId[] = "iwa_test_directory_id";
constexpr char kDeviceHostname[] = "iwa_test_hostname";
constexpr char kDeviceSerialNumber[] = "iwa_test_serial_number";

constexpr std::array<const char*, 5> kDeviceAttributeNames = {
    "AnnotatedAssetId", "AnnotatedLocation", "DirectoryId", "Hostname",
    "SerialNumber"};

constexpr std::array<const char*, 5> kExpectedDeviceAttributeValues = {
    kDeviceAnnotatedAssetId, kDeviceAnnotatedLocation, kDeviceDirectoryApiId,
    kDeviceHostname, kDeviceSerialNumber};

constexpr char kPermissionsPolicyErrorTemplate[] =
    "a JavaScript error: \"NotAllowedError: Failed to execute "
    "'get%s' on 'NavigatorManagedData': Permissions-Policy: "
    "device-attributes are disabled.";

constexpr char kNoDeviceAttributesPermissionExpectedError[] =
    "a JavaScript error: \"UnknownError: The current origin cannot use this "
    "web API because it was not granted the 'device-attributes' "
    "permission.\"\n";

const web_package::SignedWebBundleId kTestWebBundleId =
    web_app::test::GetDefaultEd25519WebBundleId();

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/"simple-iwa@localhost",
      /*web_bundle_id=*/kTestWebBundleId,
      /*update_manifest_url=*/update_manifest_url);

  KioskMixin::Config kiosk_iwa_config = {/*name=*/"IsolatedWebApp",
                                         /*auto_launch_account_id=*/{},
                                         {iwa_option}};
  return kiosk_iwa_config;
}

content::EvalJsResult CallDeviceAttributesApi(
    content::WebContents* web_contents,
    const std::string& attribute_name) {
  return content::EvalJs(
      web_contents, base::ReplaceStringPlaceholders("navigator.managed.get$1()",
                                                    {attribute_name}, nullptr));
}

struct KioskIwaDeviceAttributesApiTestParams {
  using TupleT = std::tuple<bool, bool, bool, bool>;
  bool feature_flag;
  bool allow_policy;
  bool block_policy;
  bool permissions_policy;
  explicit KioskIwaDeviceAttributesApiTestParams(TupleT t)
      : feature_flag(std::get<0>(t)),
        allow_policy(std::get<1>(t)),
        block_policy(std::get<2>(t)),
        permissions_policy(std::get<3>(t)) {}
};

}  // namespace

class KioskIwaDeviceAttributesApiTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          KioskIwaDeviceAttributesApiTestParams> {
 public:
  KioskIwaDeviceAttributesApiTest() {
    InitFeatureList();
    iwa_test_update_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(GetIwaManifestBuilder())
            .BuildBundle(web_app::test::GetDefaultEd25519KeyPair()));
  }

  ~KioskIwaDeviceAttributesApiTest() override = default;
  KioskIwaDeviceAttributesApiTest(const KioskIwaDeviceAttributesApiTest&) =
      delete;
  KioskIwaDeviceAttributesApiTest& operator=(
      const KioskIwaDeviceAttributesApiTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    SetDevicePolicies();
    LaunchIwaKiosk();
  }

 protected:
  content::WebContents* web_contents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

  bool IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() {
    return GetParam().feature_flag;
  }
  bool IsAllowPolicySet() { return GetParam().allow_policy; }
  bool IsBlockPolicySet() { return GetParam().block_policy; }
  bool IsPermissionsPolicyGranted() { return GetParam().permissions_policy; }

  void MaybeSetEnterprisePoliciesForIwaOrigin() {
    if (!IsAllowPolicySet() && !IsBlockPolicySet()) {
      return;
    }
    policy::PolicyMap policies;
    if (IsBlockPolicySet()) {
      policies.Set(
          policy::key::kDeviceAttributesBlockedForOrigins,
          policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
          policy::POLICY_SOURCE_CLOUD,
          base::Value(base::Value::List().Append(kAppOrigin.Serialize())),
          nullptr);
    }
    if (IsAllowPolicySet()) {
      policies.Set(
          policy::key::kDeviceAttributesAllowedForOrigins,
          policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
          policy::POLICY_SOURCE_CLOUD,
          base::Value(base::Value::List().Append(kAppOrigin.Serialize())),
          nullptr);
    }
    policy_provider_.UpdateChromePolicy(policies);
  }

 private:
  void InitFeatureList() {
    if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{blink::features::
                                    kDeviceAttributesPermissionPolicy},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              blink::features::kDeviceAttributesPermissionPolicy});
    }
  }

  web_app::ManifestBuilder GetIwaManifestBuilder() {
    auto manifest_builder = web_app::ManifestBuilder();
    if (IsPermissionsPolicyGranted()) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kDeviceAttributes, true,
          {});
    }
    return manifest_builder;
  }

  void SetDevicePolicies() {
    ScopedDevicePolicyUpdate scoped_update(
        policy_helper_.device_policy(), base::BindLambdaForTesting([this]() {
          policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
              {ash::kAccountsPrefDeviceLocalAccounts});
        }));
    kiosk_.Configure(
        scoped_update,
        GetKioskIwaManualLaunchConfig(
            iwa_test_update_server_.GetUpdateManifestUrl(kTestWebBundleId)));

    scoped_update.policy_data()->set_annotated_asset_id(
        kDeviceAnnotatedAssetId);
    scoped_update.policy_data()->set_annotated_location(
        kDeviceAnnotatedLocation);
    scoped_update.policy_data()->set_directory_api_id(kDeviceDirectoryApiId);
    scoped_update.policy_payload()
        ->mutable_network_hostname()
        ->set_device_hostname_template(kDeviceHostname);

    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kDeviceSerialNumber);
  }

  void LaunchIwaKiosk() {
    ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());

    ASSERT_NE(web_contents(), nullptr);
    ASSERT_EQ(web_contents()->GetVisibleURL(), kAppOrigin.GetURL());
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  const url::Origin kAppOrigin =
      url::Origin::CreateFromNormalizedTuple(webapps::kIsolatedAppScheme,
                                             kTestWebBundleId.id(),
                                             /*port=*/0);

  base::test::ScopedFeatureList feature_list_;
  web_app::IsolatedWebAppTestUpdateServer iwa_test_update_server_;
  KioskMixin kiosk_{&mixin_host_};
  policy::DevicePolicyCrosTestHelper policy_helper_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(KioskIwaDeviceAttributesApiTest,
                       ObtainingDeviceAttributes) {
  const bool device_attributes_should_work =
      IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
          ? !IsBlockPolicySet() && IsPermissionsPolicyGranted()
          : IsAllowPolicySet();
  MaybeSetEnterprisePoliciesForIwaOrigin();

  ASSERT_EQ(kDeviceAttributeNames.size(),
            kExpectedDeviceAttributeValues.size());
  for (size_t i = 0; i < kDeviceAttributeNames.size(); ++i) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents(), kDeviceAttributeNames[i]);
    if (device_attributes_should_work) {
      EXPECT_EQ(kExpectedDeviceAttributeValues[i], result.ExtractString());
    } else {
      std::string expected_error;
      if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() &&
          !IsPermissionsPolicyGranted()) {
        expected_error = base::StringPrintf(kPermissionsPolicyErrorTemplate,
                                            kDeviceAttributeNames[i]);
      } else {
        expected_error = kNoDeviceAttributesPermissionExpectedError;
      }

      EXPECT_THAT(result.ExtractError(), ::testing::StartsWith(expected_error));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaDeviceAttributesApiTest,
    ::testing::ConvertGenerator<KioskIwaDeviceAttributesApiTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Bool(),  // kDeviceAttributesPermissionPolicy
                                // feature flag
            ::testing::Bool(),  // allow policy
            ::testing::Bool(),  // block policy
            ::testing::Bool()   // permissions policy
            )),
    [](const ::testing::TestParamInfo<KioskIwaDeviceAttributesApiTestParams>&
           info) {
      return base::StringPrintf(
          "FeatureFlag%s_AllowPolicy%s_BlockPolicy%s_PermissionsPolicy%s",
          info.param.feature_flag ? "Enabled" : "Disabled",
          info.param.allow_policy ? "Set" : "Unset",
          info.param.block_policy ? "Set" : "Unset",
          info.param.permissions_policy ? "Granted" : "Denied");
    });
}  // namespace ash
