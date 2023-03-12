// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

constexpr int kTestIconSize = 64;

class AppServiceGuestOSIconTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        apps::kUnifiedAppServiceIconLoading);

    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    ash::ChunneldClient::InitializeFake();
    fake_cicerone_client_ = ash::FakeCiceroneClient::Get();

    profile_ = std::make_unique<TestingProfile>();

    proxy_ = AppServiceProxyFactory::GetForProfile(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    crostini_test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile());
    crostini_test_helper()->ReInitializeAppServiceIntegration();

    scoped_decode_request_for_testing_ =
        std::make_unique<ScopedDecodeRequestForTesting>();
  }

  void TearDown() override {
    crostini_test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  apps::IconValuePtr LoadIcon(const std::string& app_id, IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    proxy().LoadIcon(AppType::kCrostini, app_id, icon_type, kTestIconSize,
                     /*allow_placeholder_icon=*/false, result.GetCallback());
    return result.Take();
  }

  TestingProfile* profile() { return profile_.get(); }
  AppServiceProxy& proxy() { return *proxy_; }
  ash::FakeCiceroneClient* fake_cicerone_client() {
    return fake_cicerone_client_;
  }
  crostini::CrostiniTestHelper* crostini_test_helper() {
    return crostini_test_helper_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::raw_ptr<ash::FakeCiceroneClient> fake_cicerone_client_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ScopedDecodeRequestForTesting>
      scoped_decode_request_for_testing_;
  base::raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<crostini::CrostiniTestHelper> crostini_test_helper_;
};

TEST_F(AppServiceGuestOSIconTest, GetCrostiniIconWithInvalidData) {
  constexpr char kDesktopFileId[] = "desktop_file_id";

  vm_tools::apps::App app;
  app.set_desktop_file_id(kDesktopFileId);
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_locale(std::string());
  entry->set_value("Test app");
  crostini_test_helper()->AddApp(app);

  std::string app_id = crostini::CrostiniTestHelper::GenerateAppId(
      kDesktopFileId, crostini::kCrostiniDefaultVmName,
      crostini::kCrostiniDefaultContainerName);

  // When loading an icon from the VM, return an invalid PNG.
  vm_tools::cicerone::ContainerAppIconResponse response;
  auto* icon_response = response.add_icons();
  icon_response->set_icon("this string is not a valid png :)");
  icon_response->set_desktop_file_id(kDesktopFileId);
  icon_response->set_format(vm_tools::cicerone::DesktopIcon::PNG);
  fake_cicerone_client()->set_container_app_icon_response(response);

  // Since decoding the PNG data will fail, the result should be the default
  // Crostini icon.
  gfx::ImageSkia expected_image;
  LoadDefaultIcon(expected_image, IDR_LOGO_CROSTINI_DEFAULT);

  apps::IconValuePtr iv = LoadIcon(app_id, IconType::kUncompressed);
  ASSERT_EQ(iv->icon_type, IconType::kUncompressed);
  VerifyIcon(expected_image, iv->uncompressed);
}

}  // namespace apps
