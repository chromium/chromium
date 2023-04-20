// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/hid/hid_pinned_notification.h"
#else
#include "chrome/browser/hid/hid_status_icon.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using ::extensions::Extension;
using ::testing::Return;

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr char kManifestTemplate[] =
    R"({
          "name": "Test Extension",
          "version": "0.1",
          "manifest_version": 3,
          "background": {
            "service_worker": "%s"
          }
        })";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const AccountId kManagedUserAccountId =
    AccountId::FromUserEmail("example@example.com");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Need to fill it with an url.
constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["%s"]
      }
    ])";

// Create a device with a single collection containing an input report and an
// output report. Both reports have report ID 0.
device::mojom::HidDeviceInfoPtr CreateTestDeviceWithInputAndOutputReports() {
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(0x0001, 0xff00);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());

  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = "test-guid";
  device->collections.push_back(std::move(collection));
  // vendor_id and product_id needs to match setting in kPolicySetting
  device->vendor_id = 1234;
  device->product_id = 5678;

  return device;
}

}  // namespace

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Base Test fixture with kEnableWebHidOnExtensionServiceWorker default
// disabled.
class WebHidExtensionBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  WebHidExtensionBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());

    // Connect the HidManager and ensure we've received the initial enumeration
    // before continuing.
    base::RunLoop run_loop;
    auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
    chooser_context->SetHidManagerForTesting(
        std::move(hid_manager),
        base::BindLambdaForTesting(
            [&run_loop](std::vector<device::mojom::HidDeviceInfoPtr> devices) {
              run_loop.Quit();
            }));
    run_loop.Run();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // This is to set up affliated user for chromeos ash environment.
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager->AddUserWithAffiliation(kManagedUserAccountId, true);
    fake_user_manager->LoginUser(kManagedUserAccountId);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    display_service_for_system_notification_ =
        std::make_unique<NotificationDisplayServiceTester>(
            /*profile=*/nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(kManagedUserAccountId);
    scoped_user_manager_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetUpPolicy(const Extension* extension) {
    g_browser_process->local_state()->Set(
        prefs::kManagedWebHidAllowDevicesForUrls,
        base::test::ParseJson(base::StringPrintf(
            kPolicySetting, extension->url().spec().c_str())));
  }

  const Extension* LoadExtensionAndRunTest(const std::string& kBackgroundJs) {
    extensions::TestExtensionDir test_dir;

    test_dir.WriteManifest(
        base::StringPrintf(kManifestTemplate, "background.js"));
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

    // Launch the test app.
    ExtensionTestMessageListener ready_listener("ready",
                                                ReplyBehavior::kWillReply);
    extensions::ResultCatcher result_catcher;
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());

    // TODO(crbug.com/1336400): Grant permission using requestDevice().
    // Run the test.
    SetUpPolicy(extension);
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
    ready_listener.Reply("ok");
    EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

    return extension;
  }

  device::FakeHidManager* hid_manager() { return &hid_manager_; }

  void SimulateClickOnSystemTrayIconButton(Browser* browser,
                                           const Extension* extension) {
#if BUILDFLAG(IS_CHROMEOS)
    auto expected_pinned_notification_id =
        HidPinnedNotification::GetNotificationId(browser->profile());
    auto maybe_indicator_notification =
        display_service_for_system_notification_->GetNotification(
            expected_pinned_notification_id);
    ASSERT_TRUE(maybe_indicator_notification);
    EXPECT_TRUE(maybe_indicator_notification->pinned());
    display_service_for_system_notification_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, expected_pinned_notification_id,
        /*action_index=*/0, /*reply=*/absl::nullopt);
    auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetURL(), "chrome://settings/content/hidDevices");
#else
    // On non-ChromeOS platforms, as they use status icon and there isn't good
    // test infra to simulate click on the status icon button, so simulate the
    // click event by invoking ExecuteCommand of HidConnectionTracker directly.
    auto* hid_status_icon =
        static_cast<HidStatusIcon*>(g_browser_process->hid_system_tray_icon());

    hid_status_icon->ExecuteCommand(IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "https://support.google.com/chrome?p=webhid");

    hid_status_icon->ExecuteCommand(IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 1, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "chrome://settings/content/hidDevices");

    hid_status_icon->ExecuteCommand(IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 2, 0);
    EXPECT_EQ(
        browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
        "chrome://settings/content/siteDetails?site=chrome-extension%3A%2F%2F" +
            extension->id());
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<NotificationDisplayServiceTester>
      display_service_for_system_notification_;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  device::FakeHidManager hid_manager_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Test fixture with kEnableWebHidOnExtensionServiceWorker enabled.
class WebHidExtensionFeatureEnabledBrowserTest
    : public WebHidExtensionBrowserTest {
 public:
  WebHidExtensionFeatureEnabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnableWebHidOnExtensionServiceWorker}, {});
  }
};

// Test fixture with kEnableWebHidOnExtensionServiceWorker disabled.
class WebHidExtensionFeatureDisabledBrowserTest
    : public WebHidExtensionBrowserTest {
 public:
  WebHidExtensionFeatureDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kEnableWebHidOnExtensionServiceWorker});
  }
};

IN_PROC_BROWSER_TEST_F(WebHidExtensionBrowserTest, FeatureDefaultDisabled) {
  extensions::TestExtensionDir test_dir;

  constexpr char kBackgroundJs[] = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        chrome.test.assertEq(navigator.hid, undefined);
        chrome.test.notifyPass();

      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";

  LoadExtensionAndRunTest(kBackgroundJs);
}

IN_PROC_BROWSER_TEST_F(WebHidExtensionFeatureDisabledBrowserTest,
                       FeatureDisabled) {
  extensions::TestExtensionDir test_dir;

  constexpr char kBackgroundJs[] = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        chrome.test.assertEq(navigator.hid, undefined);
        chrome.test.notifyPass();

      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";

  LoadExtensionAndRunTest(kBackgroundJs);
}

IN_PROC_BROWSER_TEST_F(WebHidExtensionFeatureEnabledBrowserTest, GetDevices) {
  extensions::TestExtensionDir test_dir;

  auto device = CreateTestDeviceWithInputAndOutputReports();
  hid_manager()->AddDevice(std::move(device));

  constexpr char kBackgroundJs[] = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        const devices = await navigator.hid.getDevices();
        chrome.test.assertEq(1, devices.length);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";

  LoadExtensionAndRunTest(kBackgroundJs);
}

IN_PROC_BROWSER_TEST_F(WebHidExtensionFeatureEnabledBrowserTest,
                       RequestDevice) {
  extensions::TestExtensionDir test_dir;

  constexpr char kBackgroundJs[] = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        chrome.test.assertEq(navigator.hid.requestDevice, undefined);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";

  LoadExtensionAndRunTest(kBackgroundJs);
}

IN_PROC_BROWSER_TEST_F(WebHidExtensionFeatureEnabledBrowserTest,
                       HidConnectionTracker) {
  auto device = CreateTestDeviceWithInputAndOutputReports();
  hid_manager()->AddDevice(std::move(device));

  constexpr char kBackgroundJs[] = R"(
    // |device| is a global variable to store HidDevice object being tested in
    // case the local one is garbage collected, which can close the connection.
    var device;
    chrome.test.sendMessage("ready", async () => {
      try {
        const devices = await navigator.hid.getDevices();
        device = devices[0];
        chrome.test.assertEq(1, devices.length);
        await device.open();
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";
  const auto* extension = LoadExtensionAndRunTest(kBackgroundJs);
  SimulateClickOnSystemTrayIconButton(browser(), extension);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
