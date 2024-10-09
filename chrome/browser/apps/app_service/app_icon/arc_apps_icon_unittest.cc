// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"

namespace apps {

class ArcAppsIconFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    arc_test_.SetUp(profile());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    arc_test_.TearDown();
  }

  arc::mojom::RawIconPngDataPtr GenerateRawArcAppIcon(
      const std::string& app_id,
      ui::ResourceScaleFactor scale_factor) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
    base::test::TestFuture<arc::mojom::RawIconPngDataPtr> result;
    prefs->RequestRawIconData(app_id,
                              ArcAppIconDescriptor(kSizeInDip, scale_factor),
                              result.GetCallback());
    return result.Take();
  }

  IconValuePtr GetArcAppCompressedIconData(
      const std::string& app_id,
      ui::ResourceScaleFactor scale_factor) {
    base::test::TestFuture<IconValuePtr> result;
    apps::GetArcAppCompressedIconData(profile(), app_id, kSizeInDip,
                                      scale_factor, result.GetCallback());
    return result.Take();
  }

  TestingProfile* profile() { return &profile_; }

  ArcAppTest* arc_test() { return &arc_test_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
};

TEST_F(ArcAppsIconFactoryTest, GetArcAppCompressedIconData) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Generate the source raw icon data for comparing.
  auto raw_icon_data =
      GenerateRawArcAppIcon(app_id, ui::ResourceScaleFactor::k100Percent);
  ASSERT_TRUE(raw_icon_data);
  ASSERT_TRUE(raw_icon_data->foreground_icon_png_data.has_value());
  ASSERT_TRUE(raw_icon_data->background_icon_png_data.has_value());

  // Verify the raw icon data.
  auto iv =
      GetArcAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kCompressed);
  ASSERT_EQ(raw_icon_data->foreground_icon_png_data.value(),
            iv->foreground_icon_png_data);
  ASSERT_EQ(raw_icon_data->background_icon_png_data.value(),
            iv->background_icon_png_data);
}

class AppServiceArcAppIconTest : public ArcAppsIconFactoryTest,
                                 public ArcAppListPrefs::Observer {
 public:
  void SetUp() override {
    ArcAppsIconFactoryTest::SetUp();

    proxy_ = AppServiceProxyFactory::GetForProfile(profile());
  }

  void GenerateArcAppUncompressedIcon(const std::string& app_id,
                                      gfx::ImageSkia& image_skia) {
    gfx::ImageSkia foreground_image_skia;
    gfx::ImageSkia background_image_skia;
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      auto raw_icon_data = GenerateRawArcAppIcon(app_id, scale_factor);
      ASSERT_TRUE(raw_icon_data);
      ASSERT_TRUE(raw_icon_data->foreground_icon_png_data.has_value());
      ASSERT_TRUE(raw_icon_data->background_icon_png_data.has_value());

      auto foreground_icon_data =
          raw_icon_data->foreground_icon_png_data.value();
      auto background_icon_data =
          raw_icon_data->background_icon_png_data.value();
      SkBitmap foreground_bitmap;
      SkBitmap background_bitmap;
      gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(&foreground_icon_data.front()),
          foreground_icon_data.size(), &foreground_bitmap);
      gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(&background_icon_data.front()),
          background_icon_data.size(), &background_bitmap);

      foreground_image_skia.AddRepresentation(gfx::ImageSkiaRep(
          foreground_bitmap, ui::GetScaleForResourceScaleFactor(scale_factor)));
      background_image_skia.AddRepresentation(gfx::ImageSkiaRep(
          background_bitmap, ui::GetScaleForResourceScaleFactor(scale_factor)));
    }

    image_skia = apps::CompositeImagesAndApplyMask(foreground_image_skia,
                                                   background_image_skia);
    image_skia.MakeThreadSafe();
  }

  void GenerateArcAppCompressedIcon(const std::string& app_id,
                                    float scale,
                                    std::vector<uint8_t>& result) {
    gfx::ImageSkia image_skia;
    GenerateArcAppUncompressedIcon(app_id, image_skia);

    const gfx::ImageSkiaRep& image_skia_rep =
        image_skia.GetRepresentation(scale);
    ASSERT_EQ(image_skia_rep.scale(), scale);

    const SkBitmap& bitmap = image_skia_rep.GetBitmap();
    const bool discard_transparency = false;
    ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                  &result));
  }

  apps::IconValuePtr LoadIcon(const std::string& app_id, IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy().LoadIcon(app_id, icon_type, kSizeInDip,
                                 /*allow_placeholder_icon=*/false,
                                 result.GetCallback());
    return result.Take();
  }

  apps::IconValuePtr LoadIconWithIconEffects(const std::string& app_id,
                                             uint32_t icon_effects,
                                             IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy().LoadIconWithIconEffects(
        app_id, icon_effects, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, result.GetCallback());
    return result.Take();
  }

  AppServiceProxy& app_service_proxy() { return *proxy_; }

  // Calls `closure` once any update is made to `app_id`'s icons in
  // ArcAppListPrefs.
  void AwaitIconUpdate(const std::string& app_id, base::OnceClosure closure) {
    waiting_icon_updates_[app_id] = std::move(closure);
  }

  // ArcAppListPrefs::Observer:
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override {
    if (base::Contains(waiting_icon_updates_, app_id)) {
      std::move(waiting_icon_updates_[app_id]).Run();
      waiting_icon_updates_.erase(app_id);
    }
  }

 private:
  raw_ptr<AppServiceProxy> proxy_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::flat_map<std::string, base::OnceClosure> waiting_icon_updates_;
};

TEST_F(AppServiceArcAppIconTest, GetCompressedIconDataForUncompressedIcon) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateArcAppUncompressedIcon(app_id, src_image_skia);

  // Verify the icon reading and writing function in AppService for the
  // uncompressed icon.
  IconValuePtr iv = LoadIcon(app_id, IconType::kUncompressed);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kUncompressed);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

TEST_F(AppServiceArcAppIconTest, GetCompressedIconDataForCompressedIcon) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateArcAppCompressedIcon(app_id, /*scale=*/1.0, src_data);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon.
  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));
}

TEST_F(AppServiceArcAppIconTest, GetCompressedIconDataForStandardIcon) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateArcAppUncompressedIcon(app_id, src_image_skia);

  // Apply the paused app badge.
  extensions::ChromeAppIcon::ApplyEffects(
      kSizeInDip, extensions::ChromeAppIcon::ResizeFunction(),
      /*app_launchable=*/false, /*rounded_corners=*/false,
      extensions::ChromeAppIcon::Badge::kPaused, &src_image_skia);
  src_image_skia.MakeThreadSafe();

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  auto ret = LoadIconWithIconEffects(
      app_id, IconEffects::kCrOsStandardIcon | IconEffects::kPaused,
      IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, ret->icon_type);
  VerifyIcon(src_image_skia, ret->uncompressed);
}

TEST_F(AppServiceArcAppIconTest, GetCompressedIconDataFromArcDiskCache) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Ensure that app icons are in the ARC disk cache.
  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      observation(this);
  observation.Observe(arc_test()->arc_app_list_prefs());
  for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
    base::RunLoop run_loop;
    AwaitIconUpdate(app_id, run_loop.QuitClosure());
    arc_test()->arc_app_list_prefs()->MaybeRequestIcon(
        app_id, ArcAppIconDescriptor(kSizeInDip, scale_factor));
    run_loop.Run();
  }

  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateArcAppCompressedIcon(app_id, /*scale=*/1.0, src_data);

  // Stop ARC from running so that LoadIcon requests must load from the disk
  // cache rather than ARC itself.
  arc_test()->StopArcInstance();

  // Verify the icon reading and writing function in AppService for the
  // compressed icon.
  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));
}

}  // namespace apps
