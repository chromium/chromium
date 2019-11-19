// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;

static scoped_refptr<DevToolsAndroidBridge::RemoteBrowser>
FindBrowserByDisplayName(DevToolsAndroidBridge::RemoteBrowsers browsers,
                         const std::string& name) {
  for (auto it = browsers.begin(); it != browsers.end(); ++it)
    if ((*it)->display_name() == name)
      return *it;
  return nullptr;
}

class AdbClientSocketTest : public InProcessBrowserTest,
                            public DevToolsAndroidBridge::DeviceListListener {

 public:
  void StartTest() {
    Profile* profile = browser()->profile();
    android_bridge_ = DevToolsAndroidBridge::Factory::GetForProfile(profile);
    AndroidDeviceManager::DeviceProviders device_providers;
    device_providers.push_back(new AdbDeviceProvider());
    android_bridge_->set_device_providers_for_test(device_providers);
    android_bridge_->AddDeviceListListener(this);
    content::RunMessageLoop();
  }

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    devices_ = devices;
    android_bridge_->RemoveDeviceListListener(this);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  void CheckDevices() {
    ASSERT_EQ(2U, devices_.size());

    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> connected =
        devices_[0]->is_connected() ? devices_[0] : devices_[1];

    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> not_connected =
        devices_[0]->is_connected() ? devices_[1] : devices_[0];

    ASSERT_TRUE(connected->is_connected());
    ASSERT_FALSE(not_connected->is_connected());

    ASSERT_EQ(720, connected->screen_size().width());
    ASSERT_EQ(1184, connected->screen_size().height());

    ASSERT_EQ("01498B321301A00A", connected->serial());
    ASSERT_EQ("Nexus 6", connected->model());

    ASSERT_EQ("01498B2B0D01300E", not_connected->serial());
    ASSERT_EQ("Offline", not_connected->model());

    const DevToolsAndroidBridge::RemoteBrowsers& browsers =
        connected->browsers();
    ASSERT_EQ(4U, browsers.size());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chrome =
        FindBrowserByDisplayName(browsers, "Chrome");
    ASSERT_TRUE(chrome.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chrome_beta =
        FindBrowserByDisplayName(browsers, "Chrome Beta");
    ASSERT_TRUE(chrome_beta.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> chromium =
        FindBrowserByDisplayName(browsers, "Chromium");
    ASSERT_FALSE(chromium.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> webview =
        FindBrowserByDisplayName(browsers, "WebView in com.sample.feed");
    ASSERT_TRUE(webview.get());

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> noprocess =
        FindBrowserByDisplayName(browsers, "Noprocess");
    ASSERT_TRUE(noprocess.get());

    ASSERT_EQ("32.0.1679.0", chrome->version());
    ASSERT_EQ("31.0.1599.0", chrome_beta->version());
    ASSERT_EQ("4.0", webview->version());

    ASSERT_EQ("Test User", chrome->user());
    ASSERT_EQ("Test User : 2", chrome_beta->user());
    ASSERT_EQ("Test User", webview->user());

    DevToolsAndroidBridge::RemotePages chrome_pages =
        chrome->pages();
    DevToolsAndroidBridge::RemotePages chrome_beta_pages =
        chrome_beta->pages();
    DevToolsAndroidBridge::RemotePages webview_pages =
        webview->pages();

    ASSERT_EQ(1U, chrome_pages.size());
    ASSERT_EQ(1U, chrome_beta_pages.size());
    ASSERT_EQ(2U, webview_pages.size());

    scoped_refptr<content::DevToolsAgentHost> chrome_target(
        chrome_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> chrome_beta_target(
        chrome_beta_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> webview_target_0(
        webview_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> webview_target_1(
        webview_pages[1]->CreateTarget());

    // Check that we have non-empty description for webview pages.
    ASSERT_EQ(0U, chrome_target->GetDescription().size());
    ASSERT_EQ(0U, chrome_beta_target->GetDescription().size());
    ASSERT_NE(0U, webview_target_0->GetDescription().size());
    ASSERT_NE(0U, webview_target_1->GetDescription().size());

    ASSERT_EQ(GURL("http://www.chromium.org/"),
                   chrome_target->GetURL());
    ASSERT_EQ("The Chromium Projects",
              chrome_target->GetTitle());
  }

 private:
  DevToolsAndroidBridge* android_bridge_;
  DevToolsAndroidBridge::RemoteDevices devices_;
};

// Flaky due to failure to bind a hardcoded port. crbug.com/566057
IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, DISABLED_TestFlushWithoutSize) {
  StartMockAdbServer(FlushWithoutSize);
  StartTest();
  CheckDevices();
  StopMockAdbServer();
}

// Flaky due to failure to bind a hardcoded port. crbug.com/566057
IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, DISABLED_TestFlushWithSize) {
  StartMockAdbServer(FlushWithSize);
  StartTest();
  CheckDevices();
  StopMockAdbServer();
}

// Flaky due to failure to bind a hardcoded port. crbug.com/566057
IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, DISABLED_TestFlushWithData) {
  StartMockAdbServer(FlushWithData);
  StartTest();
  CheckDevices();
  StopMockAdbServer();
}
