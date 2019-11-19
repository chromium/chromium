// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper.pb.h"
#include "chrome/browser/chromeos/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/app_locale_settings.h"

using base::Value;
using content::BrowserThread;

namespace wallpaper_base = extensions::api::wallpaper;
namespace wallpaper_private = extensions::api::wallpaper_private;
namespace set_wallpaper_if_exists = wallpaper_private::SetWallpaperIfExists;
namespace set_wallpaper = wallpaper_private::SetWallpaper;
namespace set_custom_wallpaper = wallpaper_private::SetCustomWallpaper;
namespace set_custom_wallpaper_layout =
    wallpaper_private::SetCustomWallpaperLayout;
namespace get_thumbnail = wallpaper_private::GetThumbnail;
namespace save_thumbnail = wallpaper_private::SaveThumbnail;
namespace record_wallpaper_uma = wallpaper_private::RecordWallpaperUMA;
namespace get_collections_info = wallpaper_private::GetCollectionsInfo;
namespace get_images_info = wallpaper_private::GetImagesInfo;
namespace get_local_image_paths = wallpaper_private::GetLocalImagePaths;
namespace get_local_image_data = wallpaper_private::GetLocalImageData;
namespace get_current_wallpaper_thumbnail =
    wallpaper_private::GetCurrentWallpaperThumbnail;
namespace get_surprise_me_image = wallpaper_private::GetSurpriseMeImage;

namespace {

// The time and retry limit to re-check the profile sync service status. The
// sync extension function can get the correct value of the "syncThemes" user
// preference only after the profile sync service has been configured.
constexpr base::TimeDelta kRetryDelay = base::TimeDelta::FromSeconds(10);
constexpr int kRetryLimit = 3;

constexpr char kSyncThemes[] = "syncThemes";

constexpr char kPngFilePattern[] = "*.[pP][nN][gG]";
constexpr char kJpgFilePattern[] = "*.[jJ][pP][gG]";
constexpr char kJpegFilePattern[] = "*.[jJ][pP][eE][gG]";

bool IsOEMDefaultWallpaper() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDefaultWallpaperIsOem);
}

// Returns a suffix to be appended to the base url of Backdrop wallpapers.
std::string GetBackdropWallpaperSuffix() {
  // FIFE url is used for Backdrop wallpapers and the desired image size should
  // be specified. Currently we are using two times the display size. This is
  // determined by trial and error and is subject to change.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  return "=w" + std::to_string(
                    2 * std::max(display_size.width(), display_size.height()));
}

// Saves |data| as |file_name| to directory with |key|. Return false if the
// directory can not be found/created or failed to write file.
bool SaveData(int key,
              const std::string& file_name,
              const std::vector<uint8_t>& data) {
  base::FilePath data_dir;
  CHECK(base::PathService::Get(key, &data_dir));
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir)) {
    return false;
  }
  base::FilePath file_path = data_dir.Append(file_name);

  return base::PathExists(file_path) ||
         base::WriteFile(file_path, reinterpret_cast<const char*>(data.data()),
                         data.size()) != -1;
}

// Gets |file_name| from directory with |key|. Return false if the directory can
// not be found or failed to read file to string |data|. Note if the |file_name|
// can not be found in the directory, return true with empty |data|. It is
// expected that we may try to access file which did not saved yet.
bool GetData(const base::FilePath& path, std::string* data) {
  base::FilePath data_dir = path.DirName();
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir))
    return false;

  return !base::PathExists(path) ||
         base::ReadFileToString(path, data);
}

// Gets the |User| for a given |BrowserContext|. The function will only return
// valid objects.
const user_manager::User* GetUserFromBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

ash::WallpaperType GetWallpaperType(wallpaper_private::WallpaperSource source) {
  switch (source) {
    case wallpaper_private::WALLPAPER_SOURCE_ONLINE:
      return ash::ONLINE;
    case wallpaper_private::WALLPAPER_SOURCE_DAILY:
      return ash::DAILY;
    case wallpaper_private::WALLPAPER_SOURCE_CUSTOM:
      return ash::CUSTOMIZED;
    case wallpaper_private::WALLPAPER_SOURCE_OEM:
      return ash::DEFAULT;
    case wallpaper_private::WALLPAPER_SOURCE_THIRDPARTY:
      return ash::THIRDPARTY;
    default:
      return ash::ONLINE;
  }
}

// Helper function to get the list of image paths under |path| that match
// |pattern|.
void EnumerateImages(const base::FilePath& path,
                     const std::string& pattern,
                     std::vector<std::string>* result_out) {
  base::FileEnumerator image_enum(
      path, true /* recursive */, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL(pattern),
      base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath image_path = image_enum.Next(); !image_path.empty();
       image_path = image_enum.Next()) {
    result_out->emplace_back(image_path.value());
  }
}

// Recursively retrieves the paths of the image files under |path|.
std::vector<std::string> GetImagePaths(const base::FilePath& path) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());

  // TODO(crbug.com/810575): Add metrics on the number of files retrieved, and
  // support getting paths incrementally in case the user has a large number of
  // local images.
  std::vector<std::string> image_paths;
  EnumerateImages(path, kPngFilePattern, &image_paths);
  EnumerateImages(path, kJpgFilePattern, &image_paths);
  EnumerateImages(path, kJpegFilePattern, &image_paths);

  return image_paths;
}

// Helper function to parse the data from a |backdrop::Image| object and save it
// to |image_info_out|.
void ParseImageInfo(
    const backdrop::Image& image,
    extensions::api::wallpaper_private::ImageInfo* image_info_out) {
  // The info of each image should contain image url, action url and display
  // text.
  image_info_out->image_url = image.image_url();
  image_info_out->action_url = image.action_url();
  // Display text may have more than one strings.
  for (int i = 0; i < image.attribution_size(); ++i)
    image_info_out->display_text.push_back(image.attribution()[i].text());
}

}  // namespace

ExtensionFunction::ResponseAction WallpaperPrivateGetStringsFunction::Run() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("allCategoryLabel", IDS_WALLPAPER_MANAGER_ALL_CATEGORY_LABEL);
  SET_STRING("deleteCommandLabel", IDS_WALLPAPER_MANAGER_DELETE_COMMAND_LABEL);
  SET_STRING("customCategoryLabel",
             IDS_WALLPAPER_MANAGER_MY_IMAGES_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("refreshLabel", IDS_WALLPAPER_MANAGER_REFRESH_LABEL);
  SET_STRING("exploreLabel", IDS_WALLPAPER_MANAGER_EXPLORE_LABEL);
  SET_STRING("centerCroppedLayout",
             IDS_WALLPAPER_MANAGER_LAYOUT_CENTER_CROPPED);
  SET_STRING("centerLayout", IDS_WALLPAPER_MANAGER_LAYOUT_CENTER);
  SET_STRING("stretchLayout", IDS_WALLPAPER_MANAGER_LAYOUT_STRETCH);
  SET_STRING("connectionFailed", IDS_WALLPAPER_MANAGER_NETWORK_ERROR);
  SET_STRING("downloadFailed", IDS_WALLPAPER_MANAGER_IMAGE_ERROR);
  SET_STRING("downloadCanceled", IDS_WALLPAPER_MANAGER_DOWNLOAD_CANCEL);
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
  SET_STRING("accessFileFailure", IDS_WALLPAPER_MANAGER_ACCESS_FILE_FAILURE);
  SET_STRING("invalidWallpaper", IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER);
  SET_STRING("noImagesAvailable", IDS_WALLPAPER_MANAGER_NO_IMAGES_AVAILABLE);
  SET_STRING("surpriseMeLabel", IDS_WALLPAPER_MANAGER_DAILY_REFRESH_LABEL);
  SET_STRING("learnMore", IDS_LEARN_MORE);
  SET_STRING("currentWallpaperSetByMessage",
             IDS_CURRENT_WALLPAPER_SET_BY_MESSAGE);
  SET_STRING("currentlySetLabel", IDS_WALLPAPER_MANAGER_CURRENTLY_SET_LABEL);
  SET_STRING("confirmPreviewLabel",
             IDS_WALLPAPER_MANAGER_CONFIRM_PREVIEW_WALLPAPER_LABEL);
  SET_STRING("setSuccessfullyMessage",
             IDS_WALLPAPER_MANAGER_SET_SUCCESSFULLY_MESSAGE);
  SET_STRING("defaultWallpaperLabel", IDS_DEFAULT_WALLPAPER_ACCESSIBLE_LABEL);
  SET_STRING("backButton", IDS_ACCNAME_BACK);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  dict->SetBoolean("isOEMDefaultWallpaper", IsOEMDefaultWallpaper());
  dict->SetString("canceledWallpaper",
                  wallpaper_api_util::kCancelWallpaperMessage);
  dict->SetString("highResolutionSuffix", GetBackdropWallpaperSuffix());

  auto info = WallpaperControllerClient::Get()->GetActiveUserWallpaperInfo();
  dict->SetString("currentWallpaper", info.location);
  dict->SetString("currentWallpaperLayout",
                  wallpaper_api_util::GetLayoutString(info.layout));

  return RespondNow(OneArgument(std::move(dict)));
}

ExtensionFunction::ResponseAction
WallpaperPrivateGetSyncSettingFunction::Run() {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus,
          this));
  return RespondLater();
}

void WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus() {
  auto dict = std::make_unique<base::DictionaryValue>();

  if (retry_number_ > kRetryLimit) {
    // It's most likely that the wallpaper synchronization is enabled (It's
    // enabled by default so unless the user disables it explicitly it remains
    // enabled).
    dict->SetBoolean(kSyncThemes, true);
    Respond(OneArgument(std::move(dict)));
    return;
  }

  Profile* profile =  Profile::FromBrowserContext(browser_context());
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->CanSyncFeatureStart()) {
    // Sync as a whole is disabled.
    dict->SetBoolean(kSyncThemes, false);
    Respond(OneArgument(std::move(dict)));
    return;
  }

  if (sync_service->GetUserSettings()->IsFirstSetupComplete()) {
    // Sync is set up. Report whether the user has selected to sync themes.
    dict->SetBoolean(kSyncThemes,
                     sync_service->GetUserSettings()->GetSelectedTypes().Has(
                         syncer::UserSelectableType::kThemes));
    Respond(OneArgument(std::move(dict)));
    return;
  }

  // The user hasn't finished setting up sync, so we don't know whether they'll
  // want to sync themes. Try again in a bit.
  // TODO(xdai): It would be cleaner to implement a SyncServiceObserver and wait
  // for OnStateChanged() instead of polling.
  retry_number_++;
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus,
          this),
      retry_number_ * kRetryDelay);
}

WallpaperPrivateSetWallpaperIfExistsFunction::
    WallpaperPrivateSetWallpaperIfExistsFunction() {}

WallpaperPrivateSetWallpaperIfExistsFunction::
    ~WallpaperPrivateSetWallpaperIfExistsFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetWallpaperIfExistsFunction::Run() {
  std::unique_ptr<
      extensions::api::wallpaper_private::SetWallpaperIfExists::Params>
      params = set_wallpaper_if_exists::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  WallpaperControllerClient::Get()->SetOnlineWallpaperIfExists(
      GetUserFromBrowserContext(browser_context())->GetAccountId(), params->url,
      wallpaper_api_util::GetLayoutEnum(
          wallpaper_base::ToString(params->layout)),
      params->preview_mode,
      base::BindOnce(&WallpaperPrivateSetWallpaperIfExistsFunction::
                         OnSetOnlineWallpaperIfExistsCallback,
                     this));
  return RespondLater();
}

void WallpaperPrivateSetWallpaperIfExistsFunction::
    OnSetOnlineWallpaperIfExistsCallback(bool file_exists) {
  if (file_exists) {
    Respond(OneArgument(std::make_unique<base::Value>(true)));
  } else {
    auto args = std::make_unique<base::ListValue>();
    // TODO(crbug.com/830212): Do not send arguments when the function fails.
    // Call sites should inspect chrome.runtime.lastError instead.
    args->AppendBoolean(false);
    Respond(ErrorWithArguments(
        std::move(args), "The wallpaper doesn't exist in local file system."));
  }
}

WallpaperPrivateSetWallpaperFunction::WallpaperPrivateSetWallpaperFunction() {
}

WallpaperPrivateSetWallpaperFunction::~WallpaperPrivateSetWallpaperFunction() {
}

ExtensionFunction::ResponseAction WallpaperPrivateSetWallpaperFunction::Run() {
  std::unique_ptr<extensions::api::wallpaper_private::SetWallpaper::Params>
      params = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  WallpaperControllerClient::Get()->SetOnlineWallpaperFromData(
      GetUserFromBrowserContext(browser_context())->GetAccountId(),
      std::string(params->wallpaper.begin(), params->wallpaper.end()),
      params->url,
      wallpaper_api_util::GetLayoutEnum(
          wallpaper_base::ToString(params->layout)),
      params->preview_mode,
      base::BindOnce(
          &WallpaperPrivateSetWallpaperFunction::OnSetWallpaperCallback, this));
  return RespondLater();
}

void WallpaperPrivateSetWallpaperFunction::OnSetWallpaperCallback(
    bool success) {
  if (!success) {
    Respond(Error("Failed to set wallpaper."));
    return;
  }

  Respond(NoArguments());
}

WallpaperPrivateResetWallpaperFunction::
    WallpaperPrivateResetWallpaperFunction() {}

WallpaperPrivateResetWallpaperFunction::
    ~WallpaperPrivateResetWallpaperFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateResetWallpaperFunction::Run() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();

  WallpaperControllerClient::Get()->SetDefaultWallpaper(
      account_id, true /* show_wallpaper */);
  return RespondNow(NoArguments());
}

WallpaperPrivateSetCustomWallpaperFunction::
    WallpaperPrivateSetCustomWallpaperFunction() {}

WallpaperPrivateSetCustomWallpaperFunction::
    ~WallpaperPrivateSetCustomWallpaperFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetCustomWallpaperFunction::Run() {
  params = set_custom_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();
  wallpaper_files_id_ =
      WallpaperControllerClient::Get()->GetFilesId(account_id_);

  StartDecode(params->wallpaper);

  return RespondLater();
}

void WallpaperPrivateSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  const std::string file_name =
      base::FilePath(params->file_name).BaseName().value();
  WallpaperControllerClient::Get()->SetCustomWallpaper(
      account_id_, wallpaper_files_id_, file_name, layout, image,
      params->preview_mode);
  unsafe_wallpaper_decoder_ = nullptr;

  if (params->generate_thumbnail) {
    image.EnsureRepsForSupportedScales();
    scoped_refptr<base::RefCountedBytes> thumbnail_data;
    GenerateThumbnail(
        image, gfx::Size(kWallpaperThumbnailWidth, kWallpaperThumbnailHeight),
        &thumbnail_data);
    Respond(OneArgument(Value::CreateWithCopiedBuffer(
        reinterpret_cast<const char*>(thumbnail_data->front()),
        thumbnail_data->size())));
  } else {
    Respond(NoArguments());
  }
}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    ~WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetCustomWallpaperLayoutFunction::Run() {
  std::unique_ptr<set_custom_wallpaper_layout::Params> params(
      set_custom_wallpaper_layout::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::WallpaperLayout new_layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(new_layout);
  WallpaperControllerClient::Get()->UpdateCustomWallpaperLayout(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      new_layout);
  return RespondNow(NoArguments());
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    ~WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateMinimizeInactiveWindowsFunction::Run() {
  WallpaperControllerClient::Get()->MinimizeInactiveWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return RespondNow(NoArguments());
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    ~WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateRestoreMinimizedWindowsFunction::Run() {
  WallpaperControllerClient::Get()->RestoreMinimizedWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return RespondNow(NoArguments());
}

WallpaperPrivateGetThumbnailFunction::WallpaperPrivateGetThumbnailFunction() {
}

WallpaperPrivateGetThumbnailFunction::~WallpaperPrivateGetThumbnailFunction() {
}

ExtensionFunction::ResponseAction WallpaperPrivateGetThumbnailFunction::Run() {
  std::unique_ptr<get_thumbnail::Params> params(
      get_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath thumbnail_path;
  if (params->source == wallpaper_private::WALLPAPER_SOURCE_ONLINE) {
    std::string file_name = GURL(params->url_or_file).ExtractFileName();
    CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS,
                                 &thumbnail_path));
    thumbnail_path = thumbnail_path.Append(file_name);
  } else {
    if (!IsOEMDefaultWallpaper())
      return RespondNow(Error("No OEM wallpaper."));

    // TODO(bshe): Small resolution wallpaper is used here as wallpaper
    // thumbnail. We should either resize it or include a wallpaper thumbnail in
    // addition to large and small wallpaper resolutions.
    thumbnail_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        chromeos::switches::kDefaultWallpaperSmall);
  }

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Get,
                                this, thumbnail_path));
  // WallpaperPrivateGetThumbnailFunction::Get will respond on UI thread
  // asynchronously.
  return RespondLater();
}

void WallpaperPrivateGetThumbnailFunction::Failure(
    const std::string& file_name) {
  Respond(Error(base::StringPrintf(
      "Failed to access wallpaper thumbnails for %s.", file_name.c_str())));
}

void WallpaperPrivateGetThumbnailFunction::FileNotLoaded() {
  // TODO(https://crbug.com/829657): This should fail instead of succeeding.
  Respond(NoArguments());
}

void WallpaperPrivateGetThumbnailFunction::FileLoaded(
    const std::string& data) {
  Respond(
      OneArgument(Value::CreateWithCopiedBuffer(data.c_str(), data.size())));
}

void WallpaperPrivateGetThumbnailFunction::Get(const base::FilePath& path) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  std::string data;
  if (GetData(path, &data)) {
    if (data.empty()) {
      base::PostTask(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileNotLoaded,
                         this));
    } else {
      base::PostTask(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileLoaded,
                         this, data));
    }
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Failure, this,
                       path.BaseName().value()));
  }
}

WallpaperPrivateSaveThumbnailFunction::WallpaperPrivateSaveThumbnailFunction() {
}

WallpaperPrivateSaveThumbnailFunction::
    ~WallpaperPrivateSaveThumbnailFunction() {}

ExtensionFunction::ResponseAction WallpaperPrivateSaveThumbnailFunction::Run() {
  std::unique_ptr<save_thumbnail::Params> params(
      save_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Save, this,
                     params->data, GURL(params->url).ExtractFileName()));
  // WallpaperPrivateSaveThumbnailFunction::Save will repsond on UI thread
  // asynchronously.
  return RespondLater();
}

void WallpaperPrivateSaveThumbnailFunction::Failure(
    const std::string& file_name) {
  Respond(Error(base::StringPrintf("Failed to create/write thumbnail of %s.",
                                   file_name.c_str())));
}

void WallpaperPrivateSaveThumbnailFunction::Success() {
  Respond(NoArguments());
}

void WallpaperPrivateSaveThumbnailFunction::Save(
    const std::vector<uint8_t>& data,
    const std::string& file_name) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Success, this));
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Failure, this,
                       file_name));
  }
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    WallpaperPrivateGetOfflineWallpaperListFunction() {
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    ~WallpaperPrivateGetOfflineWallpaperListFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateGetOfflineWallpaperListFunction::Run() {
  WallpaperControllerClient::Get()->GetOfflineWallpaperList(
      base::BindOnce(&WallpaperPrivateGetOfflineWallpaperListFunction::
                         OnOfflineWallpaperListReturned,
                     this));
  return RespondLater();
}

void WallpaperPrivateGetOfflineWallpaperListFunction::
    OnOfflineWallpaperListReturned(const std::vector<std::string>& url_list) {
  auto results = std::make_unique<base::ListValue>();
  results->AppendStrings(url_list);
  Respond(OneArgument(std::move(results)));
}

ExtensionFunction::ResponseAction
WallpaperPrivateRecordWallpaperUMAFunction::Run() {
  std::unique_ptr<record_wallpaper_uma::Params> params(
      record_wallpaper_uma::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::WallpaperType source = GetWallpaperType(params->source);
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Source", source,
                            ash::WALLPAPER_TYPE_COUNT);
  return RespondNow(NoArguments());
}

WallpaperPrivateGetCollectionsInfoFunction::
    WallpaperPrivateGetCollectionsInfoFunction() = default;

WallpaperPrivateGetCollectionsInfoFunction::
    ~WallpaperPrivateGetCollectionsInfoFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetCollectionsInfoFunction::Run() {
  collection_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::CollectionInfoFetcher>();
  collection_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched,
      this));
  return RespondLater();
}

void WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched(
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  if (!success) {
    Respond(Error("Collection names are not available."));
    return;
  }

  std::vector<extensions::api::wallpaper_private::CollectionInfo>
      collections_info_list;
  for (const auto& collection : collections) {
    extensions::api::wallpaper_private::CollectionInfo collection_info;
    collection_info.collection_name = collection.collection_name();
    collection_info.collection_id = collection.collection_id();
    collections_info_list.push_back(std::move(collection_info));
  }
  Respond(ArgumentList(
      get_collections_info::Results::Create(collections_info_list)));
}

WallpaperPrivateGetImagesInfoFunction::WallpaperPrivateGetImagesInfoFunction() =
    default;

WallpaperPrivateGetImagesInfoFunction::
    ~WallpaperPrivateGetImagesInfoFunction() = default;

ExtensionFunction::ResponseAction WallpaperPrivateGetImagesInfoFunction::Run() {
  std::unique_ptr<get_images_info::Params> params(
      get_images_info::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  image_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::ImageInfoFetcher>(
          params->collection_id);
  image_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched, this));
  return RespondLater();
}

void WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched(
    bool success,
    const std::vector<backdrop::Image>& images) {
  if (!success) {
    Respond(Error("Images info is not available."));
    return;
  }

  std::vector<extensions::api::wallpaper_private::ImageInfo> images_info_list;
  for (const auto& image : images) {
    extensions::api::wallpaper_private::ImageInfo image_info;
    ParseImageInfo(image, &image_info);
    images_info_list.push_back(std::move(image_info));
  }
  Respond(ArgumentList(get_images_info::Results::Create(images_info_list)));
}

WallpaperPrivateGetLocalImagePathsFunction::
    WallpaperPrivateGetLocalImagePathsFunction() = default;

WallpaperPrivateGetLocalImagePathsFunction::
    ~WallpaperPrivateGetLocalImagePathsFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImagePathsFunction::Run() {
  base::FilePath path = file_manager::util::GetMyFilesFolderForProfile(
      Profile::FromBrowserContext(browser_context()));
  base::PostTaskAndReplyWithResult(
      WallpaperFunctionBase::GetNonBlockingTaskRunner(), FROM_HERE,
      base::BindOnce(&GetImagePaths, path),
      base::BindOnce(
          &WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete,
          this));
  return RespondLater();
}

void WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete(
    const std::vector<std::string>& image_paths) {
  Respond(ArgumentList(get_local_image_paths::Results::Create(image_paths)));
}

WallpaperPrivateGetLocalImageDataFunction::
    WallpaperPrivateGetLocalImageDataFunction() = default;

WallpaperPrivateGetLocalImageDataFunction::
    ~WallpaperPrivateGetLocalImageDataFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImageDataFunction::Run() {
  std::unique_ptr<get_local_image_data::Params> params(
      get_local_image_data::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(crbug.com/811564): Create file backed blob instead.
  auto image_data = std::make_unique<std::string>();
  std::string* image_data_ptr = image_data.get();
  base::PostTaskAndReplyWithResult(
      WallpaperFunctionBase::GetNonBlockingTaskRunner(), FROM_HERE,
      base::BindOnce(&base::ReadFileToString,
                     base::FilePath(params->image_path), image_data_ptr),
      base::BindOnce(
          &WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete,
          this, std::move(image_data)));

  return RespondLater();
}

void WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete(
    std::unique_ptr<std::string> image_data,
    bool success) {
  if (!success) {
    Respond(Error("Reading image data failed."));
    return;
  }

  Respond(ArgumentList(get_local_image_data::Results::Create(
      std::vector<uint8_t>(image_data->begin(), image_data->end()))));
}

WallpaperPrivateConfirmPreviewWallpaperFunction::
    WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

WallpaperPrivateConfirmPreviewWallpaperFunction::
    ~WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateConfirmPreviewWallpaperFunction::Run() {
  WallpaperControllerClient::Get()->ConfirmPreviewWallpaper();
  return RespondNow(NoArguments());
}

WallpaperPrivateCancelPreviewWallpaperFunction::
    WallpaperPrivateCancelPreviewWallpaperFunction() = default;

WallpaperPrivateCancelPreviewWallpaperFunction::
    ~WallpaperPrivateCancelPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateCancelPreviewWallpaperFunction::Run() {
  WallpaperControllerClient::Get()->CancelPreviewWallpaper();
  return RespondNow(NoArguments());
}

WallpaperPrivateGetCurrentWallpaperThumbnailFunction::
    WallpaperPrivateGetCurrentWallpaperThumbnailFunction() = default;

WallpaperPrivateGetCurrentWallpaperThumbnailFunction::
    ~WallpaperPrivateGetCurrentWallpaperThumbnailFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetCurrentWallpaperThumbnailFunction::Run() {
  std::unique_ptr<get_current_wallpaper_thumbnail::Params> params(
      get_current_wallpaper_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto image = WallpaperControllerClient::Get()->GetWallpaperImage();
  gfx::Size thumbnail_size(params->thumbnail_width, params->thumbnail_height);
  image.EnsureRepsForSupportedScales();
  scoped_refptr<base::RefCountedBytes> thumbnail_data;
  GenerateThumbnail(image, thumbnail_size, &thumbnail_data);
  return RespondNow(OneArgument(std::make_unique<Value>(
      Value::BlobStorage(thumbnail_data->front(),
                         thumbnail_data->front() + thumbnail_data->size()))));
}

void WallpaperPrivateGetCurrentWallpaperThumbnailFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {}

WallpaperPrivateGetSurpriseMeImageFunction::
    WallpaperPrivateGetSurpriseMeImageFunction() = default;

WallpaperPrivateGetSurpriseMeImageFunction::
    ~WallpaperPrivateGetSurpriseMeImageFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetSurpriseMeImageFunction::Run() {
  std::unique_ptr<get_surprise_me_image::Params> params(
      get_surprise_me_image::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  surprise_me_image_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::SurpriseMeImageFetcher>(
          params->collection_id,
          params->resume_token ? *params->resume_token : std::string());
  surprise_me_image_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetSurpriseMeImageFunction::OnSurpriseMeImageFetched,
      this));
  return RespondLater();
}

void WallpaperPrivateGetSurpriseMeImageFunction::OnSurpriseMeImageFetched(
    bool success,
    const backdrop::Image& image,
    const std::string& next_resume_token) {
  if (!success) {
    Respond(Error("Image not available."));
    return;
  }

  extensions::api::wallpaper_private::ImageInfo image_info;
  ParseImageInfo(image, &image_info);
  Respond(TwoArguments(image_info.ToValue(),
                       std::make_unique<Value>(next_resume_token)));
}
