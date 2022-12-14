// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

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
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#endif

namespace apps {
class AppIconFactoryTest : public testing::Test {
 public:
  base::FilePath GetPath() {
    return tmp_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe("icon.file"));
  }

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  void RunLoadIconFromFileWithFallback(apps::IconValuePtr fallback_response,
                                       bool* callback_called,
                                       bool* fallback_called,
                                       apps::IconValuePtr* result) {
    *callback_called = false;
    *fallback_called = false;

    apps::LoadIconFromFileWithFallback(
        apps::IconType::kUncompressed, 200, GetPath(), apps::IconEffects::kNone,
        base::BindOnce(
            [](bool* called, apps::IconValuePtr* result, base::OnceClosure quit,
               apps::IconValuePtr icon) {
              *called = true;
              *result = std::move(icon);
              std::move(quit).Run();
            },
            base::Unretained(callback_called), base::Unretained(result),
            run_loop_.QuitClosure()),
        base::BindOnce(
            [](bool* called, apps::IconValuePtr response,
               apps::LoadIconCallback callback) {
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
    base::FilePath icon_file_path = base_path.AppendASCII("ash")
                                        .AppendASCII("components")
                                        .AppendASCII("arc")
                                        .AppendASCII("test")
                                        .AppendASCII("data")
                                        .AppendASCII("icons")
                                        .AppendASCII(file_name);
    CHECK(base::PathExists(icon_file_path));
    CHECK(base::ReadFileToString(icon_file_path, &png_data_as_string));
    return png_data_as_string;
  }

  void RunLoadIconFromCompressedData(const std::string png_data_as_string,
                                     apps::IconType icon_type,
                                     apps::IconEffects icon_effects,
                                     apps::IconValuePtr& output_icon) {
    apps::LoadIconFromCompressedData(
        icon_type, kSizeInDip, icon_effects, png_data_as_string,
        base::BindOnce(
            [](apps::IconValuePtr* result,
               base::OnceClosure load_app_icon_callback,
               apps::IconValuePtr icon) {
              *result = std::move(icon);
              std::move(load_app_icon_callback).Run();
            },
            &output_icon, run_loop_.QuitClosure()));
    run_loop_.Run();

    ASSERT_TRUE(output_icon);
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
    output_image_skia = apps::CreateStandardIconImage(output_image_skia);
#endif
    EnsureRepresentationsLoaded(output_image_skia);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::IconValuePtr RunLoadIconFromResource(apps::IconType icon_type,
                                             apps::IconEffects icon_effects) {
    bool is_placeholder_icon = false;
    apps::IconValuePtr icon_value;
    apps::LoadIconFromResource(
        icon_type, kSizeInDip, IDR_LOGO_CROSTINI_DEFAULT, is_placeholder_icon,
        icon_effects, base::BindLambdaForTesting([&](apps::IconValuePtr icon) {
          icon_value = std::move(icon);
          run_loop_.Quit();
        }));
    run_loop_.Run();
    return icon_value;
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

  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(std::move(fallback_response),
                                  &callback_called, &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(fallback_called);
  ASSERT_TRUE(result);

  EXPECT_TRUE(
      cc::MatchesBitmap(*bitmap, *result->uncompressed.bitmap(),
                        cc::ExactPixelComparator(/*discard_alpha=*/false)));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallback) {
  auto expect_image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));

  auto fallback_response = std::make_unique<apps::IconValue>();
  fallback_response->icon_type = apps::IconType::kUncompressed;
  // Create a non-null image so we can check if we get the same image back.
  fallback_response->uncompressed = expect_image;

  auto result = std::make_unique<apps::IconValue>();
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(std::move(fallback_response),
                                  &callback_called, &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->uncompressed.BackedBySameObjectAs(expect_image));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackFailure) {
  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(std::move(fallback_response),
                                  &callback_called, &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackDoesNotReturn) {
  auto result = std::make_unique<apps::IconValue>();
  bool callback_called = false, fallback_called = false;

  apps::LoadIconFromFileWithFallback(
      apps::IconType::kUncompressed, 200, GetPath(), apps::IconEffects::kNone,
      base::BindLambdaForTesting([&](apps::IconValuePtr icon) {
        callback_called = true;
        result = std::move(icon);
        run_loop_.Quit();
      }),
      base::BindLambdaForTesting([&](apps::LoadIconCallback callback) {
        fallback_called = true;
        // Drop the callback here, like a buggy fallback might.
      }));

  run_loop_.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
}

TEST_F(AppIconFactoryTest, LoadIconFromCompressedData) {
  std::string png_data_as_string = GetPngData("icon_100p.png");

  auto icon_type = apps::IconType::kStandard;
  auto icon_effects = apps::IconEffects::kCrOsStandardIcon;

  auto result = std::make_unique<apps::IconValue>();
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
  auto icon_type = apps::IconType::kStandard;
  auto icon_effects = apps::IconEffects::kCrOsStandardIcon;

  auto result = RunLoadIconFromResource(icon_type, icon_effects);

  ASSERT_TRUE(result);
  EXPECT_EQ(icon_type, result->icon_type);
  EXPECT_FALSE(result->is_placeholder_icon);

  EnsureRepresentationsLoaded(result->uncompressed);

  gfx::ImageSkia src_image_skia;
  GenerateCrostiniPenguinIcon(src_image_skia);

  VerifyIcon(src_image_skia, result->uncompressed);
}

TEST_F(AppIconFactoryTest, LoadCrostiniPenguinCompressedIcon) {
  auto icon_effects = apps::IconEffects::kNone;
  icon_effects = apps::IconEffects::kCrOsStandardIcon;

  auto result =
      RunLoadIconFromResource(apps::IconType::kCompressed, icon_effects);

  std::vector<uint8_t> src_data;
  GenerateCrostiniPenguinCompressedIcon(src_data);

  VerifyCompressedIcon(src_data, *result);
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
  EXPECT_TRUE(result[0].isNull());
  EXPECT_FALSE(result[1].isNull());
  EXPECT_TRUE(result[2].isNull());
  EXPECT_FALSE(result[3].isNull());

  for (const auto& icon : result) {
    EXPECT_TRUE(icon.IsThreadSafe());
  }
}
#endif

}  // namespace apps
