// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/private_network_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace {
const char kURL[] = "http://example.com";
const char kCookieValue[] = "converted=true";
// Assigned to Philip J. Fry to fix eventually.
// TODO(maksims): use year 3000 when we get rid off the 32-bit
// versions. https://crbug.com/619828
const char kCookieOptions[] = ";expires=Wed Jan 01 2038 00:00:00 GMT";
constexpr int kBlockAll = 2;

bool IsJavascriptEnabled(content::WebContents* contents) {
  return content::ExecJs(
      contents->GetPrimaryMainFrame(), "123",
      content::EvalJsOptions::EXECUTE_SCRIPT_HONOR_JS_CONTENT_SETTINGS);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_DefaultCookiesSetting) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = base::StrCat({kCookieValue, kCookieOptions});
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_DefaultCookiesSetting) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  PolicyMap policies;
  policies.Set(key::kDefaultCookiesSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(CONTENT_SETTING_SESSION_ONLY), nullptr);
  UpdateProviderPolicy(policies);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultCookiesSetting) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_WebsiteCookiesSetting) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = base::StrCat({kCookieValue, kCookieOptions});
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_WebsiteCookiesSetting) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(GURL(kURL), GURL(kURL),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebsiteCookiesSetting) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Javascript) {
  // Verifies that Javascript can be disabled.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsJavascriptEnabled(contents));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));

  // Disable Javascript via policy.
  PolicyMap policies;
  policies.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  // Reload the page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  // Developer tools still work when javascript is disabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));
  // Javascript is always enabled for the internal pages.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL)));
  EXPECT_TRUE(IsJavascriptEnabled(contents));

  // The javascript content setting policy overrides the javascript policy.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  policies.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(CONTENT_SETTING_ALLOW), nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(IsJavascriptEnabled(contents));
}

class WebBluetoothPolicyTest : public PolicyTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(juncai): Remove this switch once Web Bluetooth is supported on Linux
    // and Windows.
    // https://crbug.com/570344
    // https://crbug.com/507419
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    PolicyTest::SetUpCommandLine(command_line);
  }
};

// crbug.com/1061063
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_Block DISABLED_Block
#else
#define MAYBE_Block Block
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
IN_PROC_BROWSER_TEST_F(WebBluetoothPolicyTest, MAYBE_Block) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(testing::Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Navigate to a secure context.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html")));
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_THAT(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().Serialize(),
      testing::StartsWith("http://localhost:"));

  // Set the policy to block Web Bluetooth.
  PolicyMap policies;
  policies.Set(key::kDefaultWebBluetoothGuardSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  UpdateProviderPolicy(policies);

  std::string rejection =
      content::EvalJs(
          web_contents,
          "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
          "  .then(() => 'Success',"
          "        reason => reason.name + ': ' + reason.message"
          "  );")
          .ExtractString();
  EXPECT_THAT(rejection, testing::MatchesRegex("NotFoundError: .*policy.*"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebUsbDefault) {
  const auto kTestOrigin = url::Origin::Create(GURL("https://foo.com:443"));

  // Expect the default permission value to be 'ask'.
  auto* context = UsbChooserContextFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(context->CanRequestObjectPermission(kTestOrigin));

  // Update policy to change the default permission value to 'block'.
  PolicyMap policies;
  SetPolicy(&policies, key::kDefaultWebUsbGuardSetting, base::Value(2));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(context->CanRequestObjectPermission(kTestOrigin));

  // Update policy to change the default permission value to 'ask'.
  SetPolicy(&policies, key::kDefaultWebUsbGuardSetting, base::Value(3));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(context->CanRequestObjectPermission(kTestOrigin));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebUsbAllowDevicesForUrls) {
  const auto kTestOrigin = url::Origin::Create(GURL("https://foo.com:443"));
  scoped_refptr<device::FakeUsbDeviceInfo> device =
      base::MakeRefCounted<device::FakeUsbDeviceInfo>(0, 0, "Google", "Gizmo",
                                                      "123ABC");
  const auto& device_info = device->GetDeviceInfo();

  // Expect the default permission value to be empty.
  auto* context = UsbChooserContextFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(context->HasDevicePermission(kTestOrigin, device_info));

  // Update policy to add an entry to the permission value to allow
  // |kTestOrigin| to access the device described by |device_info|.
  PolicyMap policies;

  base::Value::Dict device_value;
  device_value.Set("vendor_id", 0);
  device_value.Set("product_id", 0);

  base::Value::List devices_value;
  devices_value.Append(std::move(device_value));

  base::Value::List urls_value;
  urls_value.Append(base::Value("https://foo.com"));

  base::Value::Dict entry;
  entry.Set("devices", std::move(devices_value));
  entry.Set("urls", std::move(urls_value));

  base::Value::List policy_value;
  policy_value.Append(std::move(entry));

  SetPolicy(&policies, key::kWebUsbAllowDevicesForUrls,
            base::Value(std::move(policy_value)));
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(context->HasDevicePermission(kTestOrigin, device_info));

  // Remove the policy to ensure that it can be dynamically updated.
  SetPolicy(&policies, key::kWebUsbAllowDevicesForUrls,
            base::Value(base::Value::Type::LIST));
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(context->HasDevicePermission(kTestOrigin, device_info));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ShouldAllowInsecurePrivateNetworkRequests) {
  const auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // By default, we should block requests.
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("http://bleep.com"))));

  PolicyMap policies;
  SetPolicy(&policies, key::kInsecurePrivateNetworkRequestsAllowed,
            base::Value(false));
  UpdateProviderPolicy(policies);

  // Explicitly-disallowing is the same as not setting the policy.
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("http://bleep.com"))));

  base::Value::List allowlist;
  allowlist.Append(base::Value("http://bleep.com"));
  allowlist.Append(base::Value("http://woohoo.com:1234"));
  SetPolicy(&policies, key::kInsecurePrivateNetworkRequestsAllowedForUrls,
            base::Value(std::move(allowlist)));
  UpdateProviderPolicy(policies);

  // Domain is not the in allowlist.
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("http://default.com"))));

  // Path does not matter, only the origin.
  EXPECT_TRUE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("http://bleep.com/heyo"))));

  // Scheme matters: https is not http.
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("https://bleep.com"))));

  // Port is checked too.
  EXPECT_TRUE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map,
      url::Origin::Create(GURL("http://woohoo.com:1234/index.html"))));

  // The wrong port does not match (default is 80).
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map, url::Origin::Create(GURL("http://woohoo.com/index.html"))));

  // Opaque origins never match the allowlist.
  EXPECT_FALSE(content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      settings_map,
      url::Origin::Create(GURL("http://bleep.com")).DeriveNewOpaqueOrigin()));
}

class ScrollToTextFragmentPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Set policies before the browser starts up.
    PolicyMap policies;
    policies.Set(key::kScrollToTextFragmentEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(IsScrollToTextFragmentEnabled()), nullptr);
    UpdateProviderPolicy(policies);
    PolicyTest::CreatedBrowserMainParts(browser_main_parts);
  }

  bool IsScrollToTextFragmentEnabled() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ScrollToTextFragmentPolicyTest, RunPolicyTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html#:~:text=text"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), target_text_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  ASSERT_TRUE(
      content::WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));

  content::RenderFrameSubmissionObserver frame_observer(contents);
  if (IsScrollToTextFragmentEnabled()) {
    frame_observer.WaitForScrollOffsetAtTop(false);
  } else {
    // Force a frame - if it were going to happen, the scroll would complete
    // before this forced frame makes its way through the pipeline.
    content::RunUntilInputProcessed(
        contents->GetPrimaryMainFrame()->GetView()->GetRenderWidgetHost());
  }
  EXPECT_EQ(IsScrollToTextFragmentEnabled(),
            !frame_observer.LastRenderFrameMetadata().is_scroll_offset_at_top);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollToTextFragmentPolicyTest,
                         ::testing::Bool());

class SensorsPolicyTest : public PolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sensors API is behind Experimental Web Platform Features flag.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    PolicyTest::SetUpCommandLine(command_line);
  }

  void VerifyPermission(const char* url,
                        blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        browser()->profile()->GetPermissionController();
    EXPECT_EQ(
        permission_controller
            ->GetPermissionResultForOriginWithoutContext(
                blink::PermissionType::SENSORS, url::Origin::Create(GURL(url)))
            .status,
        status);
  }

  void AllowUrl(const char* url) {
    base::Value::List policy_value;
    policy_value.Append(url);
    SetPolicy(&policies_, key::kSensorsAllowedForUrls,
              base::Value(std::move(policy_value)));
    UpdateProviderPolicy(policies_);
  }

  void BlockUrl(const char* url) {
    base::Value::List policy_value;
    policy_value.Append(url);
    SetPolicy(&policies_, key::kSensorsBlockedForUrls,
              base::Value(std::move(policy_value)));
    UpdateProviderPolicy(policies_);
  }

  void ClearLists() {
    base::Value::List policy_value_allow;
    base::Value::List policy_value_block;
    SetPolicy(&policies_, key::kSensorsAllowedForUrls,
              base::Value(std::move(policy_value_allow)));
    SetPolicy(&policies_, key::kSensorsBlockedForUrls,
              base::Value(std::move(policy_value_block)));
    UpdateProviderPolicy(policies_);
  }

  void SetDefault(int default_value) {
    SetPolicy(&policies_, key::kDefaultSensorsSetting,
              base::Value(default_value));
    UpdateProviderPolicy(policies_);
  }

 private:
  PolicyMap policies_;
};

IN_PROC_BROWSER_TEST_F(SensorsPolicyTest, BlockSensorApi) {
  // Navigate to a secure context.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html")));
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_THAT(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().Serialize(),
      testing::StartsWith("http://localhost:"));

  // Set the policy to block Sensors.
  SetDefault(kBlockAll);

  std::string rejection =
      content::EvalJs(
          web_contents,
          "const sensor = new AmbientLightSensor();"
          "new Promise(resolve => {"
          "  sensor.onreading = () => { resolve('Success'); };"
          "  sensor.onerror = (event) => {"
          "    resolve(event.error.name + ': ' +  event.error.message);"
          "  };"
          "  sensor.start();"
          "});")
          .ExtractString();
  EXPECT_THAT(rejection,
              testing::MatchesRegex("NotAllowedError: .*Permissions.*"));
}

IN_PROC_BROWSER_TEST_F(SensorsPolicyTest, DynamicRefresh) {
  constexpr char kFooUrl[] = "https://foo.sensor";
  constexpr char kBarUrl[] = "https://bar.sensor";
  constexpr int kAllowAll = 1;

  BlockUrl(kFooUrl);
  VerifyPermission(kFooUrl, blink::mojom::PermissionStatus::DENIED);
  VerifyPermission(kBarUrl, blink::mojom::PermissionStatus::GRANTED);

  BlockUrl(kBarUrl);
  VerifyPermission(kFooUrl, blink::mojom::PermissionStatus::GRANTED);
  VerifyPermission(kBarUrl, blink::mojom::PermissionStatus::DENIED);

  SetDefault(kBlockAll);
  ClearLists();
  AllowUrl(kFooUrl);
  VerifyPermission(kFooUrl, blink::mojom::PermissionStatus::GRANTED);
  VerifyPermission(kBarUrl, blink::mojom::PermissionStatus::DENIED);

  AllowUrl(kBarUrl);
  VerifyPermission(kFooUrl, blink::mojom::PermissionStatus::DENIED);
  VerifyPermission(kBarUrl, blink::mojom::PermissionStatus::GRANTED);

  SetDefault(kAllowAll);
  ClearLists();
  VerifyPermission(kFooUrl, blink::mojom::PermissionStatus::GRANTED);
  VerifyPermission(kBarUrl, blink::mojom::PermissionStatus::GRANTED);
}

#if BUILDFLAG(IS_CHROMEOS)
class WebPrintingPolicyTest : public PolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitAndEnableFeature(blink::features::kWebPrinting);
    PolicyTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestingUrl()));
  }

 protected:
  static constexpr int32_t kBlockSetting = 2;
  static constexpr int32_t kAskSetting = 3;

  GURL GetTestingUrl() const {
    return embedded_test_server()->GetURL("/empty.html");
  }

  ContentSetting GetWebPrintingDefaultContentSetting() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetDefaultContentSetting(ContentSettingsType::WEB_PRINTING,
                                   /*provider_id=*/nullptr);
  }

  ContentSetting GetWebPrintingContentSetting(const GURL& url) {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetContentSetting(/*primary_url=*/url, /*secondary_url=*/url,
                            ContentSettingsType::WEB_PRINTING);
  }

  void SetDefaultWebPrintingSetting(int32_t setting) {
    PolicyMap policies;
    SetPolicy(&policies, key::kDefaultWebPrintingSetting, base::Value(setting));
    UpdateProviderPolicy(policies);
  }

  void SetWebPrintingAllowedFor(const GURL& url) {
    PolicyMap policies;
    SetPolicy(&policies, key::kWebPrintingAllowedForUrls,
              base::Value(base::Value::List().Append(url.spec())));
    UpdateProviderPolicy(policies);
  }

  void SetWebPrintingBlockedFor(const GURL& url) {
    PolicyMap policies;
    SetPolicy(&policies, key::kWebPrintingBlockedForUrls,
              base::Value(base::Value::List().Append(url.spec())));
    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebPrintingPolicyTest, DefaultWebPrintingSetting) {
  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingContentSetting(GetTestingUrl()));

  SetDefaultWebPrintingSetting(kBlockSetting);

  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetWebPrintingDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetWebPrintingContentSetting(GetTestingUrl()));

  SetDefaultWebPrintingSetting(kAskSetting);

  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingContentSetting(GetTestingUrl()));
}

IN_PROC_BROWSER_TEST_F(WebPrintingPolicyTest, WebPrintingAllowedForUrls) {
  SetWebPrintingAllowedFor(GetTestingUrl());

  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetWebPrintingContentSetting(GetTestingUrl()));
}

IN_PROC_BROWSER_TEST_F(WebPrintingPolicyTest, WebPrintingBlockedForUrls) {
  SetWebPrintingBlockedFor(GetTestingUrl());

  EXPECT_EQ(CONTENT_SETTING_ASK, GetWebPrintingDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetWebPrintingContentSetting(GetTestingUrl()));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
class DirectSocketsPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestingUrl()));
  }

 protected:
  GURL GetTestingUrl() const {
    return embedded_test_server()->GetURL("/empty.html");
  }

  ContentSetting GetDirectSocketsDefaultContentSetting() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetDefaultContentSetting(ContentSettingsType::DIRECT_SOCKETS,
                                   /*provider_id=*/nullptr);
  }

  ContentSetting GetDirectSocketsContentSetting(const GURL& url) {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetContentSetting(/*primary_url=*/url, /*secondary_url=*/url,
                            ContentSettingsType::DIRECT_SOCKETS);
  }

  void SetDefaultDirectSocketsSettingToBlocked() {
    SetPolicy(&policies_, key::kDefaultDirectSocketsSetting,
              base::Value(kBlockSetting));
    UpdateProviderPolicy(policies_);
  }

  void SetDirectSocketsAllowedFor(const GURL& url) {
    SetPolicy(&policies_, key::kDirectSocketsAllowedForUrls,
              base::Value(base::Value::List().Append(url.spec())));
    UpdateProviderPolicy(policies_);
  }

  void SetDirectSocketsBlockedFor(const GURL& url) {
    SetPolicy(&policies_, key::kDirectSocketsBlockedForUrls,
              base::Value(base::Value::List().Append(url.spec())));
    UpdateProviderPolicy(policies_);
  }

 private:
  static constexpr int32_t kBlockSetting = 2;
  base::test::ScopedFeatureList feature_list_;
  PolicyMap policies_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsPolicyTest, DefaultDirectSocketsSetting) {
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetDirectSocketsDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetDirectSocketsContentSetting(GetTestingUrl()));

  SetDefaultDirectSocketsSettingToBlocked();

  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetDirectSocketsDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetDirectSocketsContentSetting(GetTestingUrl()));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsPolicyTest, DirectSocketsAllowedForUrls) {
  SetDefaultDirectSocketsSettingToBlocked();
  SetDirectSocketsAllowedFor(GetTestingUrl());

  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetDirectSocketsDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetDirectSocketsContentSetting(GetTestingUrl()));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsPolicyTest, DirectSocketsBlockedForUrls) {
  SetDirectSocketsBlockedFor(GetTestingUrl());

  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetDirectSocketsDefaultContentSetting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetDirectSocketsContentSetting(GetTestingUrl()));
}
#endif

}  // namespace policy
