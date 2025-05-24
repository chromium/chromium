// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"
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

constexpr char kNotAllowedOriginExpectedError[] =
    "a JavaScript error: \"UnknownError: The current origin cannot use this "
    "web API because it is not allowed by the "
    "DeviceAttributesAllowedForOrigins policy.\"\n";

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

// Waits until a `js_name` is defined. Is used to prevent API calls before the
// test web page has loaded.
void WaitForJsObject(content::WebContents* web_contents,
                     const std::string& js_name) {
  ash::test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents, const std::string& js_name) {
            return content::EvalJs(
                       web_contents,
                       base::ReplaceStringPlaceholders(
                           "typeof $1 !== 'undefined'", {js_name}, nullptr))
                .ExtractBool();
          },
          web_contents, js_name))
      .Wait();
}

content::EvalJsResult CallDeviceAttributesApi(
    content::WebContents* web_contents,
    const std::string& attribute_name) {
  return content::EvalJs(
      web_contents, base::ReplaceStringPlaceholders("navigator.managed.get$1()",
                                                    {attribute_name}, nullptr));
}

}  // namespace

class KioskIwaDeviceAttributesApiTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaDeviceAttributesApiTest() {
    iwa_server_mixin_.AddBundle(
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
            .BuildBundle(web_app::test::GetDefaultEd25519KeyPair()));
  }

  ~KioskIwaDeviceAttributesApiTest() override = default;
  KioskIwaDeviceAttributesApiTest(const KioskIwaDeviceAttributesApiTest&) =
      delete;
  KioskIwaDeviceAttributesApiTest& operator=(
      const KioskIwaDeviceAttributesApiTest&) = delete;

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

  void AllowDeviceAttributesForIwaOrigin() {
    browser()->profile()->GetPrefs()->SetList(
        prefs::kDeviceAttributesAllowedForOrigins,
        base::Value::List().Append(kAppOrigin.Serialize()));
  }

 private:
  void SetDevicePolicies() {
    ScopedDevicePolicyUpdate scoped_update(
        policy_helper_.device_policy(), base::BindLambdaForTesting([this]() {
          policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
              {ash::kAccountsPrefDeviceLocalAccounts});
        }));
    kiosk_.Configure(
        scoped_update,
        GetKioskIwaManualLaunchConfig(
            iwa_server_mixin_.GetUpdateManifestUrl(kTestWebBundleId)));

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
    ASSERT_TRUE(WaitKioskLaunched());

    SelectFirstBrowser();
    ASSERT_NE(web_contents(), nullptr);
    ASSERT_EQ(web_contents()->GetVisibleURL(), kAppOrigin.GetURL());
  }

  const url::Origin kAppOrigin =
      url::Origin::CreateFromNormalizedTuple(chrome::kIsolatedAppScheme,
                                             kTestWebBundleId.id(),
                                             /*port=*/0);

  base::test::ScopedFeatureList feature_list_{
      ash::features::kIsolatedWebAppKiosk};
  web_app::IsolatedWebAppUpdateServerMixin iwa_server_mixin_{&mixin_host_};
  KioskMixin kiosk_{&mixin_host_};
  policy::DevicePolicyCrosTestHelper policy_helper_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(KioskIwaDeviceAttributesApiTest,
                       AllowedOriginWithAttributesSet) {
  AllowDeviceAttributesForIwaOrigin();

  WaitForJsObject(web_contents(), "navigator.managed");

  ASSERT_EQ(kDeviceAttributeNames.size(),
            kExpectedDeviceAttributeValues.size());
  for (size_t i = 0; i < kDeviceAttributeNames.size(); ++i) {
    EXPECT_EQ(kExpectedDeviceAttributeValues[i],
              CallDeviceAttributesApi(web_contents(), kDeviceAttributeNames[i])
                  .ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(KioskIwaDeviceAttributesApiTest,
                       DisallowedOriginCantAccessAPI) {
  WaitForJsObject(web_contents(), "navigator.managed");

  // All of the methods should be defined, but return the same error.
  for (const std::string& attribute : kDeviceAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents(), attribute);
    EXPECT_EQ(result.error, kNotAllowedOriginExpectedError);
    EXPECT_TRUE(result.value.is_none());
  }
}

}  // namespace ash
