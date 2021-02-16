// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_mode/app_session.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::chromeos::FakePowerManagerClient;

constexpr char kPepperPluginName1[] = "pepper_plugin_name1";
constexpr char kPepperPluginName2[] = "pepper_plugin_name2";
constexpr char kBrowserPluginName[] = "browser_plugin_name";
constexpr char kPepperPluginFilePath1[] = "/path/to/pepper_plugin1";
constexpr char kPepperPluginFilePath2[] = "/path/to/pepper_plugin2";
constexpr char kBrowserPluginFilePath[] = "/path/to/browser_plugin";
constexpr char kUnregisteredPluginFilePath[] = "/path/to/unregistered_plugin";

}  // namespace

class AppSessionTest : public testing::Test {
 public:
  AppSessionTest() = default;
  AppSessionTest(const AppSessionTest&) = delete;
  AppSessionTest& operator=(const AppSessionTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppSessionTest, WebKioskTracksBrowserCreation) {
  auto app_session = std::make_unique<AppSession>();
  TestingProfile profile;

  Browser::CreateParams params(&profile, true);
  auto app_browser = CreateBrowserWithTestWindowForParams(params);

  app_session->InitForWebKiosk(app_browser.get());

  Browser::CreateParams another_params(&profile, true);
  auto another_browser = CreateBrowserWithTestWindowForParams(another_params);

  base::RunLoop loop;
  static_cast<TestBrowserWindow*>(another_browser->window())
      ->SetCloseCallback(
          base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
  loop.Run();

  bool chrome_closed = false;
  app_session->SetAttemptUserExitForTesting(
      base::BindLambdaForTesting([&chrome_closed]() { chrome_closed = true; }));

  app_browser.reset();
  ASSERT_TRUE(chrome_closed);
}

TEST_F(AppSessionTest, ShouldHandlePlugin) {
  // Create an out-of-process pepper plugin.
  content::WebPluginInfo info1;
  info1.name = base::ASCIIToUTF16(kPepperPluginName1);
  info1.path = base::FilePath(kPepperPluginFilePath1);
  info1.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;

  // Create an in-of-process pepper plugin.
  content::WebPluginInfo info2;
  info2.name = base::ASCIIToUTF16(kPepperPluginName2);
  info2.path = base::FilePath(kPepperPluginFilePath2);
  info2.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  // Create an in-of-process browser (non-pepper) plugin.
  content::WebPluginInfo info3;
  info3.name = base::ASCIIToUTF16(kBrowserPluginName);
  info3.path = base::FilePath(kBrowserPluginFilePath);
  info3.type = content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;

  // Register two pepper plugins.
  content::PluginService* service = content::PluginService::GetInstance();
  service->RegisterInternalPlugin(info1, true);
  service->RegisterInternalPlugin(info2, true);
  service->RegisterInternalPlugin(info3, true);
  service->Init();
  service->RefreshPlugins();

  // Force plugins to load and wait for completion.
  base::RunLoop run_loop;
  service->GetPlugins(base::BindOnce(
      [](base::OnceClosure callback,
         const std::vector<content::WebPluginInfo>& ignore) {
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();

  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

  // The app session should handle two pepper plugins.
  EXPECT_TRUE(
      delegate->ShouldHandlePlugin(base::FilePath(kPepperPluginFilePath1)));
  EXPECT_TRUE(
      delegate->ShouldHandlePlugin(base::FilePath(kPepperPluginFilePath2)));

  // The app session should not handle the browser plugin.
  EXPECT_FALSE(
      delegate->ShouldHandlePlugin(base::FilePath(kBrowserPluginFilePath)));

  // The app session should not handle the unregistered plugin.
  EXPECT_FALSE(delegate->ShouldHandlePlugin(
      base::FilePath(kUnregisteredPluginFilePath)));
}

TEST_F(AppSessionTest, OnPluginCrashed) {
  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

  // Create a fake power manager client.
  FakePowerManagerClient client;

  // Verified the number of restart calls.
  EXPECT_EQ(client.num_request_restart_calls(), 0);
  delegate->OnPluginCrashed(base::FilePath(kBrowserPluginFilePath));
  EXPECT_EQ(client.num_request_restart_calls(), 1);
}

TEST_F(AppSessionTest, OnPluginHung) {
  // Create an app session.
  std::unique_ptr<AppSession> app_session = std::make_unique<AppSession>();
  KioskSessionPluginHandlerDelegate* delegate = app_session.get();

  // Create a fake power manager client.
  FakePowerManagerClient::InitializeFake();

  // Only verify if this method can be called without error.
  delegate->OnPluginHung(std::set<int>());
}

}  // namespace ash
