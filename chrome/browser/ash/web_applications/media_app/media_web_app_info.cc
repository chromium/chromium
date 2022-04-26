// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_app/media_web_app_info.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/webui/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using FileHandlerConfig = std::pair<const char*, const char*>;

// FileHandler configuration.
// See https://github.com/WICG/file-handling/blob/main/explainer.md.
constexpr FileHandlerConfig kFileHandlers[] = {
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

    // When updating this list, `FOO_EXTENSIONS` in go/bl-launch should be
    // updated as well.
};

constexpr FileHandlerConfig kAudioFileHandlers[] = {
    {"audio/*", ""},

    // More audio formats.
    {"audio/flac", ".flac"},
    {"audio/m4a", ".m4a"},
    {"audio/mpeg", ".mp3"},
    {"audio/ogg", ".oga,.ogg,.opus"},
    {"audio/wav", ".wav"},
    {"audio/webm", "weba"},

    // Note: some extensions appear twice. See mime_util.cc.
    {"audio/mp3", "mp3"},
    {"audio/x-m4a", "m4a"},

    // When updating this list, `AUDIO_EXTENSIONS` in go/bl-launch should be
    // updated as well.
};

constexpr char kPdfExtension[] = ".pdf";
constexpr FileHandlerConfig kPdfFileHandlers[] = {
    {"application/pdf", kPdfExtension},
};

// Converts a FileHandlerConfig constexpr into the type needed to populate the
// WebAppInstallInfo's `accept` property.
std::vector<apps::FileHandler::AcceptEntry> MakeFileHandlerAccept(
    base::span<const FileHandlerConfig> config) {
  std::vector<apps::FileHandler::AcceptEntry> result;
  result.reserve(config.size());

  const std::string separator = ",";
  for (const auto& handler : config) {
    result.emplace_back();
    result.back().mime_type = handler.first;
    auto file_extensions = base::SplitString(
        handler.second, separator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    result.back().file_extensions.insert(file_extensions.begin(),
                                         file_extensions.end());
  }
  return result;
}

// Picks out a single file from a template launch `params`.
const apps::AppLaunchParams PickFileFromParams(
    const apps::AppLaunchParams& params,
    size_t index) {
  return apps::AppLaunchParams(params.app_id, params.container,
                               params.disposition, params.launch_source,
                               params.display_id, {params.launch_files[index]},
                               params.intent ? params.intent.Clone() : nullptr);
}

}  // namespace

MediaSystemAppDelegate::MediaSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::MEDIA,
          "Media",
          GURL("chrome://media-app/pwa.html"),
          profile,
          web_app::OriginTrialsMap(
              {{web_app::GetOrigin("chrome://media-app"), {"FileHandling"}}})) {
}

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForMediaWebApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIMediaAppURL);
  info->scope = GURL(ash::kChromeUIMediaAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_MEDIA_APP_APP_NAME);

  bool app_icons_added = false;
  if (base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesPdf)) {
#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
    web_app::CreateIconInfoForSystemWebApp(
        info->start_url,
        {
            {"app_icon_16.png", 16, IDR_MEDIA_APP_APP_ICON_16_PNG},
            {"app_icon_32.png", 32, IDR_MEDIA_APP_APP_ICON_32_PNG},
            {"app_icon_48.png", 48, IDR_MEDIA_APP_APP_ICON_48_PNG},
            {"app_icon_64.png", 64, IDR_MEDIA_APP_APP_ICON_64_PNG},
            {"app_icon_96.png", 96, IDR_MEDIA_APP_APP_ICON_96_PNG},
            {"app_icon_128.png", 128, IDR_MEDIA_APP_APP_ICON_128_PNG},
            {"app_icon_192.png", 192, IDR_MEDIA_APP_APP_ICON_192_PNG},
            {"app_icon_256.png", 256, IDR_MEDIA_APP_APP_ICON_256_PNG},
        },
        *info);
    app_icons_added = true;
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP)
  }
  if (!app_icons_added) {
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
  }

  if (chromeos::features::IsDarkLightModeEnabled()) {
    auto* color_provider = ash::AshColorProvider::Get();
    info->theme_color =
        color_provider->GetBackgroundColorInMode(/*use_dark_color=*/false);
    info->dark_mode_theme_color =
        color_provider->GetBackgroundColorInMode(/*use_dark_color=*/true);
    info->background_color = info->theme_color;
    info->dark_mode_background_color = info->dark_mode_theme_color;
  } else {
    info->theme_color = 0xff202124;
    info->background_color = 0xff3c4043;
  }

  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::UserDisplayMode::kStandalone;

  // Add handlers for image+video and audio. We keep them separate since their
  // UX are sufficiently different (we don't want audio files to have a carousel
  // since this would be a second layer of navigation in conjunction with the
  // play queue). Order matters here; the Files app will prefer earlier
  // handlers.
  apps::FileHandler image_video_handler;
  image_video_handler.action = GURL(ash::kChromeUIMediaAppURL);
  image_video_handler.accept = MakeFileHandlerAccept(kFileHandlers);
  info->file_handlers.push_back(std::move(image_video_handler));

  apps::FileHandler audio_handler;
  audio_handler.action = GURL(ash::kChromeUIMediaAppURL);
  audio_handler.accept = MakeFileHandlerAccept(kAudioFileHandlers);
  info->file_handlers.push_back(std::move(audio_handler));

  apps::FileHandler pdf_handler;
  pdf_handler.action = GURL(ash::kChromeUIMediaAppURL);
  pdf_handler.accept = MakeFileHandlerAccept(kPdfFileHandlers);
  // Note setting `apps::FileHandler::LaunchType::kMultipleClients` here has no
  // effect for system web apps (see comments in
  // WebAppPublisherHelper::OnFileHandlerDialogCompleted()). The PDF-specifc
  // behavior to spawn multiple launches occurs in an override of
  // LaunchAndNavigateSystemWebApp().
  info->file_handlers.push_back(std::move(pdf_handler));
  return info;
}

std::unique_ptr<WebAppInstallInfo> MediaSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForMediaWebApp();
}

base::FilePath MediaSystemAppDelegate::GetLaunchDirectory(
    const apps::AppLaunchParams& params) const {
  // |launch_dir| is the directory that contains all |launch_files|. If
  // there are no launch files, launch_dir is empty.
  base::FilePath launch_dir = params.launch_files.size()
                                  ? params.launch_files[0].DirName()
                                  : base::FilePath();

#if DCHECK_IS_ON()
  // Check |launch_files| all come from the same directory.
  if (!launch_dir.empty()) {
    for (const auto& path : params.launch_files) {
      DCHECK_EQ(launch_dir, path.DirName());
    }
  }
#endif

  return launch_dir;
}

bool MediaSystemAppDelegate::ShouldShowInLauncher() const {
  return base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesPdf);
}

bool MediaSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool MediaSystemAppDelegate::ShouldShowInSearch() const {
  return ShouldShowInLauncher();
}

bool MediaSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

bool MediaSystemAppDelegate::ShouldReuseExistingWindow() const {
  return !ShouldShowNewWindowMenuOption();
}

bool MediaSystemAppDelegate::ShouldHandleFileOpenIntents() const {
  return true;
}

Browser* MediaSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  // For zero/single-file launches, or non-PDF launches, launch a single window.
  if (params.launch_files.size() < 2 ||
      !params.launch_files[0].MatchesExtension(kPdfExtension)) {
    return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
        profile, provider, url, params);
  }

  // For PDFs, launch all but the last file from scratch. Windows will cascade.
  for (size_t i = 0; i < params.launch_files.size() - 1; ++i) {
    web_app::LaunchSystemWebAppImpl(profile, web_app::SystemAppType::MEDIA, url,
                                    PickFileFromParams(params, i));
  }
  return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
      profile, provider, url,
      PickFileFromParams(params, params.launch_files.size() - 1));
}
