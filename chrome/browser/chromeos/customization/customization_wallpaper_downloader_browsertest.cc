// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/customization/customization_wallpaper_downloader.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace chromeos {

namespace {

constexpr char kOEMWallpaperRelativeURL[] = "/image.png";

constexpr char kServicesManifest[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"default_wallpaper\": \"\%s\",\n"
    "  \"default_apps\": [\n"
    "    \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\n"
    "    \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"\n"
    "  ],\n"
    "  \"localized_content\": {\n"
    "    \"en-US\": {\n"
    "      \"default_apps_folder_name\": \"EN-US OEM Name\"\n"
    "    },\n"
    "    \"en\": {\n"
    "      \"default_apps_folder_name\": \"EN OEM Name\"\n"
    "    },\n"
    "    \"default\": {\n"
    "      \"default_apps_folder_name\": \"Default OEM Name\"\n"
    "    }\n"
    "  }\n"
    "}";

// Expected minimal wallpaper download retry interval in milliseconds.
constexpr int kDownloadRetryIntervalMS = 100;

// Dimension used for width and height of default wallpaper images. A small
// value is used to minimize the amount of time spent compressing and writing
// images.
constexpr int kWallpaperSize = 2;

constexpr SkColor kCustomizedDefaultWallpaperColor = SK_ColorDKGRAY;

std::string ManifestForURL(const std::string& url) {
  return base::StringPrintf(kServicesManifest, url.c_str());
}

// Returns true if the color at the center of |image| is close to
// |expected_color|. (The center is used so small wallpaper images can be
// used.)
bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color) {
  if (image.size().IsEmpty()) {
    LOG(ERROR) << "Image is empty";
    return false;
  }

  const SkBitmap* bitmap = image.bitmap();
  if (!bitmap) {
    LOG(ERROR) << "Unable to get bitmap from image";
    return false;
  }

  gfx::Point center = gfx::Rect(image.size()).CenterPoint();
  SkColor image_color = bitmap->getColor(center.x(), center.y());

  const int kDiff = 3;
  if (std::abs(static_cast<int>(SkColorGetA(image_color)) -
               static_cast<int>(SkColorGetA(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetR(image_color)) -
               static_cast<int>(SkColorGetR(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetG(image_color)) -
               static_cast<int>(SkColorGetG(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetB(image_color)) -
               static_cast<int>(SkColorGetB(expected_color))) > kDiff) {
    LOG(ERROR) << "Expected color near 0x" << std::hex << expected_color
               << " but got 0x" << image_color;
    return false;
  }

  return true;
}

// Creates compressed JPEG image of solid color. Result bytes are written to
// |output|. Returns true on success.
bool CreateJPEGImage(int width,
                     int height,
                     SkColor color,
                     std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  if (!gfx::JPEGCodec::Encode(bitmap, 80 /*quality=*/, output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

class TestWallpaperObserver : public ash::WallpaperControllerObserver {
 public:
  TestWallpaperObserver() {
    WallpaperControllerClient::Get()->AddObserver(this);
  }

  ~TestWallpaperObserver() override {
    WallpaperControllerClient::Get()->RemoveObserver(this);
  }

  // ash::WallpaperControllerObserver:
  void OnWallpaperChanged() override {
    finished_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  // Wait until the wallpaper update is completed.
  void WaitForWallpaperChanged() {
    while (!finished_)
      base::RunLoop().Run();
  }

  void Reset() { finished_ = false; }

 private:
  bool finished_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperObserver);
};

}  // namespace

class CustomizationWallpaperDownloaderBrowserTest
    : public InProcessBrowserTest {
 public:
  CustomizationWallpaperDownloaderBrowserTest() {}
  ~CustomizationWallpaperDownloaderBrowserTest() override {}

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    std::vector<unsigned char> oem_wallpaper;
    ASSERT_TRUE(CreateJPEGImage(kWallpaperSize, kWallpaperSize,
                                kCustomizedDefaultWallpaperColor,
                                &oem_wallpaper));
    jpeg_data_.resize(oem_wallpaper.size());
    std::copy(oem_wallpaper.begin(), oem_wallpaper.end(), jpeg_data_.begin());

    // Set up the test server.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CustomizationWallpaperDownloaderBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }

  void SetRequiredRetries(size_t retries) { required_retries_ = retries; }

  size_t num_attempts() const { return attempts_.size(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    chromeos::ServicesCustomizationDocument* customization =
        chromeos::ServicesCustomizationDocument::GetInstance();
    customization->wallpaper_downloader_for_testing()
        ->set_retry_delay_for_testing(
            base::TimeDelta::FromMilliseconds(kDownloadRetryIntervalMS));

    attempts_.push_back(base::TimeTicks::Now());
    if (attempts_.size() > 1) {
      const int retry = num_attempts() - 1;
      const base::TimeDelta current_delay =
          customization->wallpaper_downloader_for_testing()
              ->retry_current_delay_for_testing();
      const double base_interval = base::TimeDelta::FromMilliseconds(
                                       kDownloadRetryIntervalMS).InSecondsF();
      EXPECT_GE(current_delay,
                base::TimeDelta::FromSecondsD(base_interval * retry * retry))
          << "Retry too fast. Actual interval " << current_delay.InSecondsF()
          << " seconds, but expected at least " << base_interval
          << " * (retry=" << retry
          << " * retry)= " << base_interval * retry * retry << " seconds.";
    }
    if (attempts_.size() > required_retries_) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_content_type("image/jpeg");
      response->set_code(net::HTTP_OK);
      response->set_content(jpeg_data_);
      return std::move(response);
    }
    return nullptr;
  }

  // Sample Wallpaper content.
  std::string jpeg_data_;

  // Number of loads performed.
  std::vector<base::TimeTicks> attempts_;

  // Number of retries required.
  size_t required_retries_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CustomizationWallpaperDownloaderBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CustomizationWallpaperDownloaderBrowserTest,
                       OEMWallpaperIsPresent) {
  TestWallpaperObserver observer;
  // Show a built-in default wallpaper first.
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Set the number of required retries.
  SetRequiredRetries(0);

  // Start fetching the customized default wallpaper.
  GURL url = embedded_test_server()->GetURL(kOEMWallpaperRelativeURL);
  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(ManifestForURL(url.spec())));
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Verify the customized default wallpaper has replaced the built-in default
  // wallpaper.
  gfx::ImageSkia image = WallpaperControllerClient::Get()->GetWallpaperImage();
  EXPECT_TRUE(ImageIsNearColor(image, kCustomizedDefaultWallpaperColor));
  EXPECT_EQ(1U, num_attempts());
}

IN_PROC_BROWSER_TEST_F(CustomizationWallpaperDownloaderBrowserTest,
                       OEMWallpaperRetryFetch) {
  TestWallpaperObserver observer;
  // Show a built-in default wallpaper.
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Set the number of required retries.
  SetRequiredRetries(1);

  // Start fetching the customized default wallpaper.
  GURL url = embedded_test_server()->GetURL(kOEMWallpaperRelativeURL);
  chromeos::ServicesCustomizationDocument* customization =
      chromeos::ServicesCustomizationDocument::GetInstance();
  EXPECT_TRUE(
      customization->LoadManifestFromString(ManifestForURL(url.spec())));
  observer.WaitForWallpaperChanged();
  observer.Reset();

  // Verify the customized default wallpaper has replaced the built-in default
  // wallpaper.
  gfx::ImageSkia image = WallpaperControllerClient::Get()->GetWallpaperImage();
  EXPECT_TRUE(ImageIsNearColor(image, kCustomizedDefaultWallpaperColor));
  EXPECT_EQ(2U, num_attempts());
}

}  // namespace chromeos
