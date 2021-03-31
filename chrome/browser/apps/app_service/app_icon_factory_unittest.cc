// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#endif

namespace {

const int kSizeInDip = 64;

void EnsureRepresentationsLoaded(gfx::ImageSkia& output_image_skia) {
  for (auto scale_factor : ui::GetSupportedScaleFactors()) {
    // Force the icon to be loaded.
    output_image_skia.GetRepresentation(
        ui::GetScaleForScaleFactor(scale_factor));
  }
}

void LoadDefaultIcon(gfx::ImageSkia& output_image_skia) {
  base::RunLoop run_loop;
  apps::LoadIconFromResource(
      apps::mojom::IconType::kUncompressed, kSizeInDip, IDR_APP_DEFAULT_ICON,
      false /* is_placeholder_icon */, apps::IconEffects::kNone,
      base::BindOnce(
          [](gfx::ImageSkia* image, base::OnceClosure load_app_icon_callback,
             apps::mojom::IconValuePtr icon) {
            *image = icon->uncompressed;
            std::move(load_app_icon_callback).Run();
          },
          &output_image_skia, run_loop.QuitClosure()));
  run_loop.Run();
  EnsureRepresentationsLoaded(output_image_skia);
}

void VerifyIcon(const gfx::ImageSkia& src, const gfx::ImageSkia& dst) {
  ASSERT_FALSE(src.isNull());
  ASSERT_FALSE(dst.isNull());

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (auto& scale_factor : scale_factors) {
    const float scale = ui::GetScaleForScaleFactor(scale_factor);
    ASSERT_TRUE(src.HasRepresentation(scale));
    ASSERT_TRUE(dst.HasRepresentation(scale));
    ASSERT_TRUE(
        gfx::test::AreBitmapsEqual(src.GetRepresentation(scale).GetBitmap(),
                                   dst.GetRepresentation(scale).GetBitmap()));
  }
}

void VerifyCompressedIcon(const std::vector<uint8_t>& src_data,
                          apps::mojom::IconValuePtr& icon) {
  ASSERT_FALSE(icon.is_null());
  ASSERT_EQ(apps::mojom::IconType::kCompressed, icon->icon_type);
  ASSERT_FALSE(icon->is_placeholder_icon);
  ASSERT_TRUE(icon->compressed.has_value());
  ASSERT_FALSE(icon->compressed.value().empty());

  ASSERT_EQ(src_data, icon->compressed.value());
}

}  // namespace

class AppIconFactoryTest : public testing::Test {
 public:
  base::FilePath GetPath() {
    return tmp_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe("icon.file"));
  }

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  void RunLoadIconFromFileWithFallback(
      apps::mojom::IconValuePtr fallback_response,
      bool* callback_called,
      bool* fallback_called,
      apps::mojom::IconValuePtr* result) {
    *callback_called = false;
    *fallback_called = false;

    apps::LoadIconFromFileWithFallback(
        apps::mojom::IconType::kUncompressed, 200, GetPath(),
        apps::IconEffects::kNone,
        base::BindOnce(
            [](bool* called, apps::mojom::IconValuePtr* result,
               base::OnceClosure quit, apps::mojom::IconValuePtr icon) {
              *called = true;
              *result = std::move(icon);
              std::move(quit).Run();
            },
            base::Unretained(callback_called), base::Unretained(result),
            run_loop_.QuitClosure()),
        base::BindOnce(
            [](bool* called, apps::mojom::IconValuePtr response,
               apps::mojom::Publisher::LoadIconCallback callback) {
              *called = true;
              std::move(callback).Run(std::move(response));
            },
            base::Unretained(fallback_called), std::move(fallback_response)));

    run_loop_.Run();
  }

  std::string GetPngData(const std::string file_name) {
    base::FilePath base_path;
    std::string png_data_as_string;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
    base::FilePath icon_file_path = base_path.AppendASCII("components")
                                        .AppendASCII("test")
                                        .AppendASCII("data")
                                        .AppendASCII("arc")
                                        .AppendASCII(file_name);
    CHECK(base::PathExists(icon_file_path));
    CHECK(base::ReadFileToString(icon_file_path, &png_data_as_string));
    return png_data_as_string;
  }

  void RunLoadIconFromCompressedData(const std::string png_data_as_string,
                                     apps::mojom::IconType icon_type,
                                     apps::IconEffects icon_effects,
                                     apps::mojom::IconValuePtr& output_icon) {
    apps::LoadIconFromCompressedData(
        icon_type, kSizeInDip, icon_effects, png_data_as_string,
        base::BindOnce(
            [](apps::mojom::IconValuePtr* result,
               base::OnceClosure load_app_icon_callback,
               apps::mojom::IconValuePtr icon) {
              *result = std::move(icon);
              std::move(load_app_icon_callback).Run();
            },
            &output_icon, run_loop_.QuitClosure()));
    run_loop_.Run();

    ASSERT_FALSE(output_icon.is_null());
    ASSERT_EQ(icon_type, output_icon->icon_type);
    ASSERT_FALSE(output_icon->is_placeholder_icon);
    ASSERT_FALSE(output_icon->uncompressed.isNull());

    EnsureRepresentationsLoaded(output_icon->uncompressed);
  }

  void GenerateIconFromCompressedData(const std::string& compressed_icon,
                                      float scale,
                                      gfx::ImageSkia& output_image_skia) {
    std::vector<uint8_t> compressed_data(compressed_icon.begin(),
                                         compressed_icon.end());
    SkBitmap decoded;
    ASSERT_TRUE(gfx::PNGCodec::Decode(compressed_data.data(),
                                      compressed_data.size(), &decoded));

    output_image_skia = gfx::ImageSkia::CreateFromBitmap(decoded, scale);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
      output_image_skia = apps::CreateStandardIconImage(output_image_skia);
    }
#endif
    EnsureRepresentationsLoaded(output_image_skia);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void RunLoadIconFromResource(apps::mojom::IconType icon_type,
                               apps::IconEffects icon_effects,
                               apps::mojom::IconValuePtr& output_icon) {
    bool is_placeholder_icon = false;
    apps::LoadIconFromResource(icon_type, kSizeInDip, IDR_LOGO_CROSTINI_DEFAULT,
                               is_placeholder_icon, icon_effects,
                               base::BindOnce(
                                   [](apps::mojom::IconValuePtr* result,
                                      base::OnceClosure load_app_icon_callback,
                                      apps::mojom::IconValuePtr icon) {
                                     *result = std::move(icon);
                                     std::move(load_app_icon_callback).Run();
                                   },
                                   &output_icon, run_loop_.QuitClosure()));
    run_loop_.Run();
  }

  void GenerateCrostiniPenguinIcon(gfx::ImageSkia& output_image_skia) {
    output_image_skia =
        *(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_LOGO_CROSTINI_DEFAULT));
    output_image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
        output_image_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kSizeInDip, kSizeInDip));

    EnsureRepresentationsLoaded(output_image_skia);
  }

  void GenerateCrostiniPenguinCompressedIcon(std::vector<uint8_t>& output) {
    base::StringPiece data =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_LOGO_CROSTINI_DEFAULT);
    output = std::vector<uint8_t>(data.begin(), data.end());
  }
#endif

 protected:
  content::BrowserTaskEnvironment task_env_{};
  base::ScopedTempDir tmp_dir_{};
  base::RunLoop run_loop_{};
};

TEST_F(AppIconFactoryTest, LoadFromFileSuccess) {
  gfx::ImageSkia image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));
  const SkBitmap* bitmap = image.bitmap();
  cc::WritePNGFile(*bitmap, GetPath(), /*discard_transparency=*/false);

  apps::mojom::IconValuePtr fallback_response, result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(fallback_called);
  EXPECT_FALSE(result.is_null());

  EXPECT_TRUE(
      cc::MatchesBitmap(*bitmap, *result->uncompressed.bitmap(),
                        cc::ExactPixelComparator(/*discard_alpha=*/false)));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallback) {
  apps::mojom::IconValuePtr fallback_response = apps::mojom::IconValue::New();
  // Create a non-null image so we can check if we get the same image back.
  fallback_response->uncompressed =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));

  apps::mojom::IconValuePtr result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
  EXPECT_TRUE(result->uncompressed.BackedBySameObjectAs(
      fallback_response->uncompressed));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackFailure) {
  apps::mojom::IconValuePtr fallback_response, result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
  EXPECT_TRUE(fallback_response.is_null());
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackDoesNotReturn) {
  apps::mojom::IconValuePtr result;
  bool callback_called = false, fallback_called = false;

  apps::LoadIconFromFileWithFallback(
      apps::mojom::IconType::kUncompressed, 200, GetPath(),
      apps::IconEffects::kNone,
      base::BindOnce(
          [](bool* called, apps::mojom::IconValuePtr* result,
             base::OnceClosure quit, apps::mojom::IconValuePtr icon) {
            *called = true;
            *result = std::move(icon);
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), base::Unretained(&result),
          run_loop_.QuitClosure()),
      base::BindOnce(
          [](bool* called, apps::mojom::Publisher::LoadIconCallback callback) {
            *called = true;
            // Drop the callback here, like a buggy fallback might.
          },
          base::Unretained(&fallback_called)));

  run_loop_.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
}

TEST_F(AppIconFactoryTest, LoadIconFromCompressedData) {
  std::string png_data_as_string = GetPngData("icon_100p.png");

  auto icon_type = apps::mojom::IconType::kUncompressed;
  auto icon_effects = apps::IconEffects::kNone;
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_type = apps::mojom::IconType::kStandard;
    icon_effects = apps::IconEffects::kCrOsStandardIcon;
  }

  apps::mojom::IconValuePtr result;
  RunLoadIconFromCompressedData(png_data_as_string, icon_type, icon_effects,
                                result);

  float scale = 1.0;
  gfx::ImageSkia src_image_skia;
  GenerateIconFromCompressedData(png_data_as_string, scale, src_image_skia);

  ASSERT_FALSE(src_image_skia.isNull());
  ASSERT_TRUE(src_image_skia.HasRepresentation(scale));
  ASSERT_TRUE(result->uncompressed.HasRepresentation(scale));
  ASSERT_TRUE(gfx::test::AreBitmapsEqual(
      src_image_skia.GetRepresentation(scale).GetBitmap(),
      result->uncompressed.GetRepresentation(scale).GetBitmap()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AppIconFactoryTest, LoadCrostiniPenguinIcon) {
  auto icon_type = apps::mojom::IconType::kUncompressed;
  auto icon_effects = apps::IconEffects::kNone;
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_type = apps::mojom::IconType::kStandard;
    icon_effects = apps::IconEffects::kCrOsStandardIcon;
  }

  apps::mojom::IconValuePtr result;
  RunLoadIconFromResource(icon_type, icon_effects, result);

  EXPECT_FALSE(result.is_null());
  EXPECT_EQ(icon_type, result->icon_type);
  EXPECT_FALSE(result->is_placeholder_icon);

  EnsureRepresentationsLoaded(result->uncompressed);

  gfx::ImageSkia src_image_skia;
  GenerateCrostiniPenguinIcon(src_image_skia);

  VerifyIcon(src_image_skia, result->uncompressed);
}

TEST_F(AppIconFactoryTest, LoadCrostiniPenguinCompressedIcon) {
  auto icon_effects = apps::IconEffects::kNone;
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effects = apps::IconEffects::kCrOsStandardIcon;
  }

  apps::mojom::IconValuePtr result;
  RunLoadIconFromResource(apps::mojom::IconType::kCompressed, icon_effects,
                          result);

  std::vector<uint8_t> src_data;
  GenerateCrostiniPenguinCompressedIcon(src_data);

  VerifyCompressedIcon(src_data, result);
}

TEST_F(AppIconFactoryTest, ArcNonAdaptiveIconToImageSkia) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New(
      false,
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()),
      std::vector<uint8_t>(), std::vector<uint8_t>());

  bool callback_called = false;
  apps::ArcRawIconPngDataToImageSkia(
      std::move(icon), 100,
      base::BindOnce(
          [](bool* called, base::OnceClosure quit,
             const gfx::ImageSkia& image) {
            if (!image.isNull()) {
              *called = true;
            }
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), run_loop_.QuitClosure()));

  run_loop_.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(AppIconFactoryTest, ArcAdaptiveIconToImageSkia) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New(
      true, std::vector<uint8_t>(),
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()),
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()));

  bool callback_called = false;
  apps::ArcRawIconPngDataToImageSkia(
      std::move(icon), 100,
      base::BindOnce(
          [](bool* called, base::OnceClosure quit,
             const gfx::ImageSkia& image) {
            if (!image.isNull()) {
              *called = true;
            }
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), run_loop_.QuitClosure()));

  run_loop_.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(AppIconFactoryTest, ArcActivityIconsToImageSkias) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  std::vector<arc::mojom::ActivityIconPtr> icons;
  icons.emplace_back(
      arc::mojom::ActivityIcon::New(arc::mojom::ActivityName::New("p0", "a0"),
                                    100, 100, std::vector<uint8_t>()));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p0", "a0"), 100, 100,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          false,
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(), std::vector<uint8_t>())));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p0", "a0"), 201, 201,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          false,
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(), std::vector<uint8_t>())));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p1", "a1"), 100, 100,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          true, std::vector<uint8_t>(),
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()))));

  std::vector<gfx::ImageSkia> result;
  bool callback_called = false;
  apps::ArcActivityIconsToImageSkias(
      icons, base::BindOnce(
                 [](bool* called, std::vector<gfx::ImageSkia>* result,
                    base::OnceClosure quit,
                    const std::vector<gfx::ImageSkia>& images) {
                   *called = true;
                   for (auto image : images) {
                     result->emplace_back(image);
                   }
                   std::move(quit).Run();
                 },
                 base::Unretained(&callback_called), base::Unretained(&result),
                 run_loop_.QuitClosure()));
  run_loop_.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(4U, result.size());
  if (result.size() == 4U) {
    EXPECT_TRUE(result[0].isNull());
    EXPECT_FALSE(result[1].isNull());
    EXPECT_TRUE(result[2].isNull());
    EXPECT_FALSE(result[3].isNull());
  }
}
#endif

class WebAppIconFactoryTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_app_provider_ = web_app::WebAppProvider::Get(profile());
    ASSERT_TRUE(web_app_provider_);

    base::RunLoop run_loop;
    web_app_provider_->registry_controller().AsWebAppSyncBridge()->Init(
        run_loop.QuitClosure());
    run_loop.Run();

    icon_manager_ = static_cast<web_app::WebAppIconManager*>(
        &(web_app_provider_->icon_manager()));
    ASSERT_TRUE(icon_manager_);

    sync_bridge_ =
        web_app_provider_->registry_controller().AsWebAppSyncBridge();
    ASSERT_TRUE(sync_bridge_);
  }

  std::unique_ptr<web_app::WebApp> CreateWebApp() {
    const GURL app_url("https://example.com/path");
    const std::string app_id = web_app::GenerateAppIdFromURL(app_url);

    auto web_app = std::make_unique<web_app::WebApp>(app_id);
    web_app->AddSource(web_app::Source::kSync);
    web_app->SetDisplayMode(web_app::DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(web_app::DisplayMode::kStandalone);
    web_app->SetName("Name");
    web_app->SetStartUrl(app_url);

    return web_app;
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
    icon_manager_->WriteData(app_id, std::move(icon_bitmaps),
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
                          gfx::ImageSkia& output_image_skia) {
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

    extensions::ChromeAppIcon::ResizeFunction resize_function;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
      if (purpose == IconPurpose::ANY) {
        output_image_skia = apps::CreateStandardIconImage(output_image_skia);
      }
      if (purpose == IconPurpose::MASKABLE) {
        output_image_skia = apps::ApplyBackgroundAndMask(output_image_skia);
      }
    } else {
      resize_function =
          base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
    }
#endif

    extensions::ChromeAppIcon::ApplyEffects(
        kSizeInDip, resize_function, true /* app_launchable */,
        true /* from_bookmark */, extensions::ChromeAppIcon::Badge::kNone,
        &output_image_skia);

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

  void LoadIconFromWebApp(const std::string& app_id,
                          apps::IconEffects icon_effects,
                          gfx::ImageSkia& output_image_skia) {
    base::RunLoop run_loop;

    auto icon_type = apps::mojom::IconType::kUncompressed;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
      icon_type = apps::mojom::IconType::kStandard;
    }
#endif

    apps::LoadIconFromWebApp(
        profile(), icon_type, kSizeInDip, app_id, icon_effects,
        base::BindOnce(
            [](gfx::ImageSkia* image, base::OnceClosure load_app_icon_callback,
               apps::mojom::IconValuePtr icon) {
              *image = icon->uncompressed;
              std::move(load_app_icon_callback).Run();
            },
            &output_image_skia, run_loop.QuitClosure()));
    run_loop.Run();

    EnsureRepresentationsLoaded(output_image_skia);
  }

  void LoadCompressedIconBlockingFromWebApp(
      const std::string& app_id,
      apps::IconEffects icon_effects,
      apps::mojom::IconValuePtr& output_data) {
    base::RunLoop run_loop;

    apps::LoadIconFromWebApp(profile(), apps::mojom::IconType::kCompressed,
                             kSizeInDip, app_id, icon_effects,
                             base::BindOnce(
                                 [](apps::mojom::IconValuePtr* result,
                                    base::OnceClosure load_app_icon_callback,
                                    apps::mojom::IconValuePtr icon) {
                                   *result = std::move(icon);
                                   std::move(load_app_icon_callback).Run();
                                 },
                                 &output_data, run_loop.QuitClosure()));
    run_loop.Run();
  }

  web_app::WebAppIconManager& icon_manager() { return *icon_manager_; }

  web_app::WebAppProvider& web_app_provider() { return *web_app_provider_; }

  web_app::WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

 private:
  web_app::WebAppProvider* web_app_provider_;
  web_app::WebAppIconManager* icon_manager_;
  web_app::WebAppSyncBridge* sync_bridge_;
};

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIcon) {
  auto web_app = CreateWebApp();
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
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effect |= apps::IconEffects::kCrOsStandardIcon;
  } else {
    icon_effect |= apps::IconEffects::kResizeAndPad;
  }
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableCompressedIcon) {
  auto web_app = CreateWebApp();
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

  apps::mojom::IconValuePtr icon;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effect |= apps::IconEffects::kCrOsStandardIcon;
  } else {
    icon_effect |= apps::IconEffects::kResizeAndPad;
  }
#endif

  LoadCompressedIconBlockingFromWebApp(app_id, icon_effect, icon);

  VerifyCompressedIcon(src_data, icon);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableIcon) {
  auto web_app = CreateWebApp();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    ASSERT_TRUE(
        icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

    gfx::ImageSkia src_image_skia;
    GenerateWebAppIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                       {{1.0, kIconSize2}, {2.0, kIconSize2}}, src_image_skia);

    gfx::ImageSkia dst_image_skia;
    LoadIconFromWebApp(app_id,
                       apps::IconEffects::kRoundCorners |
                           apps::IconEffects::kCrOsStandardBackground |
                           apps::IconEffects::kCrOsStandardMask,
                       dst_image_skia);
    VerifyIcon(src_image_skia, dst_image_skia);
    return;
  }
#endif

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize1}));

  gfx::ImageSkia src_image_skia;
  GenerateWebAppIcon(app_id, IconPurpose::ANY, {kIconSize1},
                     {{1.0, kIconSize1}, {2.0, kIconSize1}}, src_image_skia);

  gfx::ImageSkia dst_image_skia;
  LoadIconFromWebApp(app_id, apps::IconEffects::kRoundCorners, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadMaskableCompressedIcon) {
  auto web_app = CreateWebApp();
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
  apps::mojom::IconValuePtr icon;
  apps::IconEffects icon_effect = apps::IconEffects::kRoundCorners;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effect |= apps::IconEffects::kCrOsStandardBackground |
                   apps::IconEffects::kCrOsStandardMask;
    ASSERT_TRUE(
        icon_manager().HasIcons(app_id, IconPurpose::MASKABLE, {kIconSize2}));

    GenerateWebAppCompressedIcon(app_id, IconPurpose::MASKABLE, {kIconSize2},
                                 {{1.0, kIconSize2}, {2.0, kIconSize2}},
                                 src_data);

    LoadCompressedIconBlockingFromWebApp(app_id, icon_effect, icon);
    VerifyCompressedIcon(src_data, icon);
    return;
  }

  icon_effect |= apps::IconEffects::kResizeAndPad;
#endif

  ASSERT_TRUE(icon_manager().HasIcons(app_id, IconPurpose::ANY, {kIconSize1}));

  GenerateWebAppCompressedIcon(app_id, IconPurpose::ANY, {kIconSize1},
                               {{1.0, kIconSize1}, {2.0, kIconSize1}},
                               src_data);

  LoadCompressedIconBlockingFromWebApp(app_id, icon_effect, icon);

  VerifyCompressedIcon(src_data, icon);
}

TEST_F(WebAppIconFactoryTest, LoadNonMaskableIconWithMaskableIcon) {
  auto web_app = CreateWebApp();
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
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effect |= apps::IconEffects::kCrOsStandardIcon;
  } else {
    icon_effect |= apps::IconEffects::kResizeAndPad;
  }
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadSmallMaskableIcon) {
  auto web_app = CreateWebApp();
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
  auto web_app = CreateWebApp();
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
  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    icon_effect |= apps::IconEffects::kCrOsStandardIcon;
  } else {
    icon_effect |= apps::IconEffects::kResizeAndPad;
  }
#endif

  LoadIconFromWebApp(app_id, icon_effect, dst_image_skia);

  VerifyIcon(src_image_skia, dst_image_skia);
}

TEST_F(WebAppIconFactoryTest, LoadIconFailed) {
  auto web_app = CreateWebApp();
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

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (auto& scale_factor : scale_factors) {
    const float scale = ui::GetScaleForScaleFactor(scale_factor);
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

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  // Expects 32px and 64px to be chosen for 32dip-normal and 32dip-hi-DPI (2.0f
  // scale).
  const std::vector<SkColor> expected_colors{SK_ColorRED, SK_ColorGREEN};

  for (int i = 0; i < scale_factors.size(); ++i) {
    const float scale = ui::GetScaleForScaleFactor(scale_factors[i]);
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
      icon_bitmaps, /*icon_effects=*/apps::IconEffects::kCrOsStandardIcon,
      /*size_hint_in_dip=*/32);

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ASSERT_EQ(2U, scale_factors.size());

  for (int i = 0; i < scale_factors.size(); ++i) {
    const float scale = ui::GetScaleForScaleFactor(scale_factors[i]);
    ASSERT_TRUE(converted_image.HasRepresentation(scale));

    // No colour in the upper left corner.
    EXPECT_FALSE(
        converted_image.GetRepresentation(scale).GetBitmap().getColor(0, 0));

    const SquareSizePx center_px = sizes_px[i] / 2;
    EXPECT_EQ(colors[i],
              converted_image.GetRepresentation(scale).GetBitmap().getColor(
                  center_px, center_px));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
