// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif

namespace apps {

class WebAppIconFactoryTest : public ChromeRenderViewHostTestHarness {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  WebAppIconFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kWebAppsCrosapi, ash::features::kLacrosPrimary});
  }
#else
  WebAppIconFactoryTest() = default;
#endif

  ~WebAppIconFactoryTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_app_provider_ = web_app::WebAppProvider::GetForWebApps(profile());
    ASSERT_TRUE(web_app_provider_);

    base::RunLoop run_loop;
    web_app_provider_->sync_bridge_unsafe().Init(run_loop.QuitClosure());
    run_loop.Run();

    icon_manager_ = static_cast<web_app::WebAppIconManager*>(
        &(web_app_provider_->icon_manager()));
    ASSERT_TRUE(icon_manager_);

    sync_bridge_ = &web_app_provider_->sync_bridge_unsafe();
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    std::unique_ptr<web_app::WebAppRegistryUpdate> update =
        sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
    sync_bridge().CommitUpdate(std::move(update), base::DoNothing());
  }

  void WriteIcons(const std::string& app_id,
                  const std::vector<IconPurpose>& purposes,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors) {
    ASSERT_EQ(sizes_px.size(), colors.size());
    ASSERT_TRUE(!purposes.empty());

    IconBitmaps icon_bitmaps;
    for (size_t i = 0; i < sizes_px.size(); ++i) {
      if (base::Contains(purposes, IconPurpose::ANY)) {
        web_app::AddGeneratedIcon(&icon_bitmaps.any, sizes_px[i], colors[i]);
      }
      if (base::Contains(purposes, IconPurpose::MASKABLE)) {
        web_app::AddGeneratedIcon(&icon_bitmaps.maskable, sizes_px[i],
                                  colors[i]);
      }
    }

    base::RunLoop run_loop;
    icon_manager_->WriteData(app_id, std::move(icon_bitmaps), {}, {},
                             base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               run_loop.Quit();
                             }));
    run_loop.Run();
  }

  void GenerateWebAppIcon(const std::string& app_id,
                          IconPurpose purpose,
                          const std::vector<int>& sizes_px,
                          apps::ScaleToSize scale_to_size_in_px,
                          gfx::ImageSkia& output_image_skia,
                          bool skip_icon_effects = false) {
    base::RunLoop run_loop;
    icon_manager().ReadIcons(
        app_id, purpose, sizes_px,
        base::BindOnce(
            [](gfx::ImageSkia* image_skia, int size_in_dip,
               apps::ScaleToSize scale_to_size_in_px,
               base::OnceClosure load_app_icon_callback,
               std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              for (auto it : scale_to_size_in_px) {
                int icon_size_in_px =
                    gfx::ScaleToFlooredSize(gfx::Size(size_in_dip, size_in_dip),
                                            it.first)
                        .width();

                SkBitmap bitmap = icon_bitmaps[it.second];
                if (bitmap.width() != icon_size_in_px) {
                  bitmap = skia::ImageOperations::Resize(
                      bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
                      icon_size_in_px, icon_size_in_px);
                }
                image_skia->AddRepresentation(
                    gfx::ImageSkiaRep(bitmap, it.first));
              }
              std::move(load_app_icon_callback).Run();
            },
            &output_image_skia, kSizeInDip, scale_to_size_in_px,
            run_loop.QuitClosure()));
    run_loop.Run();

    if (!skip_icon_effects) {
      extensions::ChromeAppIcon::ResizeFunction resize_function;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (purpose == IconPurpose::ANY) {
        output_image_skia = apps::CreateStandardIconImage(output_image_skia);
      }
      if (purpose == IconPurpose::MASKABLE) {
        output_image_skia = apps::ApplyBackgroundAndMask(output_image_skia);
      }
#endif

      extensions::ChromeAppIcon::ApplyEffects(
          kSizeInDip, resize_function, true /* app_launchable */,
          true /* from_bookmark */, extensions::ChromeAppIcon::Badge::kNone,
          &output_image_skia);
    }

    EnsureRepresentationsLoaded(output_image_skia);
  }

  void GenerateWebAppCompressedIcon(const std::string& app_id,
                                    IconPurpose purpose,
                                    const std::vector<int>& sizes_px,
                                    apps::ScaleToSize scale_to_size_in_px,
                                    std::vector<uint8_t>& result) {
    gfx::ImageSkia image_skia;
    GenerateWebAppIcon(app_id, purpose, sizes_px, scale_to_size_in_px,
                       image_skia);

    const float scale = 1.0;
    const gfx::ImageSkiaRep& image_skia_rep =
        image_skia.GetRepresentation(scale);
    ASSERT_EQ(image_skia_rep.scale(), scale);

    const SkBitmap& bitmap = image_skia_rep.GetBitmap();
    const bool discard_transparency = false;
    ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                  &result));
  }

  void GenerateWebAppCompressedIcon(const std::string& app_id,
                                    IconPurpose purpose,
                                    IconEffects icon_effects,
                                    const std::vector<int>& sizes_px,
                                    apps::ScaleToSize scale_to_size_in_px,
                                    float scale,
                                    std::vector<uint8_t>& result) {
    gfx::ImageSkia image_skia;
    GenerateWebAppIcon(app_id, purpose, sizes_px, scale_to_size_in_px,
                       image_skia, /*skip_icon_effects=*/true);

    if (icon_effects != apps::IconEffects::kNone) {
      base::test::TestFuture<apps::IconValuePtr> iv_with_icon_effects;
      auto iv = std::make_unique<apps::IconValue>();
      iv->icon_type = apps::IconType::kUncompressed;
      iv->uncompressed = image_skia;
      apps::ApplyIconEffects(icon_effects, kSizeInDip, std::move(iv),
                             iv_with_icon_effects.GetCallback());
      image_skia = iv_with_icon_effects.Take()->uncompressed;
    }

    const gfx::ImageSkiaRep& image_skia_rep =
        image_skia.GetRepresentation(scale);
    ASSERT_EQ(image_skia_rep.scale(), scale);

    const SkBitmap& bitmap = image_skia_rep.GetBitmap();
    const bool discard_transparency = false;
    ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                  &result));
  }

  void LoadIconFromWebApp(const std::string& app_id,
                          apps::IconEffects icon_effects,
                          gfx::ImageSkia& output_image_skia) {
    base::RunLoop run_loop;

    auto icon_type = apps::IconType::kUncompressed;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    icon_type = apps::IconType::kStandard;
#endif

    apps::LoadIconFromWebApp(
        profile(), icon_type, kSizeInDip, app_id, icon_effects,
        base::BindOnce(
            [](gfx::ImageSkia* image, base::OnceClosure load_app_icon_callback,
               apps::IconValuePtr icon) {
              *image = icon->uncompressed;
              std::move(load_app_icon_callback).Run();
            },
            &output_image_skia, run_loop.QuitClosure()));
    run_loop.Run();

    EnsureRepresentationsLoaded(output_image_skia);
  }

  apps::IconValuePtr LoadCompressedIconBlockingFromWebApp(
      const std::string& app_id,
      apps::IconEffects icon_effects) {
    base::RunLoop run_loop;
    apps::IconValuePtr icon_value;
    apps::LoadIconFromWebApp(
        profile(), apps::IconType::kCompressed, kSizeInDip, app_id,
        icon_effects, base::BindLambdaForTesting([&](apps::IconValuePtr icon) {
          icon_value = std::move(icon);
          run_loop.Quit();
        }));
    run_loop.Run();
    return icon_value;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::IconValuePtr GetWebAppCompressedIconData(
      const std::string& app_id,
      ui::ResourceScaleFactor scale_factor) {
    base::test::TestFuture<apps::IconValuePtr> result;
    apps::GetWebAppCompressedIconData(profile(), app_id, kSizeInDip,
                                      scale_factor, result.GetCallback());
    return result.Take();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  web_app::WebAppIconManager& icon_manager() { return *icon_manager_; }

  web_app::WebAppProvider& web_app_provider() { return *web_app_provider_; }

  web_app::WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<web_app::WebAppProvider> web_app_provider_;
  raw_ptr<web_app::WebAppIconManager> icon_manager_;
  raw_ptr<web_app::WebAppSyncBridge> sync_bridge_;
};

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                     {{1.0, kIconSize1}, {2.0, kIconSize2}}, src_image_skia);

  gfx::ImageSkia dst_image_skia;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableNonEffectCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = kSizeInDip;
  const int kIconSize2 = 128;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data;
  GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0, src_data);

  auto icon =
      LoadCompressedIconBlockingFromWebApp(app_id, apps::IconEffects::kNone);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest,
       LoadNonMaskableNonEffectCompressedIconWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data;
  GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0, src_data);

  auto icon =
      LoadCompressedIconBlockingFromWebApp(app_id, apps::IconEffects::kNone);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data;
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY, sizes_px,
                               {{1.0, kIconSize1}, {2.0, kIconSize2}},
                               src_data);

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;
#endif

  auto icon = LoadCompressedIconBlockingFromWebApp(app_id, icon_effect);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  gfx::ImageSkia src_image_skia;
  gfx::ImageSkia dst_image_skia;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                     {{1.0, kIconSize2}, {2.0, kIconSize2}}, src_image_skia);

  LoadIconFromWebApp(app_id,
                     apps::IconEffects::kRoundCorners |
                         apps::IconEffects::kCrOsStandardBackground |
                         apps::IconEffects::kCrOsStandardMask,
                     dst_image_skia);
  VerifyIcon(src_image_skia, dst_image_skia);
  return;
#endif

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize1}));

  GenerateWebAppIcon(app_id, IconPurpose::ANY, {kIconSize1},
                     {{1.0, kIconSize1}, {2.0, kIconSize1}}, src_image_skia);

  LoadIconFromWebApp(app_id, apps::IconEffects::kRoundCorners, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  std::vector<uint8_t> src_data;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
  apps::IconValuePtr icon;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  icon_effect |= apps::IconEffects::kCrOsStandardBackground |
                 apps::IconEffects::kCrOsStandardMask;
  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  GenerateWebAppCompressedIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                               {{1.0, kIconSize2}, {2.0, kIconSize2}},
                               src_data);

  icon = LoadCompressedIconBlockingFromWebApp(app_id, icon_effect);

  VerifyCompressedIcon(src_data, *icon);
  return;
#endif

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize1}));

  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY, {kIconSize1},
                               {{1.0, kIconSize1}, {2.0, kIconSize1}},
                               src_data);

  icon = LoadCompressedIconBlockingFromWebApp(app_id, icon_effect);

  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIconWithMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 128;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize2});

  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize2}));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, {kIconSize2},
                     {{1.0, kIconSize2}, {2.0, kIconSize2}}, src_image_skia);

  gfx::ImageSkia dst_image_skia;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadSmallMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, sizes_px,
                     {{1.0, kIconSize1}, {2.0, kIconSize1}}, src_image_skia);

  gfx::ImageSkia dst_image_skia;
  LoadIconFromWebApp(app_id,
                     apps::IconEffects::kRoundCorners |
                         apps::IconEffects::kCrOsStandardBackground |
                         apps::IconEffects::kCrOsStandardMask,
                     dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadExactSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 48;
  const int kIconSize2 = 64;
  const int kIconSize3 = 96;
  const int kIconSize4 = 128;
  const int kIconSize5 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2, kIconSize3,
                                  kIconSize4, kIconSize5};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorBLACK, SK_ColorRED, SK_ColorBLUE};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                     {{1.0, kIconSize2}, {2.0, kIconSize4}}, src_image_skia);

  gfx::ImageSkia dst_image_skia;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadIconFailed) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 48;
  const int kIconSize2 = 64;
  const int kIconSize3 = 96;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2, kIconSize3};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW,
                                    SK_ColorBLACK};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia;
  LoadDefaultIcon(src_image_skia);

  gfx::ImageSkia dst_image_skia;
  LoadIconFromWebApp(
      app_id,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon,
      dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppIconFactoryTest, ConvertSquareBitmapsToImageSkia_Empty) {
  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      /*icon_bitmaps=*/std::map<SquareSizePx, SkBitmap>{},
      /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);

  EXPECT_TRUE(converted_image.isNull());
}

TEST_F(WebAppIconFactoryTest,
       ConvertSquareBitmapsToImageSkia_OneBigIconForDownscale) {
  std::map<SquareSizePx, SkBitmap> icon_bitmaps;
  web_app::AddGeneratedIcon(&icon_bitmaps, web_app::icon_size::k512,
                            SK_ColorYELLOW);

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (auto& scale_factor : scale_factors) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    ASSERT_TRUE(converted_image.HasRepresentation(scale));
    EXPECT_EQ(
        SK_ColorYELLOW,
        converted_image.GetRepresentation(scale).GetBitmap().getColor(0, 0));
  }
}

TEST_F(WebAppIconFactoryTest,
       ConvertSquareBitmapsToImageSkia_OneSmallIconNoUpscale) {
  std::map<SquareSizePx, SkBitmap> icon_bitmaps;
  web_app::AddGeneratedIcon(&icon_bitmaps, web_app::icon_size::k16,
                            SK_ColorMAGENTA);

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);
  EXPECT_TRUE(converted_image.isNull());
}

TEST_F(WebAppIconFactoryTest, ConvertSquareBitmapsToImageSkia_MatchBigger) {
  const std::vector<SquareSizePx> sizes_px{
      web_app::icon_size::k16, web_app::icon_size::k32, web_app::icon_size::k48,
      web_app::icon_size::k64, web_app::icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED, SK_ColorMAGENTA,
                                    SK_ColorGREEN, SK_ColorWHITE};

  std::map<SquareSizePx, SkBitmap> icon_bitmaps;
  for (size_t i = 0; i < sizes_px.size(); ++i) {
    web_app::AddGeneratedIcon(&icon_bitmaps, sizes_px[i], colors[i]);
  }

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  // Expects 32px and 64px to be chosen for 32dip-normal and 32dip-hi-DPI (2.0f
  // scale).
  const std::vector<SkColor> expected_colors{SK_ColorRED, SK_ColorGREEN};

  for (size_t i = 0; i < scale_factors.size(); ++i) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factors[i]);
    ASSERT_TRUE(converted_image.HasRepresentation(scale));
    EXPECT_EQ(
        expected_colors[i],
        converted_image.GetRepresentation(scale).GetBitmap().getColor(0, 0));
  }
}

TEST_F(WebAppIconFactoryTest, ConvertSquareBitmapsToImageSkia_StandardEffect) {
  const std::vector<SquareSizePx> sizes_px{web_app::icon_size::k48,
                                           web_app::icon_size::k96};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED};

  std::map<SquareSizePx, SkBitmap> icon_bitmaps;
  for (size_t i = 0; i < sizes_px.size(); ++i) {
    web_app::AddGeneratedIcon(&icon_bitmaps, sizes_px[i], colors[i]);
  }

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps,
      /*icon_effects=*/apps::IconEffects::kCrOsStandardBackground |
          apps::IconEffects::kCrOsStandardMask,
      /*size_hint_in_dip=*/32);

  EnsureRepresentationsLoaded(converted_image);

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (size_t i = 0; i < scale_factors.size(); ++i) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factors[i]);
    ASSERT_TRUE(converted_image.HasRepresentation(scale));

    // No color in the upper left corner.
    EXPECT_FALSE(
        converted_image.GetRepresentation(scale).GetBitmap().getColor(0, 0));

    // Has color in the center.
    const SquareSizePx center_px = sizes_px[i] / 2;
    EXPECT_TRUE(converted_image.GetRepresentation(scale).GetBitmap().getColor(
        center_px, center_px));
  }
}

// Regression test for crash. https://crbug.com/1335266
TEST_F(WebAppIconFactoryTest, ApplyBackgroundAndMask_NullImage) {
  gfx::ImageSkia image = apps::ApplyBackgroundAndMask(gfx::ImageSkia());
  DCHECK(image.isNull());
}

TEST_F(WebAppIconFactoryTest, GetNonMaskableCompressedIconData) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale1, src_data1);
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale2, src_data2);

  // Verify getting the compressed icon data for the compressed icon with icon
  // effects.
  auto icon1 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  auto icon2 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k200Percent);

  VerifyCompressedIcon(src_data1, *icon1);
  ASSERT_FALSE(icon1->is_maskable_icon);
  VerifyCompressedIcon(src_data2, *icon2);
  ASSERT_FALSE(icon2->is_maskable_icon);
}

TEST_F(WebAppIconFactoryTest,
       GetNonMaskableCompressedIconDataWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale1, src_data1);
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale2, src_data2);

  // Verify getting the compressed icon data for the compressed icon with icon
  // effects.
  auto icon1 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  auto icon2 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k200Percent);

  VerifyCompressedIcon(src_data1, *icon1);
  ASSERT_FALSE(icon1->is_maskable_icon);
  VerifyCompressedIcon(src_data2, *icon2);
  ASSERT_FALSE(icon2->is_maskable_icon);
}

TEST_F(WebAppIconFactoryTest, GetNonMaskableNonEffectCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale1, src_data1);
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale2, src_data2);

  auto icon1 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  auto icon2 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k200Percent);

  VerifyCompressedIcon(src_data1, *icon1);
  ASSERT_FALSE(icon1->is_maskable_icon);
  VerifyCompressedIcon(src_data2, *icon2);
  ASSERT_FALSE(icon2->is_maskable_icon);
}

TEST_F(WebAppIconFactoryTest,
       GetNonMaskableNonEffectCompressedIconWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale1, src_data1);
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale2, src_data2);

  // Verify getting the compressed icon data for the compressed icon.
  auto icon1 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  auto icon2 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k200Percent);

  VerifyCompressedIcon(src_data1, *icon1);
  ASSERT_FALSE(icon1->is_maskable_icon);
  VerifyCompressedIcon(src_data2, *icon2);
  ASSERT_FALSE(icon2->is_maskable_icon);
}

TEST_F(WebAppIconFactoryTest, GetMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::MASKABLE,
                               apps::IconEffects::kNone, {kIconSize2},
                               scale_to_size_in_px, scale1, src_data1);
  GenerateWebAppCompressedIcon(app_id, IconPurpose::MASKABLE,
                               apps::IconEffects::kNone, {kIconSize2},
                               scale_to_size_in_px, scale2, src_data2);

  apps::IconValuePtr icon;

  // Verify getting the compressed icon data for the compressed icon.
  auto icon1 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k100Percent);
  auto icon2 =
      GetWebAppCompressedIconData(app_id, ui::ResourceScaleFactor::k200Percent);

  VerifyCompressedIcon(src_data1, *icon1);
  ASSERT_TRUE(icon1->is_maskable_icon);
  VerifyCompressedIcon(src_data2, *icon2);
  ASSERT_TRUE(icon1->is_maskable_icon);
}

class AppServiceWebAppIconTest : public WebAppIconFactoryTest {
 public:
  void SetUp() override {
    WebAppIconFactoryTest::SetUp();

    proxy_ = AppServiceProxyFactory::GetForProfile(profile());
    fake_icon_loader_ = std::make_unique<apps::FakeIconLoader>(proxy_);
    OverrideAppServiceProxyInnerIconLoader(fake_icon_loader_.get());
    fake_publisher_ =
        std::make_unique<apps::FakePublisherForIconTest>(proxy_, AppType::kWeb);
    scoped_decode_request_for_testing_ =
        std::make_unique<ScopedDecodeRequestForTesting>();
  }

  void OverrideAppServiceProxyInnerIconLoader(apps::IconLoader* icon_loader) {
    app_service_proxy().OverrideInnerIconLoaderForTesting(icon_loader);
  }

  apps::IconValuePtr LoadIcon(const std::string& app_id, IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy().LoadIcon(AppType::kWeb, app_id, icon_type, kSizeInDip,
                                 /*allow_placeholder_icon=*/false,
                                 result.GetCallback());
    return result.Take();
  }

  apps::IconValuePtr LoadIconFromIconKey(const std::string& app_id,
                                         const IconKey& icon_key,
                                         IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy().LoadIconFromIconKey(
        AppType::kWeb, app_id, icon_key, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, result.GetCallback());
    return result.Take();
  }

  // Call LoadIconFromIconKey twice with the same parameters, to verify the icon
  // loading process can handle the icon loading request multiple times with the
  // same params.
  std::vector<apps::IconValuePtr> MultipleLoadIconFromIconKey(
      const std::string& app_id,
      const IconKey& icon_key,
      IconType icon_type) {
    base::test::TestFuture<std::vector<apps::IconValuePtr>> result;
    auto barrier_callback =
        base::BarrierCallback<apps::IconValuePtr>(2, result.GetCallback());

    app_service_proxy().LoadIconFromIconKey(
        AppType::kWeb, app_id, icon_key, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);
    app_service_proxy().LoadIconFromIconKey(
        AppType::kWeb, app_id, icon_key, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);

    return result.Take();
  }

  AppServiceProxy& app_service_proxy() { return *proxy_; }

 private:
  raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<apps::FakeIconLoader> fake_icon_loader_;
  std::unique_ptr<apps::FakePublisherForIconTest> fake_publisher_;
  std::unique_ptr<ScopedDecodeRequestForTesting>
      scoped_decode_request_for_testing_;

  base::WeakPtrFactory<AppServiceWebAppIconTest> weak_ptr_factory_{this};
};

TEST_F(AppServiceWebAppIconTest, GetNonMaskableCompressedIconData) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data;
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kRoundCorners, sizes_px,
                               scale_to_size_in_px, scale1, src_data);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects.
  IconKey icon_key;
  icon_key.icon_effects = apps::IconEffects::kRoundCorners;
  VerifyCompressedIcon(
      src_data, *LoadIconFromIconKey(app_id, icon_key, IconType::kCompressed));
}

TEST_F(AppServiceWebAppIconTest, GetNonMaskableStandardIconData) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
                     src_image_skia);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  IconKey icon_key;
  icon_key.icon_effects =
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon;
  apps::IconValuePtr iv =
      LoadIconFromIconKey(app_id, icon_key, IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

TEST_F(AppServiceWebAppIconTest,
       GetNonMaskableCompressedIconDataWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale = 1.0;
  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  // Create the web app compressed icon data for the size in dip 64.
  // 1. The icon file of size 96px will be resized to 64 to generated the
  // ImageSkiaRep for the scale 1.0.
  // 2. The icon file of size 256px will be resized to 128 to generated the
  // ImageSkiaRep for the scale 2.0.
  //
  // The generated ImageSkia will be applied with the icon effect kRoundCorners.
  // Then the ImageSkiaRep(scale=1.0) is encoded to generate the compressed icon
  // data `src_data`.
  std::vector<uint8_t> src_data;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kRoundCorners, sizes_px,
                               scale_to_size_in_px, scale, src_data);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects. LoadIconFromIconKey can generate the
  // ImageSkia(size_in_dip=64) with icon files(96px and 256px) after resizing
  // them, then apply the icon effect, and encode the ImageSkiaRep(scale=1.0) to
  // generate the compressed icon data.
  IconKey icon_key;
  icon_key.icon_effects = apps::IconEffects::kRoundCorners;
  VerifyCompressedIcon(
      src_data, *LoadIconFromIconKey(app_id, icon_key, IconType::kCompressed));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
                     src_image_skia, /*skip_icon_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // kUncompressed icon.
  apps::IconValuePtr iv1 = LoadIcon(app_id, IconType::kUncompressed);
  ASSERT_EQ(apps::IconType::kUncompressed, iv1->icon_type);
  VerifyIcon(src_image_skia, iv1->uncompressed);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv2 = LoadIcon(app_id, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
}

TEST_F(AppServiceWebAppIconTest,
       GetNonMaskableStandardIconDataWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
                     src_image_skia);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  IconKey icon_key;
  icon_key.icon_effects =
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon;
  auto ret = MultipleLoadIconFromIconKey(app_id, icon_key, IconType::kStandard);

  ASSERT_EQ(2U, ret.size());
  ASSERT_EQ(apps::IconType::kStandard, ret[0]->icon_type);
  VerifyIcon(src_image_skia, ret[0]->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, ret[1]->icon_type);
  VerifyIcon(src_image_skia, ret[1]->uncompressed);
}

TEST_F(AppServiceWebAppIconTest, GetNonMaskableNonEffectCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale1, src_data);

  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));
}

TEST_F(AppServiceWebAppIconTest,
       GetNonMaskableNonEffectCompressedIconWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale = 1.0;
  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY,
                               apps::IconEffects::kNone, sizes_px,
                               scale_to_size_in_px, scale, src_data);

  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
                     src_image_skia, /*skip_icon_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // kUncompressed icon.
  apps::IconValuePtr iv1 = LoadIcon(app_id, IconType::kUncompressed);
  ASSERT_EQ(apps::IconType::kUncompressed, iv1->icon_type);
  VerifyIcon(src_image_skia, iv1->uncompressed);
  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv2 = LoadIcon(app_id, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
}

TEST_F(AppServiceWebAppIconTest, GetMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale = 1.0;
  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  std::vector<uint8_t> src_data;
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  GenerateWebAppCompressedIcon(app_id, IconPurpose::MASKABLE,
                               apps::IconEffects::kNone, {kIconSize2},
                               scale_to_size_in_px, scale, src_data);

  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                     scale_to_size_in_px, src_image_skia,
                     /*skip_icon_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // kUncompressed icon.
  apps::IconValuePtr iv1 = LoadIcon(app_id, IconType::kUncompressed);
  ASSERT_EQ(apps::IconType::kUncompressed, iv1->icon_type);
  VerifyIcon(src_image_skia, iv1->uncompressed);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv2 = LoadIcon(app_id, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia, iv2->uncompressed);
}

TEST_F(AppServiceWebAppIconTest, GetMaskableStandardIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE}, sizes_px,
             colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                     scale_to_size_in_px, src_image_skia);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  // Set the icon effects kCrOsStandardIcon. AppIconReader should convert the
  // icon effects to kCrOsStandardBackground and kCrOsStandardMask for the
  // maskable icon.
  IconKey icon_key;
  icon_key.icon_effects =
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon;
  auto ret = MultipleLoadIconFromIconKey(app_id, icon_key, IconType::kStandard);

  ASSERT_EQ(2U, ret.size());
  ASSERT_EQ(apps::IconType::kStandard, ret[0]->icon_type);
  VerifyIcon(src_image_skia, ret[0]->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, ret[1]->icon_type);
  VerifyIcon(src_image_skia, ret[1]->uncompressed);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
