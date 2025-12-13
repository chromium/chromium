// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstddef>
#include <string>
#include <tuple>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/permissions/features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/origin.h"

namespace {

// Device Attribute APIs is defined when the page finishes loading.
// Used in tests to prevent API calls before the test web page has loaded.
void WaitForDeviceAttributesApiObject(content::WebContents& web_contents) {
  ASSERT_TRUE(WaitForLoadStop(&web_contents));
}

content::EvalJsResult CallDeviceAttributesApi(
    content::WebContents& web_contents,
    const std::string& attribute_name) {
  return content::EvalJs(&web_contents, base::ReplaceStringPlaceholders(
                                            "navigator.managed.get$1()",
                                            {attribute_name}, nullptr));
}

constexpr std::array<const char*, 5> kAttributeNames = {
    "AnnotatedAssetId", "AnnotatedLocation", "DirectoryId", "Hostname",
    "SerialNumber"};

constexpr char kDeviceAnnotatedAssetId[] = "kiosk_test_asset_id";
constexpr char kDeviceAnnotatedLocation[] = "kiosk_test_location";
constexpr char kDeviceDirectoryApiId[] = "kiosk_test_directory_id";
constexpr char kDeviceHostname[] = "kiosk_test_hostname";
constexpr char kDeviceSerialNumber[] = "kiosk_test_serial_number";
constexpr char kKioskOrigin[] = "https://kiosk.com";
constexpr char kTrustedOrigin[] = "https://trusted.com";
constexpr char kUntrustedOrigin[] = "https://untrusted.com";
constexpr char kKioskAccountId[] = "kiosk@account";
constexpr char kKioskPagePath[] = "title3.html";
constexpr base::FilePath::StringViewType kPathToBeServedInWebApp =
    FILE_PATH_LITERAL("chrome/test/data");

constexpr std::array<const char*, 5> kExpectedAttributeValues = {
    kDeviceAnnotatedAssetId, kDeviceAnnotatedLocation, kDeviceDirectoryApiId,
    kDeviceHostname, kDeviceSerialNumber};

constexpr char kNoDeviceAttributesPermissionExpectedError[] =
    "a JavaScript error: \"UnknownError: The current origin cannot use this "
    "web API because it was not granted the 'device-attributes' "
    "permission.\"\n";

constexpr char kNotTrustedOriginExpectedError[] =
    "a JavaScript error: \"NotAllowedError: Service connection error. This API "
    "is available only for managed apps.\"\n";

struct WebKioskDeviceAttributesTestParams {
  using TupleT = std::tuple<bool, bool, bool>;
  bool feature_flag;
  bool allow_policy;
  bool block_policy;
  explicit WebKioskDeviceAttributesTestParams(TupleT t)
      : feature_flag(std::get<0>(t)),
        allow_policy(std::get<1>(t)),
        block_policy(std::get<2>(t)) {}
};

}  // namespace

namespace ash {

using kiosk::test::WaitKioskLaunched;

class WebKioskDeviceAttributesTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<WebKioskDeviceAttributesTestParams> {
 public:
  WebKioskDeviceAttributesTest() {
    EnableFeatureAndAllowlistKioskOrigin(kTrustedOrigin);
  }

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
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());
  }

 protected:
  bool IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() {
    return GetParam().feature_flag;
  }
  bool IsAllowPolicySet() { return GetParam().allow_policy; }
  bool IsBlockPolicySet() { return GetParam().block_policy; }

  void MaybeSetEnterprisePoliciesForOrigin(const std::string& url) {
    if (!IsAllowPolicySet() && !IsBlockPolicySet()) {
      return;
    }
    policy::PolicyMap policies;
    if (IsBlockPolicySet()) {
      policies.Set(policy::key::kDeviceAttributesBlockedForOrigins,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(base::Value::List().Append(url)), nullptr);
    }
    if (IsAllowPolicySet()) {
      policies.Set(policy::key::kDeviceAttributesAllowedForOrigins,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(base::Value::List().Append(url)), nullptr);
    }
    policy_provider_.UpdateChromePolicy(policies);
  }

  void SetDeviceAttributes() {
    auto policy_update =
        kiosk_.device_state_mixin().RequestDevicePolicyUpdate();
    policy_update->policy_data()->set_annotated_asset_id(
        kDeviceAnnotatedAssetId);
    policy_update->policy_data()->set_annotated_location(
        kDeviceAnnotatedLocation);
    policy_update->policy_data()->set_directory_api_id(kDeviceDirectoryApiId);
    enterprise_management::NetworkHostnameProto* hostname_proto =
        policy_update->policy_payload()->mutable_network_hostname();
    hostname_proto->set_device_hostname_template(kDeviceHostname);

    // Set device serial number
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kDeviceSerialNumber);
  }

  content::WebContents& GetKioskAppWebContents() {
    BrowserView& browser_view =
        CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(browser()));
    return CHECK_DEREF(browser_view.GetActiveWebContents());
  }

  void EnableFeatureAndAllowlistKioskOrigin(const std::string& origin) {
    base::FieldTrialParams feature_params;
    feature_params[permissions::feature_params::
                       kWebKioskBrowserPermissionsAllowlist.name] = origin;
    if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()) {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
            feature_params},
           {blink::features::kDeviceAttributesPermissionPolicy, {}}},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
            feature_params}},
          /*disabled_features=*/{
              blink::features::kDeviceAttributesPermissionPolicy});
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  FakeOriginTestServerMixin kiosk_page_mixin_{&mixin_host_, GURL(kKioskOrigin),
                                              kPathToBeServedInWebApp};
  FakeOriginTestServerMixin trusted_page_mixin_{
      &mixin_host_, GURL(kTrustedOrigin), kPathToBeServedInWebApp};
  FakeOriginTestServerMixin untrusted_page_mixin_{
      &mixin_host_, GURL(kUntrustedOrigin), kPathToBeServedInWebApp};
  KioskMixin kiosk_{
      &mixin_host_,
      KioskMixin::Config{/*name=*/{},
                         KioskMixin::AutoLaunchAccount{kKioskAccountId},
                         {KioskMixin::WebAppOption(
                             kKioskAccountId,
                             GURL(kKioskOrigin).Resolve(kKioskPagePath))}}};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(WebKioskDeviceAttributesTest,
                       KioskOriginWithAttributesUnset) {
  content::WebContents& web_contents = GetKioskAppWebContents();
  const bool device_attributes_should_work =
      IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
          ? !IsBlockPolicySet()
          : IsAllowPolicySet();
  MaybeSetEnterprisePoliciesForOrigin(kKioskOrigin);

  WaitForDeviceAttributesApiObject(web_contents);

  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    if (device_attributes_should_work) {
      EXPECT_EQ(result, base::Value());
    } else {
      EXPECT_EQ(result.ExtractError(),
                kNoDeviceAttributesPermissionExpectedError);
    }
  }
}

IN_PROC_BROWSER_TEST_P(WebKioskDeviceAttributesTest,
                       KioskOriginWithAttributesSet) {
  SetDeviceAttributes();

  content::WebContents& web_contents = GetKioskAppWebContents();
  const bool device_attributes_should_work =
      IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
          ? !IsBlockPolicySet()
          : IsAllowPolicySet();
  MaybeSetEnterprisePoliciesForOrigin(kKioskOrigin);

  WaitForDeviceAttributesApiObject(web_contents);

  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (size_t i = 0; i < kAttributeNames.size(); ++i) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, kAttributeNames[i]);
    if (device_attributes_should_work) {
      EXPECT_EQ(kExpectedAttributeValues[i], result.ExtractString());
    } else {
      EXPECT_EQ(result.ExtractError(),
                kNoDeviceAttributesPermissionExpectedError);
    }
  }
}

IN_PROC_BROWSER_TEST_P(WebKioskDeviceAttributesTest,
                       TrustedOriginAllowedByFeature) {
  SetDeviceAttributes();

  content::WebContents& web_contents = GetKioskAppWebContents();
  const bool device_attributes_should_work =
      IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
          ? !IsBlockPolicySet()
          : IsAllowPolicySet();
  MaybeSetEnterprisePoliciesForOrigin(kTrustedOrigin);
  ASSERT_TRUE(content::NavigateToURL(
      &web_contents, GURL(kTrustedOrigin).Resolve(kKioskPagePath)));
  WaitForDeviceAttributesApiObject(web_contents);

  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (size_t i = 0; i < kAttributeNames.size(); ++i) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, kAttributeNames[i]);
    if (device_attributes_should_work) {
      EXPECT_EQ(kExpectedAttributeValues[i], result.ExtractString());
    } else {
      EXPECT_EQ(result.ExtractError(),
                kNoDeviceAttributesPermissionExpectedError);
    }
  }
}

IN_PROC_BROWSER_TEST_P(
    WebKioskDeviceAttributesTest,
    UntrustedKioskOriginsCannotAccessDeviceAttributesWhenAllowedByFeature) {
  SetDeviceAttributes();

  content::WebContents& web_contents = GetKioskAppWebContents();
  MaybeSetEnterprisePoliciesForOrigin(kTrustedOrigin);
  ASSERT_TRUE(content::NavigateToURL(
      &web_contents, GURL(kUntrustedOrigin).Resolve(kKioskPagePath)));
  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return the same error.
  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_EQ(result.ExtractError(), kNotTrustedOriginExpectedError);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebKioskDeviceAttributesTest,
    ::testing::ConvertGenerator<WebKioskDeviceAttributesTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Bool(),  // kDeviceAttributesPermissionPolicy
                                // feature flag
            ::testing::Bool(),  // allow policy
            ::testing::Bool()   // block policy
            )),
    [](const ::testing::TestParamInfo<WebKioskDeviceAttributesTestParams>&
           info) {
      return base::StringPrintf(
          "FeatureFlag%s_AllowPolicy%s_BlockPolicy%s",
          info.param.feature_flag ? "Enabled" : "Disabled",
          info.param.allow_policy ? "Set" : "Unset",
          info.param.block_policy ? "Set" : "Unset");
    });

}  // namespace ash
