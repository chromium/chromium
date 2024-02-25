// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_icon/web_app_icon_test_helper.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using IconPurpose = web_app::IconPurpose;

class FakeShortcutPublisherForIconTest : public apps::ShortcutPublisher {
 public:
  FakeShortcutPublisherForIconTest(apps::AppServiceProxy* proxy,
                                   apps::AppType app_type)
      : ShortcutPublisher(proxy) {
    RegisterShortcutPublisher(app_type);
  }

  ~FakeShortcutPublisherForIconTest() override = default;

  void LaunchShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      int64_t display_id) override {}

  void RemoveShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      UninstallSource uninstall_source) override {}

  void GetCompressedIconData(const std::string& shortcut_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback) override {
    std::string local_id = proxy()->ShortcutRegistryCache()->GetShortcutLocalId(
        apps::ShortcutId(shortcut_id));
    apps::GetWebAppCompressedIconData(proxy()->profile(), local_id, size_in_dip,
                                      scale_factor, std::move(callback));
    num_get_icon_call_++;
  }
  int num_get_icon_call() { return num_get_icon_call_; }

 private:
  int num_get_icon_call_ = 0;
};

class BrowserShortcutIconTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppShortcutUiUpdate);
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    proxy_ = AppServiceProxyFactory::GetForProfile(profile());
    fake_shortcut_publisher_ =
        std::make_unique<apps::FakeShortcutPublisherForIconTest>(
            proxy_, AppType::kChromeApp);

    std::vector<apps::AppPtr> app_deltas;
    app_deltas.push_back(apps::AppPublisher::MakeApp(
        AppType::kChromeApp, app_constants::kChromeAppId,
        apps::Readiness::kReady, "Some App Name", apps::InstallReason::kUser,
        apps::InstallSource::kSystem));
    app_service_proxy()->OnApps(std::move(app_deltas), AppType::kChromeApp,
                                /* should_notify_initialized */ true);
  }

  AppServiceProxy* app_service_proxy() { return proxy_; }

  apps::ShortcutId RegisterShortcut(std::unique_ptr<web_app::WebApp> web_app) {
    ShortcutPtr shortcut = std::make_unique<Shortcut>(
        app_constants::kChromeAppId, web_app->app_id());
    shortcut->icon_key = IconKey();
    test_helper().RegisterApp(std::move(web_app));
    apps::ShortcutId shortcut_id = shortcut->shortcut_id;
    app_service_proxy()->PublishShortcut(std::move(shortcut));
    return shortcut_id;
  }

  apps::IconValuePtr LoadShortcutIcon(const apps::ShortcutId& shortcut_id,
                                      IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy()->LoadShortcutIcon(shortcut_id, icon_type, kSizeInDip,
                                          /*allow_placeholder_icon=*/false,
                                          result.GetCallback());
    return result.Take();
  }

  FakeShortcutPublisherForIconTest* fake_shortcut_publisher() {
    return fake_shortcut_publisher_.get();
  }
  web_app::WebAppIconManager& icon_manager() {
    return web_app::WebAppProvider::GetForWebApps(profile())->icon_manager();
  }

  Profile* profile() { return profile_.get(); }

  WebAppIconTestHelper test_helper() { return WebAppIconTestHelper(profile()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<web_app::WebAppIconManager> icon_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<apps::FakeShortcutPublisherForIconTest>
      fake_shortcut_publisher_;
};

TEST_F(BrowserShortcutIconTest, GetCompressedIconData) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string web_app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  apps::ShortcutId shortcut_id = RegisterShortcut(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(web_app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      web_app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);
  EXPECT_EQ(0, fake_shortcut_publisher()->num_get_icon_call());
  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects.
  VerifyCompressedIcon(src_data,
                       *LoadShortcutIcon(shortcut_id, IconType::kCompressed));

  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());

  // Verify loading the same icon again doesn't install the icon from publisher
  // again.
  VerifyCompressedIcon(src_data,
                       *LoadShortcutIcon(shortcut_id, IconType::kCompressed));
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
}

TEST_F(BrowserShortcutIconTest, GetStandardIconData) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string web_app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  apps::ShortcutId shortcut_id = RegisterShortcut(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(web_app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      web_app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
      /*skip_icon_effects=*/true);

  EXPECT_EQ(0, fake_shortcut_publisher()->num_get_icon_call());
  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  VerifyIcon(src_image_skia, iv->uncompressed);

  // Verify loading the same icon again doesn't install the icon from publisher
  // again.
  apps::IconValuePtr iv2 = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
}

TEST_F(BrowserShortcutIconTest, GetIconDataWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string web_app_id = web_app->app_id();

  const float scale = 1.0;
  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  apps::ShortcutId shortcut_id = RegisterShortcut(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(web_app_id, IconPurpose::ANY, sizes_px));

  // Create the web app compressed icon data for the size in dip 64.
  // 1. The icon file of size 96px will be resized to 64 to generated the
  // ImageSkiaRep for the scale 1.0.
  // 2. The icon file of size 256px will be resized to 128 to generated the
  // ImageSkiaRep for the scale 2.0.
  //
  // The the ImageSkiaRep(scale=1.0) is encoded to generate the compressed icon
  // data `src_data`.
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      web_app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale);

  EXPECT_EQ(0, fake_shortcut_publisher()->num_get_icon_call());
  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects. LoadIconFromIconKey can generate the
  // ImageSkia(size_in_dip=64) with icon files(96px and 256px) after resizing
  // them, and encode the ImageSkiaRep(scale=1.0) to generate the compressed
  // icon data.
  VerifyCompressedIcon(src_data,
                       *LoadShortcutIcon(shortcut_id, IconType::kCompressed));

  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      web_app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
      /*skip_icon_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // kUncompressed icon.
  apps::IconValuePtr iv1 =
      LoadShortcutIcon(shortcut_id, IconType::kUncompressed);
  ASSERT_EQ(apps::IconType::kUncompressed, iv1->icon_type);
  VerifyIcon(src_image_skia, iv1->uncompressed);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv2 = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
}

TEST_F(BrowserShortcutIconTest, IconUpdated) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string web_app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  apps::ShortcutId shortcut_id = RegisterShortcut(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(web_app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      web_app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
      /*skip_icon_effects=*/true);

  EXPECT_EQ(0, fake_shortcut_publisher()->num_get_icon_call());
  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  VerifyIcon(src_image_skia, iv->uncompressed);

  // Update the icon
  const std::vector<SkColor> colors2{SK_ColorRED, SK_ColorBLUE};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors2);
  gfx::ImageSkia src_image_skia2 = test_helper().GenerateWebAppIcon(
      web_app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
      /*skip_icon_effects=*/true);

  auto delta =
      std::make_unique<Shortcut>(app_constants::kChromeAppId, web_app_id);
  delta->icon_key = IconKey(/*raw_icon_updated=*/true, IconEffects::kNone);
  delta->icon_key->update_version = true;
  app_service_proxy()->PublishShortcut(std::move(delta));

  apps::IconValuePtr iv2 = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  // Updating icon will delete the icon from disk, will need to install icon
  // again.
  EXPECT_EQ(4, fake_shortcut_publisher()->num_get_icon_call());
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia2, iv2->uncompressed);
}

TEST_F(BrowserShortcutIconTest, ShortcutRemovedAndCreatedAgain) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string web_app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(web_app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  apps::ShortcutId shortcut_id = RegisterShortcut(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(web_app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      web_app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
      /*skip_icon_effects=*/true);

  EXPECT_EQ(0, fake_shortcut_publisher()->num_get_icon_call());
  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  EXPECT_EQ(2, fake_shortcut_publisher()->num_get_icon_call());
  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  VerifyIcon(src_image_skia, iv->uncompressed);

  app_service_proxy()->ShortcutRemoved(shortcut_id);
  ShortcutPtr shortcut =
      std::make_unique<Shortcut>(app_constants::kChromeAppId, web_app_id);
  shortcut->icon_key = IconKey();
  app_service_proxy()->PublishShortcut(std::move(shortcut));

  apps::IconValuePtr iv2 = LoadShortcutIcon(shortcut_id, IconType::kStandard);
  // Icon has been deleted from disk, need to install the icon again.
  EXPECT_EQ(4, fake_shortcut_publisher()->num_get_icon_call());
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
}

}  // namespace apps
