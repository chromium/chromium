// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/media_app/media_web_app_info.h"

#include <memory>
#include <string>

#include "ash/webui/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/media_app_guest_ui.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"

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
    {"video/mpeg", ".3gp,.m4v,.mkv,.mov,.mp4,.mpeg,.mpeg4,.mpg,.mpg4"},

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

// The subset of supported image/video extensions that are watched for photos
// integration happiness tracking, and whether a file of that type was ever
// opened in Gallery since the start of this browser process.
constexpr const char* kWatchedImageExtensions[] = {".png", ".jpg", ".jpeg",
                                                   ".webp", ".bmp"};
constexpr const char* kWatchedVideoExtensions[] = {".webm", ".mp4"};
bool g_did_open_image_in_gallery = false;
bool g_did_open_video_in_gallery = false;

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
  return apps::AppLaunchParams(
      params.app_id, params.container, params.disposition, params.launch_source,
      params.display_id, {params.launch_files[index]},
      params.intent ? params.intent->Clone() : nullptr);
}

// Watches a Profile's AppServiceProxy's InstanceRegistry and (possibly)
// triggers a happiness tracking survey (HaTS) when it observes the Photos
// Android app being closed. Owned by a Profile. Registration starts during
// AppServiceProxy initialization when the MediaApp is configured, but via an
// asynchronous task to ensure AppServiceProxy is fully initialized when
// observers are added.
class PhotosExperienceSurveyTrigger : public apps::InstanceRegistry::Observer,
                                      public base::SupportsUserData::Data {
 public:
  PhotosExperienceSurveyTrigger(const PhotosExperienceSurveyTrigger&) = delete;
  PhotosExperienceSurveyTrigger& operator=(
      const PhotosExperienceSurveyTrigger&) = delete;
  ~PhotosExperienceSurveyTrigger() override = default;

  static void Register(Profile* profile) {
    if (profile->GetUserData(&profile_user_data_key)) {
      return;
    }

    auto instance =
        base::WrapUnique(new PhotosExperienceSurveyTrigger(profile));
    auto instance_ptr = instance->weak_ptr_factory_.GetWeakPtr();
    profile->SetUserData(&profile_user_data_key, std::move(instance));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PhotosExperienceSurveyTrigger::RegisterAsync,
                                  instance_ptr));
  }

  static const char* google_photos_app_id;  // Can be overridden for testing.
 private:
  explicit PhotosExperienceSurveyTrigger(Profile* profile)
      : profile_(profile) {}

  void RegisterAsync() {
    // This must be checked here (and not when scheduling the task), because the
    // value can change in some tests while waiting to be executed.
    if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
            profile_)) {
      profile_->SetUserData(&profile_user_data_key, nullptr);
      return;
    }

    apps::AppServiceProxyAsh* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    observation_.Observe(&app_service_proxy->InstanceRegistry());
  }

  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    // Trigger when the Photos app is closed, and no trigger has yet occurred.
    if (update.AppId() != google_photos_app_id || !update.IsDestruction() ||
        hats_notification_controller_) {
      return;
    }
    if (!ash::HatsNotificationController::ShouldShowSurveyToProfile(
            profile_, ash::kHatsPhotosExperienceSurvey)) {
      return;
    }

    hats_notification_controller_ = new ash::HatsNotificationController(
        profile_, ash::kHatsPhotosExperienceSurvey,
        HatsProductSpecificDataForMediaApp());
  }

  void OnInstanceRegistryWillBeDestroyed(apps::InstanceRegistry*) override {
    observation_.Reset();
  }

  // The memory address of this serves as the Profile user data key (which is OK
  // -- nothing is persisted to disk).
  static const int profile_user_data_key;

  raw_ptr<Profile> profile_;  // Weak. Owns `this`.
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      observation_{this};
  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
  base::WeakPtrFactory<PhotosExperienceSurveyTrigger> weak_ptr_factory_{this};
};

const int PhotosExperienceSurveyTrigger::profile_user_data_key = 0;
const char* PhotosExperienceSurveyTrigger::google_photos_app_id =
    arc::kGooglePhotosAppId;

}  // namespace

MediaSystemAppDelegate::MediaSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::MEDIA,
                                "Media",
                                GURL("chrome://media-app/pwa.html"),
                                profile) {
  // Tie survey registration to SWA registration. That is, the delegate map
  // owned by SystemWebAppManager, which is created at startup.
  PhotosExperienceSurveyTrigger::Register(profile);
}

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForMediaWebApp() {
  GURL start_url = GURL(ash::kChromeUIMediaAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIMediaAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_MEDIA_APP_APP_NAME);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
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

  info->theme_color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, /*is_dark_mode=*/false);
  info->dark_mode_theme_color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, /*is_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;

  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  // Add handlers for image+video and audio. We keep them separate since their
  // UX are sufficiently different (we don't want audio files to have a
  // carousel since this would be a second layer of navigation in conjunction
  // with the play queue). Order matters here; the Files app will prefer
  // earlier handlers.
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
  // Note setting `apps::FileHandler::LaunchType::kMultipleClients` here has
  // no effect for system web apps (see comments in
  // WebAppPublisherHelper::OnFileHandlerDialogCompleted()). The PDF-specifc
  // behavior to spawn multiple launches occurs in an override of
  // LaunchAndNavigateSystemWebApp().
  info->file_handlers.push_back(std::move(pdf_handler));
  return info;
}

base::flat_map<std::string, std::string> HatsProductSpecificDataForMediaApp() {
  ash::MediaAppUserActions actions =
      ash::GetMediaAppUserActionsForHappinessTracking();
  return base::flat_map<std::string, std::string>(
      {{"did_open_image_in_gallery",
        base::NumberToString(g_did_open_image_in_gallery)},
       {"did_open_video_in_gallery",
        base::NumberToString(g_did_open_video_in_gallery)},
       {"clicked_edit_image_in_photos",
        base::NumberToString(actions.clicked_edit_image_in_photos)},
       {"clicked_edit_video_in_photos",
        base::NumberToString(actions.clicked_edit_video_in_photos)}});
}

void SetPhotosExperienceSurveyTriggerAppIdForTesting(const char* app_id) {
  PhotosExperienceSurveyTrigger::google_photos_app_id = app_id;
}

std::unique_ptr<web_app::WebAppInstallInfo>
MediaSystemAppDelegate::GetWebAppInfo() const {
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
  return true;
}

bool MediaSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool MediaSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return ShouldShowInLauncher();
}

bool MediaSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

Browser* MediaSystemAppDelegate::GetWindowForLaunch(Profile* profile,
                                                    const GURL& url) const {
  return nullptr;
}

bool MediaSystemAppDelegate::ShouldHandleFileOpenIntents() const {
  return true;
}

Browser* MediaSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  if (!params.launch_files.empty()) {
    const base::FilePath first_file = params.launch_files[0];
    for (const char* extension : kWatchedImageExtensions) {
      g_did_open_image_in_gallery =
          g_did_open_image_in_gallery || first_file.MatchesExtension(extension);
    }
    for (const char* extension : kWatchedVideoExtensions) {
      g_did_open_video_in_gallery =
          g_did_open_video_in_gallery || first_file.MatchesExtension(extension);
    }
  }
  // For zero/single-file launches, or non-PDF launches, launch a single window.
  if (params.launch_files.size() < 2 ||
      !params.launch_files[0].MatchesExtension(kPdfExtension)) {
    return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
        profile, provider, url, params);
  }

  // For PDFs, launch all but the last file from scratch. Windows will cascade.
  for (size_t i = 0; i < params.launch_files.size() - 1; ++i) {
    ash::LaunchSystemWebAppImpl(profile, ash::SystemWebAppType::MEDIA, url,
                                PickFileFromParams(params, i));
  }
  return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
      profile, provider, url,
      PickFileFromParams(params, params.launch_files.size() - 1));
}
