// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

std::unique_ptr<net::test_server::HttpResponse> ServeSimpleHtmlPage(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head><title>Test Page</title></head>"
      "<body>A simple kiosk web page.</body>"
      "</html>");

  return http_response;
}

// Waits until the 'navigator.managed' JS object is defined.
// Used in tests to prevent API calls before the test web page has loaded.
void WaitForDeviceAttributesApiObject(content::WebContents* web_contents) {
  ash::test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents) {
            return content::EvalJs(web_contents,
                                   "typeof navigator.managed !== 'undefined'")
                .ExtractBool();
          },
          web_contents))
      .Wait();
}

content::EvalJsResult CallDeviceAttributesApi(
    content::WebContents* web_contents,
    const std::string& attribute_name) {
  return content::EvalJs(
      web_contents, base::ReplaceStringPlaceholders("navigator.managed.get$1()",
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

constexpr std::array<const char*, 5> kExpectedAttributeValues = {
    kDeviceAnnotatedAssetId, kDeviceAnnotatedLocation, kDeviceDirectoryApiId,
    kDeviceHostname, kDeviceSerialNumber};

constexpr char kNotAllowedOriginExpectedError[] =
    "a JavaScript error: \"UnknownError: The current origin cannot use this "
    "web API because it is not allowed by the "
    "DeviceAttributesAllowedForOrigins policy.\"\n";

}  // namespace

namespace ash {

class WebKioskDeviceAttributesTest : public WebKioskBaseTest {
 public:
  WebKioskDeviceAttributesTest() = default;

  void SetUpOnMainThread() override {
    InitAppServer();
    SetAppInstallUrl(web_app_server_.base_url().spec());
    WebKioskBaseTest::SetUpOnMainThread();
  }

 protected:
  void InitAppServer() {
    web_app_server_.RegisterRequestHandler(
        base::BindRepeating(&ServeSimpleHtmlPage));
    ASSERT_TRUE(web_app_server_handle_ =
                    web_app_server_.StartAndReturnHandle());
  }

  void SetDevicePolicy() {
    device_policy().SetDefaultSigningKey();
    device_policy().policy_data().set_annotated_asset_id(
        kDeviceAnnotatedAssetId);
    device_policy().policy_data().set_annotated_location(
        kDeviceAnnotatedLocation);
    device_policy().policy_data().set_directory_api_id(kDeviceDirectoryApiId);
    enterprise_management::NetworkHostnameProto* hostname_proto =
        device_policy().payload().mutable_network_hostname();
    hostname_proto->set_device_hostname_template(kDeviceHostname);
    device_policy().Build();
    policy_helper_.RefreshDevicePolicy();
  }

  void SetSerialNumber() {
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, kDeviceSerialNumber);
  }

  content::WebContents* GetKioskAppWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

  void AllowDeviceAttributesForWebApp() {
    browser()->profile()->GetPrefs()->SetList(
        prefs::kDeviceAttributesAllowedForOrigins,
        base::Value::List().Append(web_app_origin()));
  }

 private:
  policy::DevicePolicyBuilder& device_policy() {
    return *(policy_helper_.device_policy());
  }

  std::string web_app_origin() const {
    return web_app_server_.GetOrigin().Serialize();
  }

  net::test_server::EmbeddedTestServer web_app_server_;
  net::test_server::EmbeddedTestServerHandle web_app_server_handle_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       DisallowedOriginCantAccessAPI) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  WaitForDeviceAttributesApiObject(web_contents);

  // All of the methods should be defined, but return the same error.
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_EQ(result.error, kNotAllowedOriginExpectedError);
    EXPECT_TRUE(result.value.is_none());
  }
}

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       AllowedOriginWithAttributesUnset) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  AllowDeviceAttributesForWebApp();

  WaitForDeviceAttributesApiObject(web_contents);

  // All of the methods should be defined, return null and no error.
  for (const std::string& attribute : kAttributeNames) {
    content::EvalJsResult result =
        CallDeviceAttributesApi(web_contents, attribute);
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.value.is_none());
  }
}

IN_PROC_BROWSER_TEST_F(WebKioskDeviceAttributesTest,
                       AllowedOriginWithAttributesSet) {
  SetDevicePolicy();
  SetSerialNumber();

  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  AllowDeviceAttributesForWebApp();

  WaitForDeviceAttributesApiObject(web_contents);

  // All of the methods should be defined and return the values that were set in
  // the device policy.
  ASSERT_EQ(kAttributeNames.size(), kExpectedAttributeValues.size());
  for (size_t i = 0; i < kAttributeNames.size(); ++i) {
    EXPECT_EQ(kExpectedAttributeValues[i],
              CallDeviceAttributesApi(web_contents, kAttributeNames[i])
                  .ExtractString());
  }
}

}  // namespace ash
