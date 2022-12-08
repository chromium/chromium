// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

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
    base::FilePath source_install_dir =
        data_dir().AppendASCII("app_list").AppendASCII("Extensions");
    base::FilePath pref_path =
        source_install_dir.DirName().Append(chrome::kPreferencesFilename);
    ExtensionServiceInitParams params;
    InitializeInstalledExtensionService(pref_path, source_install_dir, params);
    service_->Init();

    // Let any async services complete their set-up.
    base::RunLoop().RunUntilIdle();

    // There should be 4 extensions in the test profile.
    ASSERT_EQ(4U, registry()->enabled_extensions().size());
  }

  void GenerateExtensionAppIcon(const std::string app_id,
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    output_image_skia =
        skip_effects
            ? result.Take().AsImageSkia()
            : apps::CreateStandardIconImage(result.Take().AsImageSkia());
#else
    output_image_skia = result.Take().AsImageSkia();
#endif
  }

  void GenerateExtensionAppCompressedIcon(const std::string app_id,
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
      IconType icon_type,
      ui::ResourceScaleFactor scale_factor) {
    base::test::TestFuture<apps::IconValuePtr> result;
    apps::GetChromeAppCompressedIconData(profile(), app_id, icon_type,
                                         kSizeInDip, scale_factor,
                                         result.GetCallback());
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
TEST_F(ChromeAppsIconFactoryTest, GetCompressedIconDataForUncompressedIcon) {
  // Generate the source uncompressed icon for comparing.
  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data1,
                                     /*skip_effects=*/true);
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/2.0, src_data2,
                                     /*skip_effects=*/true);

  IconValuePtr icon1 =
      GetCompressedIconData(kPackagedApp1Id, IconType::kUncompressed,
                            ui::ResourceScaleFactor::k100Percent);
  IconValuePtr icon2 =
      GetCompressedIconData(kPackagedApp1Id, IconType::kUncompressed,
                            ui::ResourceScaleFactor::k200Percent);
  VerifyCompressedIcon(src_data1, *icon1);
  VerifyCompressedIcon(src_data2, *icon2);
}

TEST_F(ChromeAppsIconFactoryTest, GetCompressedIconDataForStandardIcon) {
  // Generate the source uncompressed icon for comparing.
  std::vector<uint8_t> src_data1;
  std::vector<uint8_t> src_data2;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data1,
                                     /*skip_effects=*/true);
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/2.0, src_data2,
                                     /*skip_effects=*/true);

  IconValuePtr icon1 =
      GetCompressedIconData(kPackagedApp1Id, IconType::kStandard,
                            ui::ResourceScaleFactor::k100Percent);
  IconValuePtr icon2 =
      GetCompressedIconData(kPackagedApp1Id, IconType::kStandard,
                            ui::ResourceScaleFactor::k200Percent);
  VerifyCompressedIcon(src_data1, *icon1);
  VerifyCompressedIcon(src_data2, *icon2);
}

TEST_F(ChromeAppsIconFactoryTest, GetCompressedIconDataForCompressedIcon) {
  // Generate the source compressed icon for comparing.
  std::vector<uint8_t> src_data;
  GenerateExtensionAppCompressedIcon(kPackagedApp1Id, /*scale=*/1.0, src_data,
                                     /*skip_effects=*/true);

  IconValuePtr icon =
      GetCompressedIconData(kPackagedApp1Id, IconType::kCompressed,
                            ui::ResourceScaleFactor::k100Percent);
  VerifyCompressedIcon(src_data, *icon);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
