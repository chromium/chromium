// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#endif

namespace apps {

const char kPackagedApp1Id[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

class ChromeAppsIconFactoryTest : public extensions::ExtensionServiceTestBase {
 public:
  ChromeAppsIconFactoryTest() = default;
  ~ChromeAppsIconFactoryTest() override = default;

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    // Load "app_list" extensions test profile.
    // The test profile has 4 extensions:
    // - 1 dummy extension (which should not be visible in the launcher)
    // - 2 packaged extension apps
    // - 1 hosted extension app
    ExtensionServiceInitParams params;
    ASSERT_TRUE(params.ConfigureByTestDataDirectory(
        data_dir().AppendASCII("app_list")));
    InitializeExtensionService(std::move(params));
    service_->Init();

    // Let any async services complete their set-up.
    base::RunLoop().RunUntilIdle();

    // There should be 4 extensions in the test profile.
    ASSERT_EQ(4U, registry()->enabled_extensions().size());
  }

  void GenerateExtensionAppIcon(const std::string& app_id,
                                gfx::ImageSkia& output_image_skia,
                                bool skip_effects = false) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    ASSERT_TRUE(registry);
    const extensions::Extension* extension =
        registry->GetInstalledExtension(app_id);
    ASSERT_TRUE(extension);

    base::test::TestFuture<const gfx::Image&> result;
    extensions::ImageLoader::Get(profile())->LoadImageAtEveryScaleFactorAsync(
        extension, gfx::Size(kSizeInDip, kSizeInDip), result.GetCallback());

    output_image_skia =
        skip_effects
            ? result.Take().AsImageSkia()
            : apps::CreateStandardIconImage(result.Take().AsImageSkia());
  }

  void GenerateExtensionAppCompressedIcon(const std::string& app_id,
                                          float scale,
                                          std::vector<uint8_t>& result,
                                          bool skip_effects = false) {
    gfx::ImageSkia image_skia;
    GenerateExtensionAppIcon(app_id, image_skia, skip_effects);

    const gfx::ImageSkiaRep& image_skia_rep =
        image_skia.GetRepresentation(scale);
    ASSERT_EQ(image_skia_rep.scale(), scale);

    const SkBitmap& bitmap = image_skia_rep.GetBitmap();
    const bool discard_transparency = false;
    ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                  &result));
  }

  IconValuePtr LoadIconFromExtension(const std::string& app_id,
                                     IconType icon_type,
                                     IconEffects icon_effects) {
    base::test::TestFuture<IconValuePtr> result;
    apps::LoadIconFromExtension(icon_type, kSizeInDip, profile(), app_id,
                                icon_effects, result.GetCallback());
    return result.Take();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::IconValuePtr GetCompressedIconData(
      const std::string& app_id,
      ui::ResourceScaleFactor scale_factor) {
    base::test::TestFuture<apps::IconValuePtr> result;
    apps::GetChromeAppCompressedIconData(profile(), app_id, kSizeInDip,
                                         scale_factor, result.GetCallback());
    return result.Take();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(ChromeAppsIconFactoryTest, LoadUncompressedIcon) {
  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateExtensionAppIcon(kPackagedApp1Id, src_image_skia);

  IconValuePtr iv = LoadIconFromExtension(
      kPackagedApp1Id, IconType::kUncompressed, IconEffects::kCrOsStandardIcon);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kUncompressed);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

TEST_F(ChromeAppsIconFactoryTest, LoadStandardIcon) {
  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateExtensionAppIcon(kPackagedApp1Id, src_image_skia);

  IconValuePtr iv = LoadIconFromExtension(kPackagedApp1Id, IconType::kStandard,
                                          IconEffects::kCrOsStandardIcon);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kStandard);
  VerifyIcon(src_image_skia, iv->uncompressed);
}

TEST_F(ChromeAppsIconFactoryTest, LoadCompressedIcon) {
  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data);

  IconValuePtr iv = LoadIconFromExtension(
      kPackagedApp1Id, IconType::kCompressed, IconEffects::kCrOsStandardIcon);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kCompressed);
  VerifyCompressedIcon(src_data, *iv);
}

TEST_F(ChromeAppsIconFactoryTest, LoadCompressedIconWithoutEffect) {
  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data,
                                     /*skip_effects=*/true);

  IconValuePtr iv = LoadIconFromExtension(
      kPackagedApp1Id, IconType::kCompressed, IconEffects::kNone);
  ASSERT_TRUE(iv);
  ASSERT_EQ(iv->icon_type, IconType::kCompressed);
  VerifyCompressedIcon(src_data, *iv);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeAppsIconFactoryTest, GetCompressedIconData) {
  // Generate the source uncompressed icon for comparing.
  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data1,
                                     /*skip_effects=*/true);
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/2.0, src_data2,
                                     /*skip_effects=*/true);

  IconValuePtr icon1 = GetCompressedIconData(
      kPackagedApp1Id, ui::ResourceScaleFactor::k100Percent);
  IconValuePtr icon2 = GetCompressedIconData(
      kPackagedApp1Id, ui::ResourceScaleFactor::k200Percent);
  VerifyCompressedIcon(src_data1, *icon1);
  VerifyCompressedIcon(src_data2, *icon2);
}

class AppServiceChromeAppIconTest : public ChromeAppsIconFactoryTest {
 public:
  void SetUp() override {
    ChromeAppsIconFactoryTest::SetUp();

    proxy_ = AppServiceProxyFactory::GetForProfile(profile());
    fake_icon_loader_ = std::make_unique<apps::FakeIconLoader>(proxy_);
    OverrideAppServiceProxyInnerIconLoader(fake_icon_loader_.get());
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
  std::vector<apps::IconValuePtr> MultipleLoadIconWithIconEffects(
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

  AppServiceProxy& app_service_proxy() { return *proxy_; }

 private:
  raw_ptr<AppServiceProxy> proxy_;
  std::unique_ptr<apps::FakeIconLoader> fake_icon_loader_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AppServiceChromeAppIconTest, GetCompressedIconDataForCompressedIcon) {
  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data,
                                     /*skip_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // compressed icon.
  VerifyCompressedIcon(src_data,
                       *LoadIcon(kPackagedApp1Id, IconType::kCompressed));
}

TEST_F(AppServiceChromeAppIconTest, GetCompressedIconDataForStandardIcon) {
  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateExtensionAppIcon(kPackagedApp1Id, src_image_skia);

  // Verify the icon reading and writing function in AppService for the
  // kStandard icon.
  auto ret = MultipleLoadIconWithIconEffects(
      kPackagedApp1Id, IconEffects::kCrOsStandardIcon, IconType::kStandard);

  ASSERT_EQ(2U, ret.size());
  ASSERT_EQ(apps::IconType::kStandard, ret[0]->icon_type);
  VerifyIcon(src_image_skia, ret[0]->uncompressed);
  ASSERT_EQ(apps::IconType::kStandard, ret[1]->icon_type);
  VerifyIcon(src_image_skia, ret[1]->uncompressed);
}

TEST_F(AppServiceChromeAppIconTest, GetCompressedIconDataForUncompressedIcon) {
  // Generate the source uncompressed icon for comparing.
  gfx::ImageSkia src_image_skia;
  GenerateExtensionAppIcon(kPackagedApp1Id, src_image_skia,
                           /*skip_effects=*/true);

  // Verify the icon reading and writing function in AppService for the
  // kUncompressed icon.
  auto ret = LoadIconWithIconEffects(kPackagedApp1Id, IconEffects::kNone,
                                     IconType::kUncompressed);

  ASSERT_EQ(apps::IconType::kUncompressed, ret->icon_type);
  VerifyIcon(src_image_skia, ret->uncompressed);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
