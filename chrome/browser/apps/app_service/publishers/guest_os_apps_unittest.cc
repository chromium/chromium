// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"

#include <memory>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

// An app publisher (in the App Service sense) that inherits GuestOSApps and
// implements the necessary virtual functions.
class TestPublisher : public GuestOSApps {
 public:
  explicit TestPublisher(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

 private:
  bool CouldBeAllowed() const override { return true; }

  apps::AppType AppType() const override { return AppType::kBruschetta; }
  guest_os::VmType VmType() const override {
    return guest_os::VmType::BRUSCHETTA;
  }

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const IconKey& icon_key,
                IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override {}
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override {}
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override {}

  void CreateAppOverrides(
      const guest_os::GuestOsRegistryService::Registration& registration,
      App* app) override {
    app->name = "override_name";
  }
};

class GuestOSAppsTest : public testing::Test {
 public:
  GuestOSAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  guest_os::GuestOsRegistryService* registry() { return registry_.get(); }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    registry_ =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override { profile_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::raw_ptr<AppServiceProxy> app_service_proxy_ = nullptr;
  base::raw_ptr<guest_os::GuestOsRegistryService> registry_ = nullptr;
};

TEST_F(GuestOSAppsTest, CreateApp) {
  // Create the test publisher and register it.
  auto pub = std::make_unique<TestPublisher>(app_service_proxy());
  pub->InitializeForTesting();

  // Create a test app.
  vm_tools::apps::App app;
  app.add_mime_types("text/plain");
  app.set_desktop_file_id("app_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_value("app_name");

  // Update the ApplicationList, this calls through GuestOSApps::CreateApp.
  vm_tools::apps::ApplicationList app_list;
  app_list.set_vm_name(bruschetta::kBruschettaVmName);
  app_list.set_container_name("test_container");
  *app_list.add_apps() = app;
  registry()->UpdateApplicationList(app_list);

  // Get the AppUpdate from the registry and check its contents.
  std::string id = registry()->GenerateAppId(
      "app_id", bruschetta::kBruschettaVmName, "test_container");
  bool seen = false;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      id, [&seen](const AppUpdate& update) {
        seen = true;
        EXPECT_EQ(update.AppType(), AppType::kBruschetta);
        EXPECT_EQ(update.Readiness(), Readiness::kReady);
        EXPECT_EQ(update.Name(), "override_name")
            << "CreateAppOverrides should change the name.";
        EXPECT_EQ(update.InstallReason(), InstallReason::kUser);
        EXPECT_EQ(update.InstallSource(), InstallSource::kUnknown);
        EXPECT_TRUE(update.IconKey().has_value());
        EXPECT_TRUE(update.ShowInLauncher());
        EXPECT_TRUE(update.ShowInSearch());
        EXPECT_TRUE(update.ShowInShelf());
      });
  EXPECT_TRUE(seen) << "Couldn't find test app in registry.";
}

}  // namespace apps
