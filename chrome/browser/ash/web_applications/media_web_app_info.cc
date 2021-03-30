// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_web_app_info.h"

#include <memory>
#include <string>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_media_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// FileHandler configuration.
// See https://github.com/WICG/file-handling/blob/master/explainer.md.
constexpr std::tuple<const char*, const char*> kFileHandlers[] = {
    {"image/*", ""},
    {"video/*", ""},

    // Raw images. Note the MIME type doesn't really matter here. MIME sniffing
    // logic in the files app tends to detect image/tiff, but sniffing is only
    // done for "local" files, so the extension list is needed in addition to
    // the "image/*" wildcard above. The MIME type is never sent to the web app.
    {"image/tiff", ".arw,.cr2,.dng,.nef,.nrw,.orf,.raf,.rw2"},

    // More video formats; building on the video/* wildcard which doesn't
    // actually catch very much due to hard-coded maps in Chromium. Again, the
    // MIME type doesn't really matter. "video/mpeg" is used a catchall for
    // unknown mime types.
    {"video/ogg", ".ogv,.ogx,.ogm"},
    {"video/webm", ".webm"},
    {"video/mpeg", ".3gp,.avi,.m4v,.mkv,.mov,.mp4,.mpeg,.mpeg4,.mpg,.mpg4"},

    // More image formats. These are needed because MIME types are not always
    // available, even for known extensions. E.g. filesystem providers may
    // override Chrome's builtin mapping with an empty string. All supported
    // image/* `k{Primary,Secondary}Mappings` in net/base/mime_util.cc should be
    // listed here to ensure consistency with the image/* handler.
    {"image/bmp", ".bmp"},
    {"image/gif", ".gif"},
    {"image/vnd.microsoft.icon", ".ico"},
    {"image/jpeg", ".jpeg,.jpg,.jpe,.jfif,.jif,.jfi,.pjpeg,.pjp"},
    {"image/png", ".png"},
    {"image/webp", ".webp"},
    {"image/svg+xml", ".svg,.svgz"},
    {"image/avif", ".avif"},
};

using AcceptMap = decltype(blink::Manifest::FileHandler::accept);

// Converts the kFileHandlers constexpr into the std::map needed to populate the
// web app manifest's `accept` property.
AcceptMap MakeHandlerAccept() {
  AcceptMap result;
  const std::u16string separator = u",";
  for (const auto& handler : kFileHandlers) {
    result[base::ASCIIToUTF16(std::get<0>(handler))] =
        base::SplitString(base::ASCIIToUTF16(std::get<1>(handler)), separator,
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  return result;
}

}  // namespace

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForMediaWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIMediaAppURL);
  info->scope = GURL(chromeos::kChromeUIMediaAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_MEDIA_APP_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_16.png", 16, IDR_MEDIA_APP_GALLERY_ICON_16_PNG},
          {"app_icon_32.png", 32, IDR_MEDIA_APP_GALLERY_ICON_32_PNG},
          {"app_icon_48.png", 48, IDR_MEDIA_APP_GALLERY_ICON_48_PNG},
          {"app_icon_64.png", 64, IDR_MEDIA_APP_GALLERY_ICON_64_PNG},
          {"app_icon_96.png", 96, IDR_MEDIA_APP_GALLERY_ICON_96_PNG},
          {"app_icon_128.png", 128, IDR_MEDIA_APP_GALLERY_ICON_128_PNG},
          {"app_icon_192.png", 192, IDR_MEDIA_APP_GALLERY_ICON_192_PNG},
          {"app_icon_256.png", 256, IDR_MEDIA_APP_GALLERY_ICON_256_PNG},

      },
      *info);
  info->theme_color = 0xff202124;
  info->background_color = 0xff3c4043;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  blink::Manifest::FileHandler file_handler;
  file_handler.action = GURL(chromeos::kChromeUIMediaAppURL);
  file_handler.name = u"Media File";
  file_handler.accept = MakeHandlerAccept();
  info->file_handlers.push_back(std::move(file_handler));
  return info;
}
