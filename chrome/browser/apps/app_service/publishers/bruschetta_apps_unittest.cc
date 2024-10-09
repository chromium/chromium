// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/bruschetta_apps.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/bruschetta/fake_bruschetta_launcher.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace apps {

class BruschettaAppsTest : public testing::Test,
                           public guest_os::FakeVmServicesHelper {
 public:
  BruschettaAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  Profile* profile() { return profile_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  guest_os::GuestOsRegistryService* registry() { return registry_.get(); }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    registry_ =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_.get());
    const guest_os::GuestId id(bruschetta::kBruschettaVmName, "test_container");
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_.get())
        ->AddGuestForTesting(id);
  }

  void TearDown() override { profile_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AppServiceProxy, DanglingUntriaged> app_service_proxy_ = nullptr;
  raw_ptr<guest_os::GuestOsRegistryService, DanglingUntriaged> registry_ =
      nullptr;
};

class BruschettaAppsTestHelper {
 public:
  static void Launch(BruschettaApps* pub,
                     const std::string& app_id,
                     int32_t event_flags,
                     LaunchSource launch_source,
                     WindowInfoPtr window_info) {
    pub->Launch(app_id, event_flags, launch_source, std::move(window_info));
  }
};

TEST_F(BruschettaAppsTest, LaunchApp) {
  // Create the publisher and register it.
  auto pub = std::make_unique<BruschettaApps>(app_service_proxy());
  pub->InitializeForTesting();

  // Create a test app.
  vm_tools::apps::App app;
  app.add_mime_types("text/plain");
  app.set_desktop_file_id("desktop_file_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_value("app_name");

  // Add it to the ApplicationList.
  vm_tools::apps::ApplicationList app_list;
  app_list.set_vm_name(bruschetta::kBruschettaVmName);
  app_list.set_container_name("test_container");
  *app_list.add_apps() = app;
  registry()->UpdateApplicationList(app_list);

  std::string id = registry()->GenerateAppId(
      "desktop_file_id", bruschetta::kBruschettaVmName, "test_container");

  // Set up FakeBruschettaLauncher to pretend to launch the VM.
  bruschetta::BruschettaServiceFactory::GetForProfile(profile())
      ->SetLauncherForTesting(
          bruschetta::kBruschettaVmName,
          std::make_unique<bruschetta::FakeBruschettaLauncher>());

  // Get ready to catch the launch.
  bool ran = false;
  FakeCiceroneClient()->SetOnLaunchContainerApplicationCallback(
      base::BindLambdaForTesting(
          [&](const vm_tools::cicerone::LaunchContainerApplicationRequest&
                  request,
              chromeos::DBusMethodCallback<
                  vm_tools::cicerone::LaunchContainerApplicationResponse>
                  callback) {
            ran = true;
            EXPECT_EQ(request.desktop_file_id(), "desktop_file_id");
            vm_tools::cicerone::LaunchContainerApplicationResponse response;
            response.set_success(true);
            std::move(callback).Run(response);
          }));

  // Launch the app.
  const int32_t event_flags = ui::EF_LEFT_MOUSE_BUTTON;
  const LaunchSource launch_source = LaunchSource::kFromAppListGrid;
  WindowInfoPtr window_info;
  BruschettaAppsTestHelper::Launch(pub.get(), id, event_flags, launch_source,
                                   std::move(window_info));

  EXPECT_TRUE(ran) << "Expected FakeCiceroneClient's "
                      "OnLaunchContainerApplicationCallback to run.";
}

}  // namespace apps
