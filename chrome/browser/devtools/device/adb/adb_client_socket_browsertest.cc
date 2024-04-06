// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
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
  void StartTest(base::RunLoop* loop) {
    Profile* profile = browser()->profile();
    android_bridge_ = DevToolsAndroidBridge::Factory::GetForProfile(profile);
    AndroidDeviceManager::DeviceProviders device_providers;
    device_providers.push_back(new AdbDeviceProvider());
    android_bridge_->set_device_providers_for_test(device_providers);
    android_bridge_->AddDeviceListListener(this);
    loop_ = loop;
    loop_->Run();
  }

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    devices_ = devices;
    android_bridge_->RemoveDeviceListListener(this);
    loop_->QuitWhenIdle();
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
    ASSERT_EQ(5U, browsers.size());

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

    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> node =
        FindBrowserByDisplayName(browsers, "Node");
    ASSERT_TRUE(node.get());

    ASSERT_EQ("32.0.1679.0", chrome->version());
    ASSERT_EQ("31.0.1599.0", chrome_beta->version());
    ASSERT_EQ("4.0", webview->version());
    ASSERT_EQ("v10.15.3", node->version());

    ASSERT_EQ("Test User", chrome->user());
    ASSERT_EQ("Test User : 2", chrome_beta->user());
    ASSERT_EQ("Test User", webview->user());

    DevToolsAndroidBridge::RemotePages chrome_pages =
        chrome->pages();
    DevToolsAndroidBridge::RemotePages chrome_beta_pages =
        chrome_beta->pages();
    DevToolsAndroidBridge::RemotePages webview_pages =
        webview->pages();
    DevToolsAndroidBridge::RemotePages node_pages = node->pages();

    ASSERT_EQ(1U, chrome_pages.size());
    ASSERT_EQ(1U, chrome_beta_pages.size());
    ASSERT_EQ(2U, webview_pages.size());
    ASSERT_EQ(1U, node_pages.size());

    scoped_refptr<content::DevToolsAgentHost> chrome_target(
        chrome_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> chrome_beta_target(
        chrome_beta_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> webview_target_0(
        webview_pages[0]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> webview_target_1(
        webview_pages[1]->CreateTarget());
    scoped_refptr<content::DevToolsAgentHost> node_target(
        node_pages[0]->CreateTarget());

    // Check that we have non-empty description for webview pages.
    ASSERT_EQ(0U, chrome_target->GetDescription().size());
    ASSERT_EQ(0U, chrome_beta_target->GetDescription().size());
    ASSERT_NE(0U, webview_target_0->GetDescription().size());
    ASSERT_NE(0U, webview_target_1->GetDescription().size());

    ASSERT_EQ(GURL("http://www.chromium.org/"),
                   chrome_target->GetURL());
    ASSERT_EQ("The Chromium Projects",
              chrome_target->GetTitle());
    ASSERT_EQ("node", node_target->GetType());
  }

 private:
  raw_ptr<DevToolsAndroidBridge, DanglingUntriaged> android_bridge_;
  DevToolsAndroidBridge::RemoteDevices devices_;
  // base::RunLoop used to require kNestableTaskAllowed
  raw_ptr<base::RunLoop> loop_;
};

// Combine all tests into one. Splitting up into multiple tests can be flaky
// due to failure to bind a hardcoded port. crbug.com/566057
// The tests seems to be stable on Windows bots only:
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestCombined TestCombined
#else
#define MAYBE_TestCombined DISABLED_TestCombined
#endif
IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, MAYBE_TestCombined) {
  base::RunLoop loop1, loop2, loop3;
  StartMockAdbServer(FlushWithoutSize);
  StartTest(&loop1);
  CheckDevices();
  StopMockAdbServer();

  StartMockAdbServer(FlushWithSize);
  StartTest(&loop2);
  CheckDevices();
  StopMockAdbServer();

  StartMockAdbServer(FlushWithData);
  StartTest(&loop3);
  CheckDevices();
  StopMockAdbServer();
}
