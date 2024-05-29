// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_icon/web_app_icon_test_helper.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif

namespace apps {

using IconPurpose = web_app::IconPurpose;

class WebAppIconFactoryTest : public testing::Test {
 public:
  WebAppIconFactoryTest() = default;

  ~WebAppIconFactoryTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  gfx::ImageSkia LoadIconFromWebApp(const std::string& app_id,
                                    apps::IconEffects icon_effects) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromWebApp(profile(), apps::IconType::kStandard, kSizeInDip,
                             app_id, icon_effects, future.GetCallback());
    auto icon = future.Take();
    EnsureRepresentationsLoaded(icon->uncompressed);
    return icon->uncompressed;
  }

  apps::IconValuePtr LoadCompressedIconBlockingFromWebApp(
      const std::string& app_id,
      apps::IconEffects icon_effects) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromWebApp(profile(), apps::IconType::kCompressed, kSizeInDip,
                             app_id, icon_effects, future.GetCallback());
    auto icon = future.Take();
    return icon;
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

  web_app::WebAppIconManager& icon_manager() {
    return web_app_provider().icon_manager();
  }

  web_app::WebAppProvider& web_app_provider() {
    return *web_app::WebAppProvider::GetForWebApps(profile());
  }

  web_app::WebAppSyncBridge& sync_bridge() {
    return web_app_provider().sync_bridge_unsafe();
  }

  Profile* profile() { return profile_.get(); }

  WebAppIconTestHelper test_helper() { return WebAppIconTestHelper(profile()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                                       {{1.0, kIconSize1}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableNonEffectCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = kSizeInDip;
  const int kIconSize2 = 128;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}}, /*scale=*/1.0);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, sizes_px,
      {{1.0, kIconSize1}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2},
      {{1.0, kIconSize2}, {2.0, kIconSize2}});

  gfx::ImageSkia dst = LoadIconFromWebApp(
      app_id, apps::IconEffects::kRoundCorners |
                  apps::IconEffects::kCrOsStandardBackground |
                  apps::IconEffects::kCrOsStandardMask);
  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
  apps::IconValuePtr icon;

  icon_effect |= apps::IconEffects::kCrOsStandardBackground |
                 apps::IconEffects::kCrOsStandardMask;
  ASSERT_TRUE(
      icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2},
      {{1.0, kIconSize2}, {2.0, kIconSize2}});

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize2}));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, {kIconSize2},
                                       {{1.0, kIconSize2}, {2.0, kIconSize2}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
}

TEST_F(WebAppIconFactoryTest, LoadSmallMaskableIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, sizes_px,
                                       {{1.0, kIconSize1}, {2.0, kIconSize1}});

  gfx::ImageSkia dst = LoadIconFromWebApp(
      app_id, apps::IconEffects::kRoundCorners |
                  apps::IconEffects::kCrOsStandardBackground |
                  apps::IconEffects::kCrOsStandardMask);

  VerifyIcon(src_image_skia, dst);
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia =
      test_helper().GenerateWebAppIcon(app_id, IconPurpose::ANY, sizes_px,
                                       {{1.0, kIconSize2}, {2.0, kIconSize4}});

  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

  icon_effect |= apps::IconEffects::kCrOsStandardIcon;

  gfx::ImageSkia dst = LoadIconFromWebApp(app_id, icon_effect);

  VerifyIcon(src_image_skia, dst);
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);

  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  gfx::ImageSkia src_image_skia;
  LoadDefaultIcon(src_image_skia);

  gfx::ImageSkia dst =
      LoadIconFromWebApp(app_id, apps::IconEffects::kRoundCorners |
                                     apps::IconEffects::kCrOsStandardIcon);

  VerifyIcon(src_image_skia, dst);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppIconFactoryTest, ConvertSquareBitmapsToImageSkia_Empty) {
  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      /*icon_bitmaps=*/std::map<web_app::SquareSizePx, SkBitmap>{},
      /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);

  EXPECT_TRUE(converted_image.isNull());
}

TEST_F(WebAppIconFactoryTest,
       ConvertSquareBitmapsToImageSkia_OneBigIconForDownscale) {
  std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps;
  web_app::AddGeneratedIcon(&icon_bitmaps, web_app::icon_size::k512,
                            SK_ColorYELLOW);

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (const auto scale_factor : scale_factors) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    ASSERT_TRUE(converted_image.HasRepresentation(scale));
    EXPECT_EQ(
        SK_ColorYELLOW,
        converted_image.GetRepresentation(scale).GetBitmap().getColor(0, 0));
  }
}

TEST_F(WebAppIconFactoryTest,
       ConvertSquareBitmapsToImageSkia_OneSmallIconNoUpscale) {
  std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps;
  web_app::AddGeneratedIcon(&icon_bitmaps, web_app::icon_size::k16,
                            SK_ColorMAGENTA);

  gfx::ImageSkia converted_image = ConvertSquareBitmapsToImageSkia(
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kNone,
      /*size_hint_in_dip=*/32);
  EXPECT_TRUE(converted_image.isNull());
}

TEST_F(WebAppIconFactoryTest, ConvertSquareBitmapsToImageSkia_MatchBigger) {
  const std::vector<web_app::SquareSizePx> sizes_px{
      web_app::icon_size::k16, web_app::icon_size::k32, web_app::icon_size::k48,
      web_app::icon_size::k64, web_app::icon_size::k128};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED, SK_ColorMAGENTA,
                                    SK_ColorGREEN, SK_ColorWHITE};

  std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps;
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
  const std::vector<web_app::SquareSizePx> sizes_px{web_app::icon_size::k48,
                                                    web_app::icon_size::k96};
  const std::vector<SkColor> colors{SK_ColorBLUE, SK_ColorRED};

  std::map<web_app::SquareSizePx, SkBitmap> icon_bitmaps;
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
    const web_app::SquareSizePx center_px = sizes_px[i] / 2;
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);
  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale2);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);
  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale2);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);
  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale2);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  test_helper().RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);
  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale2);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  test_helper().RegisterApp(std::move(web_app));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::MASKABLE, apps::IconEffects::kNone, {kIconSize2},
      scale_to_size_in_px, scale1);
  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::MASKABLE, apps::IconEffects::kNone, {kIconSize2},
      scale_to_size_in_px, scale2);

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
  }

  void OverrideAppServiceProxyInnerIconLoader(apps::IconLoader* icon_loader) {
    app_service_proxy().OverrideInnerIconLoaderForTesting(icon_loader);
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

  // Call LoadIconWithIconEffects twice with the same parameters, to verify the
  // icon loading process can handle the icon loading request multiple times
  // with the same params.
  std::vector<apps::IconValuePtr> MultipleLoadIconWithSameIconEffects(
      const std::string& app_id,
      uint32_t icon_effects,
      IconType icon_type) {
    base::test::TestFuture<std::vector<apps::IconValuePtr>> result;
    auto barrier_callback =
        base::BarrierCallback<apps::IconValuePtr>(2, result.GetCallback());

    app_service_proxy().LoadIconWithIconEffects(
        app_id, icon_effects, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);
    app_service_proxy().LoadIconWithIconEffects(
        app_id, icon_effects, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);

    return result.Take();
  }

  // Call LoadIconWithIconEffects twice with icon_effects1 and icon_effects2, to
  // verify the icon loading process can handle the icon loading request
  // multiple times with the different icon effects.
  std::vector<apps::IconValuePtr> MultipleLoadIconWithIconEffects(
      const std::string& app_id,
      uint32_t icon_effects1,
      uint32_t icon_effects2,
      IconType icon_type) {
    base::test::TestFuture<std::vector<apps::IconValuePtr>> result;
    auto barrier_callback =
        base::BarrierCallback<apps::IconValuePtr>(2, result.GetCallback());

    app_service_proxy().LoadIconWithIconEffects(
        app_id, icon_effects1, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);
    app_service_proxy().LoadIconWithIconEffects(
        app_id, icon_effects2, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, barrier_callback);

    return result.Take();
  }

  void UpdateIcon(const std::string& app_id, const IconKey& icon_key) {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, app_id);
    app->icon_key = std::move(*icon_key.Clone());
    apps.push_back(std::move(app));
    app_service_proxy().OnApps(std::move(apps), AppType::kWeb,
                               /*should_notify_initialized=*/false);
  }

  void ReinstallApps(const std::vector<std::string>& app_ids) {
    std::vector<AppPtr> uninstall_apps;
    for (const auto& app_id : app_ids) {
      AppPtr app = std::make_unique<App>(AppType::kWeb, app_id);
      app->readiness = Readiness::kUninstalledByUser;
      uninstall_apps.push_back(std::move(app));
    }
    app_service_proxy().OnApps(std::move(uninstall_apps), AppType::kWeb,
                               /*should_notify_initialized=*/false);

    std::vector<AppPtr> reinstall_apps;
    for (const auto& app_id : app_ids) {
      AppPtr app = std::make_unique<App>(AppType::kWeb, app_id);
      app->readiness = Readiness::kReady;
      reinstall_apps.push_back(std::move(app));
    }
    app_service_proxy().OnApps(std::move(reinstall_apps), AppType::kWeb,
                               /*should_notify_initialized=*/false);
  }

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    std::vector<AppPtr> apps;
    AppPtr app = std::make_unique<App>(AppType::kWeb, web_app->app_id());
    app->readiness = Readiness::kReady;
    apps.push_back(std::move(app));

    test_helper().RegisterApp(std::move(web_app));
    app_service_proxy().OnApps(std::move(apps), AppType::kWeb,
                               /*should_notify_initialized=*/false);
  }

  AppServiceProxy& app_service_proxy() { return *proxy_; }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<apps::FakeIconLoader> fake_icon_loader_;
  std::unique_ptr<apps::FakePublisherForIconTest> fake_publisher_;
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kRoundCorners, sizes_px,
      scale_to_size_in_px, scale1);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects.
  VerifyCompressedIcon(src_data, *LoadIconWithIconEffects(
                                     app_id, apps::IconEffects::kRoundCorners,
                                     IconType::kCompressed));
}

// Verify AppIconWriter when write icon files multiple times for compressed
// icons with icon effects and without icon effects separately.
TEST_F(AppServiceWebAppIconTest, GetNonMaskableCompressedIconDatasSeparately) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);

  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kRoundCorners, sizes_px,
      scale_to_size_in_px, scale1);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon without icon effects.
  VerifyCompressedIcon(src_data1,
                       *LoadIconWithIconEffects(app_id, IconEffects::kNone,
                                                IconType::kCompressed));

  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects.
  VerifyCompressedIcon(src_data2, *LoadIconWithIconEffects(
                                      app_id, apps::IconEffects::kRoundCorners,
                                      IconType::kCompressed));
}

// Verify AppIconWriter when write icon files multiple times for compressed
// icons with icon effects and without icon effects at the same time.
TEST_F(AppServiceWebAppIconTest, GetNonMaskableCompressedIconDatas) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data1 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);

  std::vector<uint8_t> src_data2 = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kRoundCorners, sizes_px,
      scale_to_size_in_px, scale1);

  // Verify the icon reading and writing function in AppService at the same time
  // for the compressed icons with and without icon effects.
  auto ret = MultipleLoadIconWithIconEffects(app_id, apps::IconEffects::kNone,
                                             apps::IconEffects::kRoundCorners,
                                             IconType::kCompressed);

  ASSERT_EQ(2U, ret.size());
  VerifyCompressedIcon(src_data1, *ret[0]);
  VerifyCompressedIcon(src_data2, *ret[1]);
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  apps::IconValuePtr iv = LoadIconWithIconEffects(
      app_id,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon,
      IconType::kStandard);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

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
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kRoundCorners, sizes_px,
      scale_to_size_in_px, scale);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon with icon effects. LoadIconWithIconEffects can generate the
  // ImageSkia(size_in_dip=64) with icon files(96px and 256px) after resizing
  // them, then apply the icon effect, and encode the ImageSkiaRep(scale=1.0) to
  // generate the compressed icon data.
  VerifyCompressedIcon(src_data, *LoadIconWithIconEffects(
                                     app_id, apps::IconEffects::kRoundCorners,
                                     IconType::kCompressed));

  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
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

TEST_F(AppServiceWebAppIconTest,
       GetNonMaskableStandardIconDataWithDifferentSizeIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const int kIconSize1 = 96;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  auto ret = MultipleLoadIconWithSameIconEffects(
      app_id,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon,
      IconType::kStandard);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale1);

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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
      scale_to_size_in_px, scale);

  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));

  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px,
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

TEST_F(AppServiceWebAppIconTest, GetMaskableCompressedIcon) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale = 1.0;
  const int kIconSize1 = 128;
  const int kIconSize2 = 256;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));
  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  std::vector<uint8_t> src_data = test_helper().GenerateWebAppCompressedIcon(
      app_id, IconPurpose::MASKABLE, apps::IconEffects::kNone, {kIconSize2},
      scale_to_size_in_px, scale);

  VerifyCompressedIcon(src_data, *LoadIcon(app_id, IconType::kCompressed));

  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2}, scale_to_size_in_px,
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
  test_helper().WriteIcons(app_id, {IconPurpose::ANY, IconPurpose::MASKABLE},
                           sizes_px, colors);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {kIconSize1});
  web_app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {kIconSize2});

  RegisterApp(std::move(web_app));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize2},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::MASKABLE, {kIconSize2}, scale_to_size_in_px);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  // Set the icon effects kCrOsStandardIcon. AppIconReader should convert the
  // icon effects to kCrOsStandardBackground and kCrOsStandardMask for the
  // maskable icon.
  auto ret = MultipleLoadIconWithSameIconEffects(
      app_id,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon,
      IconType::kStandard);

  ASSERT_EQ(2U, ret.size());
  ASSERT_EQ(apps::IconType::kStandard, ret[0]->icon_type);
  VerifyIcon(src_image_skia, ret[0]->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, ret[1]->icon_type);
  VerifyIcon(src_image_skia, ret[1]->uncompressed);
}

// Verify we can get the new icon after the app icon is updated.
TEST_F(AppServiceWebAppIconTest, IconUpdate) {
  auto web_app = web_app::test::CreateWebApp();
  const std::string app_id = web_app->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors1{SK_ColorGREEN, SK_ColorYELLOW};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors1);

  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app));

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia1 = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  // Load the kStandard icon to generate the icon file in the AppService
  // directory.
  uint32_t icon_effects =
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon;
  apps::IconValuePtr iv1 =
      LoadIconWithIconEffects(app_id, icon_effects, IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, iv1->icon_type);
  VerifyIcon(src_image_skia1, iv1->uncompressed);

  // Update the icon
  const std::vector<SkColor> colors2{SK_ColorRED, SK_ColorBLUE};
  test_helper().WriteIcons(app_id, {IconPurpose::ANY}, sizes_px, colors2);
  gfx::ImageSkia src_image_skia2 = test_helper().GenerateWebAppIcon(
      app_id, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  IconKey icon_key(/*raw_icon_updated=*/true, icon_effects);
  UpdateIcon(app_id, icon_key);

  // Load the kStandard icon again after updating the icon.
  apps::IconValuePtr iv2 =
      LoadIconWithIconEffects(app_id, icon_effects, IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia2, iv2->uncompressed);
}

// Verify we can get the new app icons after the apps are uninstalled and
// reinstalled again.
TEST_F(AppServiceWebAppIconTest, IconLoadingForReinstallApps) {
  const GURL app1_scope("https://example.com/app");
  const GURL app2_scope("https://example.com/app-two");

  auto web_app1 = web_app::test::CreateWebApp(app1_scope);
  const std::string app_id1 = web_app1->app_id();
  auto web_app2 = web_app::test::CreateWebApp(app2_scope);
  const std::string app_id2 = web_app2->app_id();

  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = kSizeInDip * scale1;
  const int kIconSize2 = kSizeInDip * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors1{SK_ColorGREEN, SK_ColorYELLOW};
  const std::vector<SkColor> colors2{SK_ColorRED, SK_ColorBLUE};
  test_helper().WriteIcons(app_id1, {IconPurpose::ANY}, sizes_px, colors1);
  test_helper().WriteIcons(app_id2, {IconPurpose::ANY}, sizes_px, colors2);

  web_app1->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app1));
  web_app2->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  RegisterApp(std::move(web_app2));

  ASSERT_TRUE(icon_manager().HasIcons(app_id1, IconPurpose::ANY, sizes_px));
  ASSERT_TRUE(icon_manager().HasIcons(app_id2, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};
  gfx::ImageSkia src_image_skia1 = test_helper().GenerateWebAppIcon(
      app_id1, IconPurpose::ANY, sizes_px, scale_to_size_in_px);
  gfx::ImageSkia src_image_skia2 = test_helper().GenerateWebAppIcon(
      app_id2, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  // Load the kStandard icon to generate the icon files in the AppService
  // directory.
  uint32_t icon_effects =
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon;
  apps::IconValuePtr iv1 =
      LoadIconWithIconEffects(app_id1, icon_effects, IconType::kStandard);
  apps::IconValuePtr iv2 =
      LoadIconWithIconEffects(app_id2, icon_effects, IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, iv1->icon_type);
  VerifyIcon(src_image_skia1, iv1->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, iv2->icon_type);
  VerifyIcon(src_image_skia2, iv2->uncompressed);

  const std::vector<SkColor> colors3{SK_ColorBLACK, SK_ColorWHITE};
  const std::vector<SkColor> colors4{SK_ColorDKGRAY, SK_ColorLTGRAY};
  test_helper().WriteIcons(app_id1, {IconPurpose::ANY}, sizes_px, colors3);
  test_helper().WriteIcons(app_id2, {IconPurpose::ANY}, sizes_px, colors4);

  gfx::ImageSkia src_image_skia3 = test_helper().GenerateWebAppIcon(
      app_id1, IconPurpose::ANY, sizes_px, scale_to_size_in_px);
  gfx::ImageSkia src_image_skia4 = test_helper().GenerateWebAppIcon(
      app_id2, IconPurpose::ANY, sizes_px, scale_to_size_in_px);

  // Uninstall and reinstall apps
  ReinstallApps({app_id1, app_id2});

  // Load the kStandard icons again after reinstall apps.
  apps::IconValuePtr iv3 =
      LoadIconWithIconEffects(app_id1, icon_effects, IconType::kStandard);
  apps::IconValuePtr iv4 =
      LoadIconWithIconEffects(app_id2, icon_effects, IconType::kStandard);

  ASSERT_EQ(apps::IconType::kStandard, iv3->icon_type);
  VerifyIcon(src_image_skia3, iv3->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, iv4->icon_type);
  VerifyIcon(src_image_skia4, iv4->uncompressed);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
