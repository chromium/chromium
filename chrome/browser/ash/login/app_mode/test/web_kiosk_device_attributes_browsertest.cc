// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

// Waits until the 'navigator.managed' JS object is defined.
// Used in tests to prevent API calls before the test web page has loaded.
void WaitForDeviceAttributesApiObject(content::WebContents& web_contents) {
  ash::test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents) {
            return content::EvalJs(web_contents,
                                   "typeof navigator.managed !== 'undefined'")
                .ExtractBool();
          },
          &web_contents))
      .Wait();
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

constexpr char kNotAllowedOriginExpectedError[] =
    "a JavaScript error: \"UnknownError: The current origin cannot use this "
    "web API because it is not allowed by the "
    "DeviceAttributesAllowedForOrigins policy.\"\n";

constexpr char kNotTrustedOriginExpectedError[] =
    "a JavaScript error: \"NotAllowedError: Service connection error. This API "
    "is available only for managed apps.\"\n";

}  // namespace

namespace ash {

using kiosk::test::WaitKioskLaunched;

class WebKioskDeviceAttributesTest : public MixinBasedInProcessBrowserTest {
 public:
  WebKioskDeviceAttributesTest() {
    EnableFeatureAndAllowlistKioskOrigin(kTrustedOrigin);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
    SelectFirstBrowser();
  }

 protected:
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

  void AllowDeviceAttributesForOrigin(const std::string& origin) {
    browser()->profile()->GetPrefs()->SetList(
        prefs::kDeviceAttributesAllowedForOrigins,
        base::Value::List().Append(origin));
  }

  void EnableFeatureAndAllowlistKioskOrigin(const std::string& origin) {
    base::FieldTrialParams feature_params;
    feature_params[permissions::feature_params::
                       kWebKioskBrowserPermissionsAllowlist.name] = origin;
    feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
        feature_params);
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
};

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       DisallowedOriginCantAccessAPI) {
  content::WebContents& web_contents = GetKioskAppWebContents();

  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return the same error.
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_EQ(result.error, kNotAllowedOriginExpectedError);
    EXPECT_TRUE(result.value.is_none());
  }
}

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       AllowedOriginWithAttributesUnset) {
  AllowDeviceAttributesForOrigin(kKioskOrigin);
  content::WebContents& web_contents = GetKioskAppWebContents();

  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return null and no error.
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.value.is_none());
  }
}

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       AllowedOriginWithAttributesSet) {
  SetDeviceAttributes();
  AllowDeviceAttributesForOrigin(kKioskOrigin);

  content::WebContents& web_contents = GetKioskAppWebContents();

  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return the values that were set in the device policy.
  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (size_t i = 0; i < kAttributeNames.size(); ++i) {
    EXPECT_EQ(kExpectedAttributeValues[i],
              CallDeviceAttributesApi(web_contents, kAttributeNames[i])
                  .ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(
    WebKioskDeviceAttributesTest,
    TrustedKioskOriginsCanAccessDeviceAttributesWhenAllowedByFeature) {
  SetDeviceAttributes();
  AllowDeviceAttributesForOrigin(kTrustedOrigin);

  content::WebContents& web_contents = GetKioskAppWebContents();
  ASSERT_TRUE(content::NavigateToURL(
      &web_contents, GURL(kTrustedOrigin).Resolve(kKioskPagePath)));

  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return the values that were set in the device policy.
  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (size_t i = 0; i < kAttributeNames.size(); ++i) {
    EXPECT_EQ(kExpectedAttributeValues[i],
              CallDeviceAttributesApi(web_contents, kAttributeNames[i])
                  .ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(
    WebKioskDeviceAttributesTest,
    UntrustedKioskOriginsCannotAccessDeviceAttributesWhenAllowedByFeature) {
  SetDeviceAttributes();
  AllowDeviceAttributesForOrigin(kTrustedOrigin);

  content::WebContents& web_contents = GetKioskAppWebContents();
  ASSERT_TRUE(content::NavigateToURL(
      &web_contents, GURL(kUntrustedOrigin).Resolve(kKioskPagePath)));

  WaitForDeviceAttributesApiObject(web_contents);

  // All methods should return the same error.
  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_EQ(result.error, kNotTrustedOriginExpectedError);
    EXPECT_TRUE(result.value.is_none());
  }
}

}  // namespace ash
