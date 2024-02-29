// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_sea_pen_provider_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/manta/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash::vc_background_ui {
namespace {

// Create fake Jpg image bytes.
std::string CreateJpgBytes() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

class VcBackgroundUISeaPenProviderImplTest : public InProcessBrowserTest {
 public:
  VcBackgroundUISeaPenProviderImplTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference, manta::features::kMantaService,
         features::kVcBackgroundReplace,
         features::kCameraEffectsSupportedByHardware},
        {});
  }

  VcBackgroundUISeaPenProviderImplTest(
      const VcBackgroundUISeaPenProviderImplTest&) = delete;
  VcBackgroundUISeaPenProviderImplTest& operator=(
      const VcBackgroundUISeaPenProviderImplTest&) = delete;

  ~VcBackgroundUISeaPenProviderImplTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Setup  `sea_pen_provider_ `.
    auto* web_contents = content::WebContents::FromRenderFrameHost(
        ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    web_ui_.set_web_contents(web_contents);
    sea_pen_provider_ = std::make_unique<VcBackgroundUISeaPenProviderImpl>(
        &web_ui_,
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    sea_pen_provider_->BindInterface(
        sea_pen_provider_remote_.BindNewPipeAndPassReceiver());

    // Setup  `camera_effects_controller_ `.
    auto* camera_effects_controller = Shell::Get()->camera_effects_controller();

    // Enable test mode to mock the SetCameraEffects calls.
    camera_effects_controller->bypass_set_camera_effects_for_testing(true);

    const base::FilePath camera_background_img_dir =
        browser()->profile()->GetPath().AppendASCII(
            "camera_background_img_dir");
    const base::FilePath camera_background_run_dir =
        browser()->profile()->GetPath().AppendASCII(
            "camera_background_run_dir");
    ASSERT_TRUE(base::CreateDirectory(camera_background_img_dir));
    ASSERT_TRUE(base::CreateDirectory(camera_background_run_dir));
    camera_effects_controller->set_camera_background_img_dir_for_testing(
        camera_background_img_dir);
    camera_effects_controller->set_camera_background_run_dir_for_testing(
        camera_background_run_dir);

    // Create fake background images.
    const base::Time time = base::Time::Now();
    for (std::size_t i = 0; i < existing_image_ids_.size(); ++i) {
      const auto filename =
          camera_background_img_dir
              .Append(base::NumberToString(existing_image_ids_[i]))
              .AddExtension(".jpg");
      ASSERT_TRUE(base::WriteFile(filename, CreateJpgBytes()));
      // Change file modification time so that the first file is the latest.
      ASSERT_TRUE(base::TouchFile(filename, time - base::Minutes(i),
                                  time - base::Minutes(i)));
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestWebUI web_ui_;
  mojo::Remote<ash::personalization_app::mojom::SeaPenProvider>
      sea_pen_provider_remote_;
  std::unique_ptr<VcBackgroundUISeaPenProviderImpl> sea_pen_provider_;

  const std::vector<uint32_t> existing_image_ids_{19, 23};
};

IN_PROC_BROWSER_TEST_F(VcBackgroundUISeaPenProviderImplTest, AllTests) {
  // Get all background images.
  base::RunLoop run_loop;
  sea_pen_provider_->GetRecentSeaPenImages(
      base::BindLambdaForTesting([&](const std::vector<uint32_t>& ids) {
        EXPECT_EQ(existing_image_ids_, ids);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Select an image as background.
  base::RunLoop run_loop2;
  sea_pen_provider_->SelectRecentSeaPenImage(
      existing_image_ids_[0],
      base::BindLambdaForTesting([&](bool call_succeeded) {
        EXPECT_TRUE(call_succeeded);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Get the content of the image.
  base::RunLoop run_loop3;
  sea_pen_provider_->GetRecentSeaPenImageThumbnail(
      existing_image_ids_[0],
      base::BindLambdaForTesting(
          [&](personalization_app::mojom::RecentSeaPenThumbnailDataPtr
                  thumbnail_data) {
            EXPECT_FALSE(thumbnail_data->url.is_empty());
            run_loop3.Quit();
          }));

  run_loop3.Run();

  // Delete an existing image should return true.
  base::RunLoop run_loop4;
  sea_pen_provider_->DeleteRecentSeaPenImage(
      existing_image_ids_[1],
      base::BindLambdaForTesting([&](bool call_succeeded) {
        EXPECT_TRUE(call_succeeded);

        run_loop4.Quit();
      }));

  run_loop4.Run();

  // Select an deleted image should return false.
  base::RunLoop run_loop5;
  sea_pen_provider_->SelectRecentSeaPenImage(
      existing_image_ids_[1],
      base::BindLambdaForTesting([&](bool call_succeeded) {
        EXPECT_FALSE(call_succeeded);
        run_loop5.Quit();
      }));

  run_loop5.Run();
  // Get content of an deleted image should return nullptr.
  base::RunLoop run_loop6;
  sea_pen_provider_->GetRecentSeaPenImageThumbnail(
      existing_image_ids_[1],
      base::BindLambdaForTesting(
          [&](personalization_app::mojom::RecentSeaPenThumbnailDataPtr
                  thumbnail_data) {
            EXPECT_FALSE(thumbnail_data);
            run_loop6.Quit();
          }));
  run_loop6.Run();
}

}  // namespace

}  // namespace ash::vc_background_ui
