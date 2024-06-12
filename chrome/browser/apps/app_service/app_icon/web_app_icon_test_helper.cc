// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/web_app_icon_test_helper.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/icon_standardizer.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif

namespace apps {

using IconPurpose = web_app::IconPurpose;

WebAppIconTestHelper::WebAppIconTestHelper(Profile* profile)
    : profile_(profile) {}
WebAppIconTestHelper::~WebAppIconTestHelper() = default;

void WebAppIconTestHelper::WriteIcons(const std::string& app_id,
                                      const std::vector<IconPurpose>& purposes,
                                      const std::vector<int>& sizes_px,
                                      const std::vector<SkColor>& colors) {
  ASSERT_EQ(sizes_px.size(), colors.size());
  ASSERT_TRUE(!purposes.empty());

  web_app::IconBitmaps icon_bitmaps;
  for (size_t i = 0; i < sizes_px.size(); ++i) {
    if (base::Contains(purposes, IconPurpose::ANY)) {
      web_app::AddGeneratedIcon(&icon_bitmaps.any, sizes_px[i], colors[i]);
    }
    if (base::Contains(purposes, IconPurpose::MASKABLE)) {
      web_app::AddGeneratedIcon(&icon_bitmaps.maskable, sizes_px[i], colors[i]);
    }
  }

  base::test::TestFuture<bool> future;
  icon_manager().WriteData(app_id, std::move(icon_bitmaps), {}, {},
                           future.GetCallback());
  bool success = future.Get();
  EXPECT_TRUE(success);
}

gfx::ImageSkia WebAppIconTestHelper::GenerateWebAppIcon(
    const std::string& app_id,
    IconPurpose purpose,
    const std::vector<int>& sizes_px,
    apps::ScaleToSize scale_to_size_in_px,
    bool skip_icon_effects) {
  base::test::TestFuture<std::map<web_app::SquareSizePx, SkBitmap>> future;
  icon_manager().ReadIcons(app_id, purpose, sizes_px, future.GetCallback());
  auto icon_bitmaps = future.Take();

  gfx::ImageSkia output_image_skia;

  for (auto [scale, size_in_px] : scale_to_size_in_px) {
    int icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(kSizeInDip, kSizeInDip), scale)
            .width();

    SkBitmap bitmap = icon_bitmaps[size_in_px];
    if (bitmap.width() != icon_size_in_px) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3, icon_size_in_px,
          icon_size_in_px);
    }
    output_image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  if (!skip_icon_effects) {
    extensions::ChromeAppIcon::ResizeFunction resize_function;
    if (purpose == IconPurpose::ANY) {
      output_image_skia = apps::CreateStandardIconImage(output_image_skia);
    }
    if (purpose == IconPurpose::MASKABLE) {
      output_image_skia = apps::ApplyBackgroundAndMask(output_image_skia);
    }

    extensions::ChromeAppIcon::ApplyEffects(
        kSizeInDip, resize_function, /*app_launchable=*/true,
        /*rounded_corners=*/true, extensions::ChromeAppIcon::Badge::kNone,
        &output_image_skia);
  }

  EnsureRepresentationsLoaded(output_image_skia);

  return output_image_skia;
}

std::vector<uint8_t> WebAppIconTestHelper::GenerateWebAppCompressedIcon(
    const std::string& app_id,
    IconPurpose purpose,
    const std::vector<int>& sizes_px,
    apps::ScaleToSize scale_to_size_in_px) {
  gfx::ImageSkia image_skia =
      GenerateWebAppIcon(app_id, purpose, sizes_px, scale_to_size_in_px);

  const float scale = 1.0;
  const gfx::ImageSkiaRep& image_skia_rep = image_skia.GetRepresentation(scale);
  CHECK_EQ(image_skia_rep.scale(), scale);

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  const bool discard_transparency = false;
  std::vector<uint8_t> result;
  CHECK(
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency, &result));
  return result;
}

std::vector<uint8_t> WebAppIconTestHelper::GenerateWebAppCompressedIcon(
    const std::string& app_id,
    IconPurpose purpose,
    IconEffects icon_effects,
    const std::vector<int>& sizes_px,
    apps::ScaleToSize scale_to_size_in_px,
    float scale) {
  gfx::ImageSkia image_skia =
      GenerateWebAppIcon(app_id, purpose, sizes_px, scale_to_size_in_px,
                         /*skip_icon_effects=*/true);

  if (icon_effects != apps::IconEffects::kNone) {
    base::test::TestFuture<apps::IconValuePtr> iv_with_icon_effects;
    auto iv = std::make_unique<apps::IconValue>();
    iv->icon_type = apps::IconType::kUncompressed;
    iv->uncompressed = image_skia;
    apps::ApplyIconEffects(profile_, app_id, icon_effects, kSizeInDip,
                           std::move(iv), iv_with_icon_effects.GetCallback());
    image_skia = iv_with_icon_effects.Take()->uncompressed;
  }

  const gfx::ImageSkiaRep& image_skia_rep = image_skia.GetRepresentation(scale);
  CHECK_EQ(image_skia_rep.scale(), scale);

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  const bool discard_transparency = false;
  std::vector<uint8_t> result;
  CHECK(
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency, &result));
  return result;
}

void WebAppIconTestHelper::RegisterApp(
    std::unique_ptr<web_app::WebApp> web_app) {
  web_app::ScopedRegistryUpdate update =
      web_app::WebAppProvider::GetForWebApps(profile_)
          ->sync_bridge_unsafe()
          .BeginUpdate();
  update->CreateApp(std::move(web_app));
}

web_app::WebAppIconManager& WebAppIconTestHelper::icon_manager() {
  return web_app::WebAppProvider::GetForWebApps(profile_)->icon_manager();
}
}  // namespace apps
