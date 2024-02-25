// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_decoder.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace apps {

namespace {
constexpr char kAppId[] = "foobar";
constexpr int kIconSizeDp = 64;
}  // namespace

class AppIconDecoderTest : public testing::Test {
 public:
  // The type of compressed icon file, which determines the name of the file the
  // icon is stored in.
  enum class StoredIconType {
    kNonMaskable,
    kMaskable,
    kAdaptiveBackground,
    kAdaptiveForeground
  };

  // Stores icons for all scale factors for the given `app_id` and `size_dp` in
  // the App Service icon directory. The icons will be a solid block of the
  // given `color.
  void StoreSolidColorIconsForApp(const std::string& app_id,
                                  int size_dp,
                                  StoredIconType type,
                                  SkColor color) {
    for (const auto scale_factor : ui::GetSupportedResourceScaleFactors()) {
      int icon_size_in_px =
          gfx::ScaleToFlooredSize(gfx::Size(size_dp, size_dp), scale_factor)
              .width();
      SkBitmap bitmap = gfx::test::CreateBitmap(icon_size_in_px, color);

      std::vector<unsigned char> output;
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                        &output);

      base::FilePath path;
      if (type == StoredIconType::kAdaptiveBackground) {
        path = GetBackgroundIconPath(base_path(), app_id, icon_size_in_px);
      } else if (type == StoredIconType::kAdaptiveForeground) {
        path = GetForegroundIconPath(base_path(), app_id, icon_size_in_px);
      } else {
        path = GetIconPath(base_path(), app_id, icon_size_in_px,
                           type == StoredIconType::kMaskable);
      }

      base::ScopedAllowBlockingForTesting scoped_allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(path.DirName()));
      ASSERT_TRUE(base::WriteFile(path, output));
    }
  }

  base::FilePath base_path() { return profile_.GetPath(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AppIconDecoderTest, DecodeNonMaskable) {
  StoreSolidColorIconsForApp(kAppId, kIconSizeDp, StoredIconType::kNonMaskable,
                             SK_ColorGREEN);

  base::test::TestFuture<AppIconDecoder*, IconValuePtr> icon_future;

  AppIconDecoder decoder(base_path(), kAppId, kIconSizeDp,
                         icon_future.GetCallback());
  decoder.Start();
  ASSERT_TRUE(icon_future.Wait());

  const IconValuePtr& iv = icon_future.Get<1>();
  EXPECT_EQ(iv->icon_type, IconType::kUncompressed);
  EXPECT_FALSE(iv->is_maskable_icon);

  gfx::ImageSkia expected_image =
      CreateSquareIconImageSkia(kIconSizeDp, SK_ColorGREEN);
  VerifyIcon(expected_image, iv->uncompressed);
}

TEST_F(AppIconDecoderTest, DecodeMaskable) {
  StoreSolidColorIconsForApp(kAppId, kIconSizeDp, StoredIconType::kMaskable,
                             SK_ColorGREEN);

  base::test::TestFuture<AppIconDecoder*, IconValuePtr> icon_future;

  AppIconDecoder decoder(base_path(), kAppId, kIconSizeDp,
                         icon_future.GetCallback());
  decoder.Start();
  ASSERT_TRUE(icon_future.Wait());

  const IconValuePtr& iv = icon_future.Get<1>();
  EXPECT_EQ(iv->icon_type, IconType::kUncompressed);
  EXPECT_TRUE(iv->is_maskable_icon);

  gfx::ImageSkia expected_image =
      CreateSquareIconImageSkia(kIconSizeDp, SK_ColorGREEN);
  VerifyIcon(expected_image, iv->uncompressed);
}

TEST_F(AppIconDecoderTest, DecodeAdaptiveIcon) {
  // If the app has both adaptive and non-adaptive icons in storage,
  // non-adaptive icons should be ignored.
  StoreSolidColorIconsForApp(kAppId, kIconSizeDp, StoredIconType::kNonMaskable,
                             SK_ColorRED);
  StoreSolidColorIconsForApp(
      kAppId, kIconSizeDp, StoredIconType::kAdaptiveBackground, SK_ColorGREEN);
  StoreSolidColorIconsForApp(kAppId, kIconSizeDp,
                             StoredIconType::kAdaptiveForeground, SK_ColorBLUE);

  base::test::TestFuture<AppIconDecoder*, IconValuePtr> icon_future;

  AppIconDecoder decoder(base_path(), kAppId, kIconSizeDp,
                         icon_future.GetCallback());
  decoder.Start();
  ASSERT_TRUE(icon_future.Wait());

  const IconValuePtr& iv = icon_future.Get<1>();
  EXPECT_EQ(iv->icon_type, IconType::kUncompressed);
  EXPECT_FALSE(iv->is_maskable_icon);

  gfx::ImageSkia expected_foreground =
      CreateSquareIconImageSkia(kIconSizeDp, SK_ColorBLUE);
  gfx::ImageSkia expected_background =
      CreateSquareIconImageSkia(kIconSizeDp, SK_ColorGREEN);
  gfx::ImageSkia expected_image = apps::CompositeImagesAndApplyMask(
      expected_foreground, expected_background);
  EnsureRepresentationsLoaded(expected_image);

  VerifyIcon(expected_image, iv->uncompressed);
}

TEST_F(AppIconDecoderTest, DecodeFromEmptyStorage) {
  // No icons are placed into storage, which should result in a failed decode.
  base::test::TestFuture<AppIconDecoder*, IconValuePtr> icon_future;

  AppIconDecoder decoder(base_path(), kAppId, kIconSizeDp,
                         icon_future.GetCallback());
  decoder.Start();
  ASSERT_TRUE(icon_future.Wait());

  const IconValuePtr& iv = icon_future.Get<1>();
  ASSERT_EQ(iv->icon_type, IconType::kUnknown);
}

}  // namespace apps
