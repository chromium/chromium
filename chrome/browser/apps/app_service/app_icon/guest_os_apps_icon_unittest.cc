// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
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
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace apps {

namespace {

constexpr int kTestIconSize = 64;

}  // namespace

class AppServiceGuestOSIconTest : public testing::Test {
 public:
  void SetUp() override {
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
  }

  void TearDown() override {
    crostini_test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  apps::IconValuePtr LoadIcon(const std::string& app_id,
                              int size_dp,
                              IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    proxy().LoadIcon(app_id, icon_type, size_dp,
                     /*allow_placeholder_icon=*/false, result.GetCallback());
    return result.Take();
  }

  // Registers a test app in Crostini and returns the app service App Id.
  std::string AddApp(const std::string& desktop_file_id) {
    vm_tools::apps::App app;
    app.set_desktop_file_id(desktop_file_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value("Test app");
    crostini_test_helper()->AddApp(app);

    return crostini::CrostiniTestHelper::GenerateAppId(
        desktop_file_id, crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName);
  }

  // Manually generates an icon made up of a `solid_color` with applied
  // `effects`, without going through any publisher icon loading code.
  IconValuePtr GenerateIcon(std::optional<std::string> app_id,
                            SkColor solid_color,
                            int size_dp,
                            IconEffects effects) {
    gfx::ImageSkia image = CreateSquareIconImageSkia(size_dp, solid_color);
    auto iv = std::make_unique<apps::IconValue>();
    iv->icon_type = apps::IconType::kUncompressed;
    iv->uncompressed = image;

    base::test::TestFuture<IconValuePtr> image_with_effects;
    ApplyIconEffects(profile(), app_id, effects, size_dp, std::move(iv),
                     image_with_effects.GetCallback());

    IconValuePtr result = image_with_effects.Take();
    EnsureRepresentationsLoaded(result->uncompressed);
    return result;
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
  raw_ptr<ash::FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;
  std::unique_ptr<TestingProfile> profile_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  raw_ptr<AppServiceProxy, DanglingUntriaged> proxy_;
  std::unique_ptr<crostini::CrostiniTestHelper> crostini_test_helper_;
};

// Verify loading a Crostini app icon by retrieving data from the VM.
TEST_F(AppServiceGuestOSIconTest, GetStandardCrostiniIconFromVM) {
  constexpr char kDesktopFileId[] = "desktop_file_id";
  std::string app_id = AddApp(kDesktopFileId);

  // The VM can return an image of any size, it will be resized by App Service.
  constexpr int kVmIconSizePx = 150;
  SkBitmap red_bitmap = gfx::test::CreateBitmap(kVmIconSizePx, SK_ColorRED);
  std::vector<uint8_t> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(red_bitmap, false, &png_bytes);

  vm_tools::cicerone::ContainerAppIconResponse response;
  auto* icon_response = response.add_icons();
  icon_response->set_icon(&png_bytes[0], png_bytes.size());
  icon_response->set_desktop_file_id(kDesktopFileId);
  icon_response->set_format(vm_tools::cicerone::DesktopIcon::PNG);
  fake_cicerone_client()->set_container_app_icon_response(response);

  IconValuePtr iv = LoadIcon(app_id, kTestIconSize, IconType::kStandard);
  ASSERT_EQ(iv->icon_type, IconType::kStandard);

  IconValuePtr expected = GenerateIcon(app_id, SK_ColorRED, kTestIconSize,
                                       IconEffects::kCrOsStandardIcon);
  VerifyIcon(iv->uncompressed, expected->uncompressed);
}

TEST_F(AppServiceGuestOSIconTest, GetStandardCrostiniMultiContainerIconFromVM) {
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_multi_container_allowed(true);

  constexpr char kDesktopFileId[] = "desktop_file_id";
  std::string app_id = AddApp(kDesktopFileId);

  constexpr int kVmIconSizePx = 150;
  SkBitmap red_bitmap = gfx::test::CreateBitmap(kVmIconSizePx, SK_ColorRED);
  std::vector<uint8_t> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(red_bitmap, false, &png_bytes);

  vm_tools::cicerone::ContainerAppIconResponse response;
  auto* icon_response = response.add_icons();
  icon_response->set_icon(&png_bytes[0], png_bytes.size());
  icon_response->set_desktop_file_id(kDesktopFileId);
  icon_response->set_format(vm_tools::cicerone::DesktopIcon::PNG);
  fake_cicerone_client()->set_container_app_icon_response(response);

  IconValuePtr iv = LoadIcon(app_id, kTestIconSize, IconType::kStandard);
  ASSERT_EQ(iv->icon_type, IconType::kStandard);

  IconValuePtr expected =
      GenerateIcon(app_id, SK_ColorRED, kTestIconSize,
                   IconEffects::kCrOsStandardIcon | IconEffects::kGuestOsBadge);

  VerifyIcon(iv->uncompressed, expected->uncompressed);
}

// Verify loading a Crostini app icon by falling back to data in GuestOS's disk
// cache.
TEST_F(AppServiceGuestOSIconTest, GetStandardCrostiniIconFromDisk) {
  constexpr char kDesktopFileId[] = "desktop_file_id";
  std::string app_id = AddApp(kDesktopFileId);

  constexpr int kVmIconSizePx = 256;
  SkBitmap red_bitmap = gfx::test::CreateBitmap(kVmIconSizePx, SK_ColorGREEN);
  std::vector<uint8_t> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(red_bitmap, false, &png_bytes);

  vm_tools::cicerone::ContainerAppIconResponse response;
  auto* icon_response = response.add_icons();
  icon_response->set_icon(&png_bytes[0], png_bytes.size());
  icon_response->set_desktop_file_id(kDesktopFileId);
  icon_response->set_format(vm_tools::cicerone::DesktopIcon::PNG);
  fake_cicerone_client()->set_container_app_icon_response(response);

  // Load an icon once to populate the GuestOS disk cache.
  IconValuePtr iv1 = LoadIcon(app_id, kTestIconSize, IconType::kStandard);

  // Prevent further responses from Cicerone, simulating the VM being shut down.
  fake_cicerone_client()->set_container_app_icon_response(
      vm_tools::cicerone::ContainerAppIconResponse::default_instance());

  // Loading an icon with a different size should read data from the GuestOS
  // disk cache.
  IconValuePtr iv2 = LoadIcon(app_id, kTestIconSize * 2, IconType::kStandard);
  ASSERT_EQ(iv2->icon_type, IconType::kStandard);

  IconValuePtr expected = GenerateIcon(app_id, SK_ColorGREEN, kTestIconSize * 2,
                                       IconEffects::kCrOsStandardIcon);
  VerifyIcon(iv2->uncompressed, expected->uncompressed);
}

TEST_F(AppServiceGuestOSIconTest, GetCrostiniIconWithInvalidData) {
  constexpr char kDesktopFileId[] = "desktop_file_id";
  std::string app_id = AddApp(kDesktopFileId);

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

  apps::IconValuePtr iv =
      LoadIcon(app_id, kTestIconSize, IconType::kUncompressed);
  ASSERT_EQ(iv->icon_type, IconType::kUncompressed);
  VerifyIcon(expected_image, iv->uncompressed);
}

}  // namespace apps
