// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
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
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/md_icon_normalizer.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "components/services/app_service/public/cpp/features.h"
#endif

namespace apps {
class AppIconFactoryTest : public testing::Test {
 public:
  base::FilePath GetPath() {
    return tmp_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe("icon.file"));
  }

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  bool RunLoadIconFromFileWithFallback(apps::IconValuePtr fallback_response,
                                       apps::IconValuePtr* result) {
    bool fallback_called = false;

    base::test::TestFuture<apps::IconValuePtr> success_future;
    apps::LoadIconFromFileWithFallback(
        apps::IconType::kUncompressed, 200, GetPath(), apps::IconEffects::kNone,
        success_future.GetCallback(),
        base::BindLambdaForTesting([&](apps::LoadIconCallback callback) {
          fallback_called = true;
          std::move(callback).Run(std::move(fallback_response));
        }));

    *result = success_future.Take();
    return fallback_called;
  }

  std::string GetPngData(const std::string& file_name) {
    base::FilePath base_path;
    std::string png_data_as_string;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
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

  void RunLoadIconFromCompressedData(const std::string& png_data_as_string,
                                     apps::IconType icon_type,
                                     apps::IconEffects icon_effects,
                                     apps::IconValuePtr& output_icon) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromCompressedData(icon_type, kSizeInDip, icon_effects,
                                     png_data_as_string, future.GetCallback());
    output_icon = future.Take();
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

    output_image_skia = apps::CreateStandardIconImage(output_image_skia);
    EnsureRepresentationsLoaded(output_image_skia);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::IconValuePtr RunLoadIconFromResource(apps::IconType icon_type,
                                             apps::IconEffects icon_effects) {
    base::test::TestFuture<apps::IconValuePtr> future;
    apps::LoadIconFromResource(/*profile=*/nullptr, /*app_id=*/std::nullopt,
                               icon_type, kSizeInDip, IDR_LOGO_CROSTINI_DEFAULT,
                               /*is_placeholder_icon=*/false, icon_effects,
                               future.GetCallback());
    auto icon = future.Take();
    return icon;
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
    std::string_view data =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_LOGO_CROSTINI_DEFAULT);
    output = std::vector<uint8_t>(data.begin(), data.end());
  }

  void GeneratePlayStoreIcon(gfx::ImageSkia& output_image_skia) {
    output_image_skia =
        *(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_ARC_SUPPORT_ICON_192_PNG));
    output_image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
        output_image_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kSizeInDip, kSizeInDip));
    EnsureRepresentationsLoaded(output_image_skia);
  }
#endif

 protected:
  content::BrowserTaskEnvironment task_env_;
  base::ScopedTempDir tmp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AppIconFactoryTest, LoadFromFileSuccess) {
  gfx::ImageSkia image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));
  const SkBitmap* bitmap = image.bitmap();
  cc::WritePNGFile(*bitmap, GetPath(), /*discard_transparency=*/false);

  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_FALSE(fallback_called);
  ASSERT_TRUE(result);

  EXPECT_TRUE(cc::MatchesBitmap(*bitmap, *result->uncompressed.bitmap(),
                                cc::ExactPixelComparator()));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallback) {
  auto expect_image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));

  auto fallback_response = std::make_unique<apps::IconValue>();
  fallback_response->icon_type = apps::IconType::kUncompressed;
  // Create a non-null image so we can check if we get the same image back.
  fallback_response->uncompressed = expect_image;

  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->uncompressed.BackedBySameObjectAs(expect_image));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackFailure) {
  auto fallback_response = std::make_unique<apps::IconValue>();
  auto result = std::make_unique<apps::IconValue>();
  bool fallback_called =
      RunLoadIconFromFileWithFallback(std::move(fallback_response), &result);
  EXPECT_TRUE(fallback_called);
  ASSERT_TRUE(result);
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackDoesNotReturn) {
  base::test::TestFuture<apps::IconValuePtr> success_future;

  bool fallback_called = false;
  apps::LoadIconFromFileWithFallback(
      apps::IconType::kUncompressed, /*size_hint_in_dip=*/200, GetPath(),
      apps::IconEffects::kNone, success_future.GetCallback(),
      base::BindLambdaForTesting([&](apps::LoadIconCallback) {
        // Drop the callback here, like a buggy fallback might.
        fallback_called = true;
      }));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(fallback_called);
  auto result = success_future.Take();
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

  base::test::TestFuture<const gfx::ImageSkia&> future;
  apps::ArcRawIconPngDataToImageSkia(std::move(icon), 100,
                                     future.GetCallback());
  auto image = future.Take();
  EXPECT_TRUE(!image.isNull());
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

  base::test::TestFuture<const gfx::ImageSkia&> future;
  apps::ArcRawIconPngDataToImageSkia(std::move(icon), 100,
                                     future.GetCallback());
  auto image = future.Take();
  EXPECT_TRUE(!image.isNull());
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

  base::test::TestFuture<const std::vector<gfx::ImageSkia>&> future;
  apps::ArcActivityIconsToImageSkias(icons, future.GetCallback());

  auto result = future.Take();
  EXPECT_EQ(4U, result.size());
  EXPECT_TRUE(result[0].isNull());
  EXPECT_FALSE(result[1].isNull());
  EXPECT_TRUE(result[2].isNull());
  EXPECT_FALSE(result[3].isNull());

  for (const auto& icon : result) {
    EXPECT_TRUE(icon.IsThreadSafe());
  }
}

class AppServiceAppIconTest : public AppIconFactoryTest {
 public:
  void SetUp() override {
    AppIconFactoryTest::SetUp();

    ash::CiceroneClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());

    crostini_test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());
    crostini_test_helper_->ReInitializeAppServiceIntegration();

    fake_publisher_ =
        std::make_unique<apps::FakePublisherForIconTest>(proxy_, AppType::kArc);
  }

  void TearDown() override {
    crostini_test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
  }

  void OverrideAppServiceProxyInnerIconLoader(apps::IconLoader* icon_loader) {
    app_service_proxy().OverrideInnerIconLoaderForTesting(icon_loader);
  }

  void AddApp(const std::string& app_id, AppType app_type) {
    std::vector<AppPtr> deltas;
    deltas.push_back(std::make_unique<App>(app_type, app_id));
    proxy_->OnApps(std::move(deltas), app_type,
                   /*should_notify_initialized=*/false);
  }

  // Set up the test Crostini app.
  std::string AddCrostiniApp(std::string app_id, std::string app_name) {
    vm_tools::apps::App app;
    app.set_desktop_file_id(app_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_name);
    crostini_test_helper_->AddApp(app);

    return crostini::CrostiniTestHelper::GenerateAppId(
        app.desktop_file_id(), crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName);
  }

  apps::IconValuePtr LoadIconFromIconKey(const std::string& app_id,
                                         const IconKey& icon_key,
                                         IconType icon_type) {
    base::test::TestFuture<apps::IconValuePtr> result;
    app_service_proxy().app_icon_loader()->LoadIconFromIconKey(
        app_id, icon_key, icon_type, kSizeInDip,
        /*allow_placeholder_icon=*/false, result.GetCallback());
    return result.Take();
  }

  AppServiceProxy& app_service_proxy() { return *proxy_; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AppServiceProxy, DanglingUntriaged> proxy_;
  std::unique_ptr<apps::FakePublisherForIconTest> fake_publisher_;

  std::unique_ptr<crostini::CrostiniTestHelper> crostini_test_helper_;

  base::WeakPtrFactory<AppServiceAppIconTest> weak_ptr_factory_{this};
};

TEST_F(AppServiceAppIconTest, GetCrostiniPenguinIcon) {
  gfx::ImageSkia src_image_skia;
  GenerateCrostiniPenguinIcon(src_image_skia);

  std::string app_id = AddCrostiniApp("app_id", "app_name");

  // Verify the icon reading function in AppService for the default Crostini
  // penguin icon by calling LoadIconFromResource.
  IconKey icon_key;
  icon_key.resource_id = IDR_LOGO_CROSTINI_DEFAULT;
  icon_key.icon_effects = apps::IconEffects::kCrOsStandardIcon;
  auto iv = LoadIconFromIconKey(app_id, icon_key, IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  EnsureRepresentationsLoaded(iv->uncompressed);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

TEST_F(AppServiceAppIconTest, GetCrostiniPenguinCompressedIcon) {
  std::vector<uint8_t> src_data;
  GenerateCrostiniPenguinCompressedIcon(src_data);

  std::string app_id = AddCrostiniApp("app_id", "app_name");

  // Verify the icon reading function in AppService for the default Crostini
  // penguin icon by calling LoadIconFromResource.
  IconKey icon_key;
  icon_key.resource_id = IDR_LOGO_CROSTINI_DEFAULT;
  icon_key.icon_effects = apps::IconEffects::kCrOsStandardIcon;
  auto iv = LoadIconFromIconKey(app_id, icon_key, IconType::kCompressed);
  VerifyCompressedIcon(src_data, *iv);
}

TEST_F(AppServiceAppIconTest, GetPlayStoreIcon) {
  gfx::ImageSkia src_image_skia;
  GeneratePlayStoreIcon(src_image_skia);

  AddApp(arc::kPlayStoreAppId, AppType::kArc);

  // Verify the icon reading function in AppService for the Play Store icon by
  // calling LoadIconFromResource.
  auto iv =
      LoadIconFromIconKey(arc::kPlayStoreAppId, IconKey(), IconType::kStandard);
  ASSERT_EQ(apps::IconType::kStandard, iv->icon_type);
  EnsureRepresentationsLoaded(iv->uncompressed);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

#endif

}  // namespace apps
