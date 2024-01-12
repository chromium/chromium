// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class CrostiniAppsTest : public testing::Test {
 public:
  CrostiniAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  crostini::CrostiniTestHelper* test_helper() { return test_helper_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  TestingProfile* profile() { return profile_.get(); }

  void SetUp() override {
    ash::CiceroneClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());
    test_helper()->ReInitializeAppServiceIntegration();
  }

  void TearDown() override {
    test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
  }

  // Adds a test Crostini app and returns its app_id.
  std::string AddCrostiniApp(std::string desktop_file_id,
                             std::string app_name) {
    vm_tools::apps::App app;
    app.set_desktop_file_id(desktop_file_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_name);
    test_helper()->AddApp(app);
    return crostini::CrostiniTestHelper::GenerateAppId(
        app.desktop_file_id(), crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;
  raw_ptr<AppServiceProxy, DanglingUntriaged> app_service_proxy_ = nullptr;
};

TEST_F(CrostiniAppsTest, AppReadinessUpdatesWhenCrostiniDisabled) {
  // Install a Crostini app.
  const std::string app_id = AddCrostiniApp("desktop_file_id", "app_name");

  // Check that the app is ready.
  apps::Readiness readiness_before = Readiness::kUnknown;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [&readiness_before](const AppUpdate& update) {
        readiness_before = update.Readiness();
      });
  ASSERT_EQ(readiness_before, Readiness::kReady);

  // Disable Crostini. This call uninstalls all Crostini apps.
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
      ->ClearApplicationList(guest_os::VmType::TERMINA,
                             crostini::kCrostiniDefaultVmName, "");

  // Check that the app is now disabled.
  apps::Readiness readiness_after;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [&readiness_after](const AppUpdate& update) {
        readiness_after = update.Readiness();
      });
  ASSERT_EQ(readiness_after, Readiness::kUninstalledByUser);
}

}  // namespace apps
