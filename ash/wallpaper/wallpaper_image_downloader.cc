// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_image_downloader.h"

#include <optional>
#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/account_id/account_id.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kDownloadGooglePhotoTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_download_google_photo",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Image Downloader"
        description:
          "When the user selects a photo from their Google Photos collection, "
          "the image must be downloaded at a high enough resolution to display "
          "as a wallpaper. This request fetches that image."
        trigger: "When the user selects a Google Photo as their wallpaper, or "
                 "when that selection reaches this device from cross-device "
                 "sync."
        data: "Stored credentials for the user's Google account."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "assistive-eng@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2023-03-06"
      }
      policy {
        cookies_allowed: NO
        setting: "The policy if set, controls the wallpaper image and disables "
        "this feature for user."
        chrome_policy {
          WallpaperImage {
            WallpaperImage: "{}"
          }
        }
      })");

constexpr net::NetworkTrafficAnnotationTag
    kDownloadOnlineWallpaperTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_online_downloader",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Image Downloader"
        description:
          "When the user selects a photo from their desktop wallpaper "
          "collection, the image must be downloaded at a high enough "
          "resolution to display as a wallpaper. This request fetches "
          "that image."
        trigger: "When the user clicks on the wallpaper thumbnail in "
        "the wallpaper collection"
        data: "None. These URLs are publicly accessible."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "assitive-eng@google.com"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2023-03-06"
      }
     policy {
        cookies_allowed: NO
        setting: "The policy if set, controls the wallpaper image and disables "
        "this feature for user."
        chrome_policy {
          WallpaperImage {
            WallpaperImage: "{}"
          }
        }
      })");

GURL AddDimensionsToGooglePhotosURL(GURL url) {
  // Add a string with size data to the URL to make sure we get back the correct
  // resolution image, within reason and maintaining aspect ratio. See:
  // https://developers.google.com/photos/library/guides/access-media-items
  return GURL(base::StringPrintf("%s=w%d-h%d", url.spec().c_str(),
                                 kLargeWallpaperMaxWidth,
                                 kLargeWallpaperMaxHeight));
}

// Returns a suffix to be appended to the base url of Backdrop (online)
// wallpapers.
std::string GetBackdropWallpaperSuffix() {
  // TODO(b/186807814) handle different display resolutions better.
  // FIFE url is used for Backdrop wallpapers and the desired image size should
  // be specified. Currently we are using two times the display size. This is
  // determined by trial and error and is subject to change.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  return "=w" + base::NumberToString(
                    2 * std::max(display_size.width(), display_size.height()));
}

}  // namespace

WallpaperImageDownloaderImpl::WallpaperImageDownloaderImpl() = default;

WallpaperImageDownloaderImpl::~WallpaperImageDownloaderImpl() = default;

void WallpaperImageDownloaderImpl::DownloadGooglePhotosImage(
    const GURL& url,
    const AccountId& account_id,
    const std::optional<std::string>& access_token,
    ImageDownloader::DownloadCallback callback) const {
  GURL url_with_dimensions = AddDimensionsToGooglePhotosURL(url);

  net::HttpRequestHeaders headers;
  if (access_token.has_value()) {
    headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                      "Bearer " + access_token.value());
  }
  ImageDownloader::Get()->Download(url_with_dimensions,
                                   kDownloadGooglePhotoTrafficAnnotation,
                                   account_id, headers, std::move(callback));
}

void WallpaperImageDownloaderImpl::DownloadBackdropImage(
    const GURL& url,
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback) const {
  ImageDownloader::Get()->Download(
      GURL(url.spec() + GetBackdropWallpaperSuffix()),
      kDownloadOnlineWallpaperTrafficAnnotation, account_id,
      std::move(callback));
}

}  // namespace ash