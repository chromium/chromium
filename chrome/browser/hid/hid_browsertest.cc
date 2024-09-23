// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
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

using ::testing::Return;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using ::extensions::Extension;
using ::extensions::ExtensionId;
using ::extensions::TestExtensionDir;

const char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
// Key for extension id `kTestExtensionId`.
constexpr const char kTestExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
    "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
    "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
    "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
    "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
    "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";

// Observer for an extension service worker events like start, activated, and
// stop.
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  TestServiceWorkerContextObserver(content::ServiceWorkerContext* context,
                                   const ExtensionId& extension_id)
      : extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)) {
    scoped_observation_.Observe(context);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  ~TestServiceWorkerContextObserver() override = default;

  void WaitForWorkerStart() {
    started_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerActivated() {
    activated_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerStop() {
    stopped_run_loop_.Run();
    EXPECT_EQ(running_version_id_, std::nullopt);
  }

  int64_t GetServiceWorkerVersionId() { return running_version_id_.value(); }

 private:
  // ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }
    running_version_id_ = version_id;
    started_run_loop_.Quit();
  }

  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    if (running_version_id_ != version_id) {
      return;
    }
    activated_run_loop_.Quit();
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    if (running_version_id_ != version_id) {
      return;
    }
    stopped_run_loop_.Quit();
    running_version_id_ = std::nullopt;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    ASSERT_TRUE(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
  }

  base::RunLoop started_run_loop_;
  base::RunLoop activated_run_loop_;
  base::RunLoop stopped_run_loop_;
  std::optional<int64_t> running_version_id_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
  GURL extension_url_;
};

class TestServiceWorkerConsoleObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit TestServiceWorkerConsoleObserver(
      content::BrowserContext* browser_context) {
    content::StoragePartition* partition =
        browser_context->GetDefaultStoragePartition();
    scoped_observation_.Observe(partition->GetServiceWorkerContext());
  }
  ~TestServiceWorkerConsoleObserver() override = default;

  TestServiceWorkerConsoleObserver(const TestServiceWorkerConsoleObserver&) =
      delete;
  TestServiceWorkerConsoleObserver& operator=(
      const TestServiceWorkerConsoleObserver&) = delete;

  using Message = content::ConsoleMessage;
  const std::vector<Message>& messages() const { return messages_; }

  void WaitForMessages() { run_loop_.Run(); }

 private:
  // ServiceWorkerContextObserver:
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const Message& message) override {
    messages_.push_back(message);
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  std::vector<Message> messages_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

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

  void SetUpTestDir(extensions::TestExtensionDir& test_dir,
                    const std::string& background_js) {
    test_dir.WriteManifest(base::StringPrintf(
        R"({
          "name": "Test Extension",
          "version": "0.1",
          "key": "%s",
          "manifest_version": 3,
          "background": {
            "service_worker": "background.js"
          }
        })",
        kTestExtensionKey));
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_js);
  }

  const Extension* LoadExtensionAndRunTest(const std::string& background_js) {
    extensions::TestExtensionDir test_dir;
    SetUpTestDir(test_dir, background_js);

    // Launch the test app.
    ExtensionTestMessageListener ready_listener("ready",
                                                ReplyBehavior::kWillReply);
    extensions::ResultCatcher result_catcher;
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    CHECK(extension);
    CHECK_EQ(extension->id(), kTestExtensionId);

    // TODO(crbug.com/40847683): Grant permission using requestDevice().
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
    auto* hid_pinned_notification = static_cast<HidPinnedNotification*>(
        g_browser_process->hid_system_tray_icon());

    auto* device_pinned_notification_renderer =
        static_cast<DevicePinnedNotificationRenderer*>(
            hid_pinned_notification->GetIconRendererForTesting());

    auto expected_pinned_notification_id =
        device_pinned_notification_renderer->GetNotificationId(
            browser->profile());
    auto maybe_indicator_notification =
        display_service_for_system_notification_->GetNotification(
            expected_pinned_notification_id);
    ASSERT_TRUE(maybe_indicator_notification);
    EXPECT_TRUE(maybe_indicator_notification->pinned());
    display_service_for_system_notification_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, expected_pinned_notification_id,
        /*action_index=*/0, /*reply=*/std::nullopt);
    auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetURL(), "chrome://settings/content/hidDevices");
#else
    // On non-ChromeOS platforms, as they use status icon and there isn't good
    // test infra to simulate click on the status icon button, so simulate the
    // click event by invoking ExecuteCommand of HidConnectionTracker directly.
    auto* hid_status_icon =
        static_cast<HidStatusIcon*>(g_browser_process->hid_system_tray_icon());

    auto* status_icon_renderer = static_cast<DeviceStatusIconRenderer*>(
        hid_status_icon->GetIconRendererForTesting());

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "https://support.google.com/chrome?p=webhid");

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 1, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "chrome://settings/content/hidDevices");

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 2, 0);
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

// TODO(crbug.com/41494522): Re-enable on ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_GetDevices DISABLED_GetDevices
#else
#define MAYBE_GetDevices GetDevices
#endif
IN_PROC_BROWSER_TEST_F(WebHidExtensionBrowserTest, MAYBE_GetDevices) {
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

// TODO(crbug.com/41494522): Re-enable on ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RequestDevice DISABLED_RequestDevice
#else
#define MAYBE_RequestDevice RequestDevice
#endif
IN_PROC_BROWSER_TEST_F(WebHidExtensionBrowserTest, MAYBE_RequestDevice) {
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

// Test the scenario of waking up the service worker upon device events and
// the service worker being kept alive with active device session.
// TODO(crbug.com/41493373): enable the flaky test.
#if (BUILDFLAG(IS_LINUX) && defined(LEAK_SANITIZER)) || \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped \
  DISABLED_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped
#else
#define MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped \
  DeviceConnectAndOpenDeviceWhenServiceWorkerStopped
#endif
IN_PROC_BROWSER_TEST_F(
    WebHidExtensionBrowserTest,
    MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped) {
  content::ServiceWorkerContext* context = browser()
                                               ->profile()
                                               ->GetDefaultStoragePartition()
                                               ->GetServiceWorkerContext();
  // Set up an observer for service worker events.
  TestServiceWorkerContextObserver sw_observer(context, kTestExtensionId);

  TestExtensionDir test_dir;
  constexpr char kBackgroundJs[] = R"(
    navigator.hid.onconnect = async (e) => {
      chrome.test.sendMessage("connect", async () => {
        try {
          let device = e.device;
          // Bounce device a few times to make sure nothing unexpected
          // happens.
          await device.open();
          await device.close();
          await device.open();
          await device.close();
          await device.open();
          chrome.test.notifyPass();
        } catch (e) {
          chrome.test.fail(e.name + ':' + e.message);
        }
      });
    }

    navigator.hid.ondisconnect = async (e) => {
      chrome.test.sendMessage("disconnect", async () => {
        try {
          chrome.test.notifyPass();
        } catch (e) {
          chrome.test.fail(e.name + ':' + e.message);
        }
      });
    }
  )";
  SetUpTestDir(test_dir, kBackgroundJs);

  // Launch the test app.
  ExtensionTestMessageListener connect_listener("connect",
                                                ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // TODO(crbug.com/40847683): Grant permission using requestDevice().
  // Run the test.
  SetUpPolicy(extension);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), kTestExtensionId);
  sw_observer.WaitForWorkerStart();
  sw_observer.WaitForWorkerActivated();

  // The device event is handled right after the service worker is activated.
  int64_t service_worker_version_id = sw_observer.GetServiceWorkerVersionId();
  base::SimpleTestTickClock tick_clock;
  auto device = CreateTestDeviceWithInputAndOutputReports();
  hid_manager()->AddDevice(device.Clone());
  EXPECT_TRUE(connect_listener.WaitUntilSatisfied());
  connect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and the service worker is still alive due to active device
  // session.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_TRUE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  // Since we have active HID device session at this point, click the HID system
  // tray icon and check right links are opened by the browser.
  SimulateClickOnSystemTrayIconButton(browser(), extension);

  // Remove device will close the device session, and worker will stop running
  // when it times out.
  ExtensionTestMessageListener disconnect_listener("disconnect",
                                                   ReplyBehavior::kWillReply);
  hid_manager()->RemoveDevice(device->guid);
  EXPECT_TRUE(disconnect_listener.WaitUntilSatisfied());
  disconnect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_FALSE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  sw_observer.WaitForWorkerStop();

  // Another device event wakes up the inactive worker.
  connect_listener.Reset();
  hid_manager()->AddDevice(device.Clone());
  EXPECT_TRUE(connect_listener.WaitUntilSatisfied());
  connect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and the service worker is still alive due to active device
  // session.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_TRUE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  // Since we have active HID device session at this point, click the HID system
  // tray icon and check right links are opened by the browser.
  SimulateClickOnSystemTrayIconButton(browser(), extension);
}

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_EventListenerAddedAfterServiceWorkerIsActivated \
  DISABLED_EventListenerAddedAfterServiceWorkerIsActivated
#else
#define MAYBE_EventListenerAddedAfterServiceWorkerIsActivated \
  EventListenerAddedAfterServiceWorkerIsActivated
#endif
IN_PROC_BROWSER_TEST_F(WebHidExtensionBrowserTest,
                       MAYBE_EventListenerAddedAfterServiceWorkerIsActivated) {
  const char kWarningMessage[] =
      "Event handler of '%s' event must be added on the initial evaluation "
      "of worker script. More info: "
      "https://developer.chrome.com/docs/extensions/mv3/service_workers/"
      "events/";

  content::ServiceWorkerContext* context = browser()
                                               ->profile()
                                               ->GetDefaultStoragePartition()
                                               ->GetServiceWorkerContext();
  // Set up an observer for service worker events.
  TestServiceWorkerContextObserver sw_observer(context, kTestExtensionId);
  // Set up an observer for console messages reported by service worker
  TestServiceWorkerConsoleObserver console_observer(browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveWebContents()
                                                        ->GetBrowserContext());
  TestExtensionDir test_dir;
  constexpr char kBackgroundJs[] = R"(
      chrome.test.sendMessage("ready", function() {
        navigator.hid.addEventListener("connect", () => {});
      });
    )";
  SetUpTestDir(test_dir, kBackgroundJs);

  // Launch the test app.
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // TODO(crbug.com/40847683): Grant permission using requestDevice().
  // Run the test.
  SetUpPolicy(extension);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), kTestExtensionId);
  sw_observer.WaitForWorkerStart();
  sw_observer.WaitForWorkerActivated();

  auto device = CreateTestDeviceWithInputAndOutputReports();
  hid_manager()->AddDevice(device.Clone());

  // Warning message will be displayed when event listener is nested inside a
  // function
  console_observer.WaitForMessages();
  EXPECT_EQ(console_observer.messages().size(), 1u);
  EXPECT_EQ(console_observer.messages().begin()->message_level,
            blink::mojom::ConsoleMessageLevel::kWarning);
  EXPECT_EQ(console_observer.messages().begin()->message,
            base::UTF8ToUTF16(base::StringPrintf(kWarningMessage, "connect")));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
