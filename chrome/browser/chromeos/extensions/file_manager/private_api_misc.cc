// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>
#include <vector>

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/encoding_detection.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "google_apis/drive/auth_service.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace extensions {
namespace {

using api::file_manager_private::ProfileInfo;

const char kCWSScope[] = "https://www.googleapis.com/auth/chromewebstore";

// Thresholds for mountCrostini() API.
constexpr base::TimeDelta kMountCrostiniSlowOperationThreshold =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kMountCrostiniVerySlowOperationThreshold =
    base::TimeDelta::FromSeconds(30);

// Obtains the current app window.
AppWindow* GetCurrentAppWindow(ExtensionFunction* function) {
  content::WebContents* const contents = function->GetSenderWebContents();
  return contents
             ? AppWindowRegistry::Get(function->browser_context())
                   ->GetAppWindowForWebContents(contents)
             : nullptr;
}

std::vector<ProfileInfo> GetLoggedInProfileInfoList() {
  DCHECK(user_manager::UserManager::IsInitialized());
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::set<Profile*> original_profiles;
  std::vector<ProfileInfo> result_profiles;

  for (Profile* profile : profiles) {
    // Filter the profile.
    profile = profile->GetOriginalProfile();
    if (original_profiles.count(profile))
      continue;
    original_profiles.insert(profile);
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    if (!user || !user->is_logged_in())
      continue;

    // Make a ProfileInfo.
    ProfileInfo profile_info;
    profile_info.profile_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    profile_info.display_name = base::UTF16ToUTF8(user->GetDisplayName());
    // TODO(hirono): Remove the property from the profile_info.
    profile_info.is_current_profile = true;

    result_profiles.push_back(std::move(profile_info));
  }

  return result_profiles;
}

// Converts a list of file system urls (as strings) to a pair of a provided file
// system object and a list of unique paths on the file system. In case of an
// error, false is returned and the error message set.
bool ConvertURLsToProvidedInfo(
    const scoped_refptr<storage::FileSystemContext>& file_system_context,
    const std::vector<std::string>& urls,
    chromeos::file_system_provider::ProvidedFileSystemInterface** file_system,
    std::vector<base::FilePath>* paths,
    std::string* error) {
  DCHECK(file_system);
  DCHECK(error);

  if (urls.empty()) {
    *error = "At least one file must be specified.";
    return false;
  }

  *file_system = nullptr;
  for (const auto& url : urls) {
    const storage::FileSystemURL file_system_url(
        file_system_context->CrackURL(GURL(url)));

    chromeos::file_system_provider::util::FileSystemURLParser parser(
        file_system_url);
    if (!parser.Parse()) {
      *error = "Related provided file system not found.";
      return false;
    }

    if (*file_system != nullptr) {
      if (*file_system != parser.file_system()) {
        *error = "All entries must be on the same file system.";
        return false;
      }
    } else {
      *file_system = parser.file_system();
    }
    paths->push_back(parser.file_path());
  }

  // Erase duplicates.
  std::sort(paths->begin(), paths->end());
  paths->erase(std::unique(paths->begin(), paths->end()), paths->end());

  return true;
}

bool IsAllowedSource(storage::FileSystemType type,
                     api::file_manager_private::SourceRestriction restriction) {
  switch (restriction) {
    case api::file_manager_private::SOURCE_RESTRICTION_NONE:
      NOTREACHED();
      return false;

    case api::file_manager_private::SOURCE_RESTRICTION_ANY_SOURCE:
      return true;

    case api::file_manager_private::SOURCE_RESTRICTION_NATIVE_SOURCE:
      return type == storage::kFileSystemTypeNativeLocal;
  }
}

// Encodes PNG data as a dataURL.
std::string MakeThumbnailDataUrlOnThreadPool(
    base::span<const uint8_t> png_data) {
  base::AssertLongCPUWorkAllowed();
  return base::StrCat({"data:image/png;base64,", base::Base64Encode(png_data)});
}

// Converts bitmap to a PNG image and encodes it as a dataURL.
std::string ConvertAndEncode(const SkBitmap& bitmap) {
  if (bitmap.isNull()) {
    DLOG(WARNING) << "Got an invalid bitmap";
    return std::string();
  }
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  sk_sp<SkData> png_data(image->encodeToData(SkEncodedImageFormat::kPNG, 100));
  if (!png_data) {
    DLOG(WARNING) << "Thumbnail encoding error";
    return std::string();
  }
  return MakeThumbnailDataUrlOnThreadPool(
      base::make_span(png_data->bytes(), png_data->size()));
}

// The maximum size of the input PDF file for which thumbnails are generated.
constexpr uint32_t kMaxPdfSizeInBytes = 1024u * 1024u;

// A function that performs IO operations to read and render PDF thumbnail
// Must be run by a blocking task runner.
std::string ReadLocalPdf(const base::FilePath& pdf_file_path) {
  int64_t file_size;
  if (!base::GetFileSize(pdf_file_path, &file_size)) {
    DLOG(ERROR) << "Failed to get file size of " << pdf_file_path;
    return std::string();
  }
  if (file_size > kMaxPdfSizeInBytes) {
    DLOG(ERROR) << "File " << pdf_file_path << " is too large " << file_size;
    return std::string();
  }
  std::string contents;
  if (!base::ReadFileToString(pdf_file_path, &contents)) {
    DLOG(ERROR) << "Failed to load " << pdf_file_path;
    return std::string();
  }
  return contents;
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateLogoutUserForReauthenticationFunction::Run() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context()));
  if (user) {
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user->GetAccountId(), user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
  }

  chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetPreferencesFunction::Run() {
  api::file_manager_private::Preferences result;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const service = profile->GetPrefs();
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  result.drive_enabled = drive::util::IsDriveEnabledForProfile(profile) &&
                         drive_integration_service &&
                         !drive_integration_service->mount_failed();
  result.cellular_disabled =
      service->GetBoolean(drive::prefs::kDisableDriveOverCellular);
  result.search_suggest_enabled =
      service->GetBoolean(prefs::kSearchSuggestEnabled);
  result.use24hour_clock = service->GetBoolean(prefs::kUse24HourClock);
  result.timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());
  result.arc_enabled = service->GetBoolean(arc::prefs::kArcEnabled);
  result.arc_removable_media_access_enabled =
      service->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);

  return RespondNow(OneArgument(result.ToValue()));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSetPreferencesFunction::Run() {
  using extensions::api::file_manager_private::SetPreferences::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* const service = profile->GetPrefs();

  if (params->change_info.cellular_disabled) {
    service->SetBoolean(drive::prefs::kDisableDriveOverCellular,
                        *params->change_info.cellular_disabled);
  }
  if (params->change_info.arc_enabled) {
    service->SetBoolean(arc::prefs::kArcEnabled,
                        *params->change_info.arc_enabled);
  }
  if (params->change_info.arc_removable_media_access_enabled) {
    service->SetBoolean(
        arc::prefs::kArcHasAccessToRemovableMedia,
        *params->change_info.arc_removable_media_access_enabled);
  }

  return RespondNow(NoArguments());
}

FileManagerPrivateInternalZipSelectionFunction::
    FileManagerPrivateInternalZipSelectionFunction() = default;

FileManagerPrivateInternalZipSelectionFunction::
    ~FileManagerPrivateInternalZipSelectionFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalZipSelectionFunction::Run() {
  using extensions::api::file_manager_private_internal::ZipSelection::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // First param is the parent directory URL.
  if (params->parent_url.empty())
    return RespondNow(Error("Empty parent URL."));

  const ChromeExtensionFunctionDetails chrome_details(this);
  base::FilePath src_dir = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), chrome_details.GetProfile(),
      GURL(params->parent_url));
  if (src_dir.empty())
    return RespondNow(Error("Invalid source dir."));

  // Second param is the list of selected file URLs to be zipped.
  if (params->urls.empty())
    return RespondNow(Error("No files selected to be zipped."));

  std::vector<base::FilePath> files;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    base::FilePath path = file_manager::util::GetLocalPathFromURL(
        render_frame_host(), chrome_details.GetProfile(),
        GURL(params->urls[i]));
    if (path.empty())
      return RespondNow(Error("Invalid selected file path."));
    files.push_back(path);
  }

  // Third param is the name of the output zip file.
  if (params->dest_name.empty())
    return RespondNow(Error("Empty output file name."));

  base::FilePath dest_file = src_dir.Append(params->dest_name);
  std::vector<base::FilePath> src_relative_paths;
  for (size_t i = 0; i != files.size(); ++i) {
    const base::FilePath& file_path = files[i];

    // Obtain the relative path of |file_path| under |src_dir|.
    base::FilePath relative_path;
    if (!src_dir.AppendRelativePath(file_path, &relative_path))
      return RespondNow(Error("Invalid selected file path."));
    src_relative_paths.push_back(relative_path);
  }

  (new ZipFileCreator(
       base::BindOnce(
           &FileManagerPrivateInternalZipSelectionFunction::OnZipDone, this),
       src_dir, src_relative_paths, dest_file))
      ->Start(LaunchFileUtilService());
  return RespondLater();
}

void FileManagerPrivateInternalZipSelectionFunction::OnZipDone(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

ExtensionFunction::ResponseAction FileManagerPrivateZoomFunction::Run() {
  using extensions::api::file_manager_private::Zoom::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  content::PageZoom zoom_type;
  switch (params->operation) {
    case api::file_manager_private::ZOOM_OPERATION_TYPE_IN:
      zoom_type = content::PAGE_ZOOM_IN;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_OUT:
      zoom_type = content::PAGE_ZOOM_OUT;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_RESET:
      zoom_type = content::PAGE_ZOOM_RESET;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  zoom::PageZoom::Zoom(GetSenderWebContents(), zoom_type);
  return RespondNow(NoArguments());
}

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    FileManagerPrivateRequestWebStoreAccessTokenFunction()
    : chrome_details_(this) {}

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    ~FileManagerPrivateRequestWebStoreAccessTokenFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateRequestWebStoreAccessTokenFunction::Run() {
  std::vector<std::string> scopes;
  scopes.emplace_back(kCWSScope);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(chrome_details_.GetProfile());

  if (!identity_manager) {
    drive::EventLogger* logger =
        file_manager::util::GetLogger(chrome_details_.GetProfile());
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS Access token fetch failed. IdentityManager can't "
                  "be retrieved.");
    }
    return RespondNow(Error("Unable to fetch token."));
  }

  // "Unconsented" because this class doesn't care about browser sync consent.
  auth_service_ = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kNotRequired),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      scopes);
  auth_service_->StartAuthentication(
      base::BindOnce(&FileManagerPrivateRequestWebStoreAccessTokenFunction::
                         OnAccessTokenFetched,
                     this));

  return RespondLater();
}

void FileManagerPrivateRequestWebStoreAccessTokenFunction::OnAccessTokenFetched(
    google_apis::DriveApiErrorCode code,
    const std::string& access_token) {
  drive::EventLogger* logger =
      file_manager::util::GetLogger(chrome_details_.GetProfile());

  if (code == google_apis::HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    DCHECK(access_token == auth_service_->access_token());
    if (logger)
      logger->Log(logging::LOG_INFO, "CWS OAuth token fetch succeeded.");
    Respond(OneArgument(std::make_unique<base::Value>(access_token)));
  } else {
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS OAuth token fetch failed. (DriveApiErrorCode: %s)",
                  google_apis::DriveApiErrorCodeToString(code).c_str());
    }
    Respond(Error("Token fetch failed."));
  }
}

ExtensionFunction::ResponseAction FileManagerPrivateGetProfilesFunction::Run() {
  const std::vector<ProfileInfo>& profiles = GetLoggedInProfileInfoList();

  // Obtains the display profile ID.
  AppWindow* const app_window = GetCurrentAppWindow(this);
  ash::MultiUserWindowManager* const window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  const AccountId current_profile_id = multi_user_util::GetAccountIdFromProfile(
      Profile::FromBrowserContext(browser_context()));
  const AccountId display_profile_id =
      window_manager && app_window ? window_manager->GetUserPresentingWindow(
                                         app_window->GetNativeWindow())
                                   : EmptyAccountId();

  return RespondNow(
      ArgumentList(api::file_manager_private::GetProfiles::Results::Create(
          profiles, current_profile_id.GetUserEmail(),
          display_profile_id.is_valid() ? display_profile_id.GetUserEmail()
                                        : current_profile_id.GetUserEmail())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenInspectorFunction::Run() {
  using extensions::api::file_manager_private::OpenInspector::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  switch (params->type) {
    case extensions::api::file_manager_private::INSPECTION_TYPE_NORMAL:
      // Open inspector for foreground page.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_CONSOLE:
      // Open inspector for foreground page and bring focus to the console.
      DevToolsWindow::OpenDevToolsWindow(
          GetSenderWebContents(), DevToolsToggleAction::ShowConsolePanel());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_ELEMENT:
      // Open inspector for foreground page in inspect element mode.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents(),
                                         DevToolsToggleAction::Inspect());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_BACKGROUND:
      // Open inspector for background page.
      extensions::devtools_util::InspectBackgroundPage(
          extension(), Profile::FromBrowserContext(browser_context()));
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(
          base::StringPrintf("Unexpected inspection type(%d) is specified.",
                             static_cast<int>(params->type))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenSettingsSubpageFunction::Run() {
  using extensions::api::file_manager_private::OpenSettingsSubpage::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (chromeos::settings::IsOSSettingsSubPage(params->sub_page)) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, params->sub_page);
  } else {
    chrome::ShowSettingsSubPageForProfile(profile, params->sub_page);
  }
  return RespondNow(NoArguments());
}

FileManagerPrivateInternalGetMimeTypeFunction::
    FileManagerPrivateInternalGetMimeTypeFunction() = default;

FileManagerPrivateInternalGetMimeTypeFunction::
    ~FileManagerPrivateInternalGetMimeTypeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetMimeTypeFunction::Run() {
  using extensions::api::file_manager_private_internal::GetMimeType::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert file url to local path.
  const ChromeExtensionFunctionDetails chrome_details(this);
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details.GetProfile(), render_frame_host());

  storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->url)));

  app_file_handler_util::GetMimeTypeForLocalPath(
      chrome_details.GetProfile(), file_system_url.path(),
      base::BindOnce(
          &FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType, this));

  return RespondLater();
}

void FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType(
    const std::string& mimeType) {
  Respond(OneArgument(std::make_unique<base::Value>(mimeType)));
}

FileManagerPrivateGetProvidersFunction::FileManagerPrivateGetProvidersFunction()
    : chrome_details_(this) {}

ExtensionFunction::ResponseAction
FileManagerPrivateGetProvidersFunction::Run() {
  using chromeos::file_system_provider::Capabilities;
  using chromeos::file_system_provider::IconSet;
  using chromeos::file_system_provider::ProviderId;
  using chromeos::file_system_provider::ProviderInterface;
  using chromeos::file_system_provider::Service;
  const Service* const service = Service::Get(chrome_details_.GetProfile());

  using api::file_manager_private::Provider;
  std::vector<Provider> result;
  for (const auto& pair : service->GetProviders()) {
    const ProviderInterface* const provider = pair.second.get();
    const ProviderId provider_id = provider->GetId();

    Provider result_item;
    result_item.provider_id = provider->GetId().ToString();
    const IconSet& icon_set = provider->GetIconSet();
    file_manager::util::FillIconSet(&result_item.icon_set, icon_set);
    result_item.name = provider->GetName();

    const Capabilities capabilities = provider->GetCapabilities();
    result_item.configurable = capabilities.configurable;
    result_item.watchable = capabilities.watchable;
    result_item.multiple_mounts = capabilities.multiple_mounts;
    switch (capabilities.source) {
      case SOURCE_FILE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_FILE;
        break;
      case SOURCE_DEVICE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_DEVICE;
        break;
      case SOURCE_NETWORK:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_NETWORK;
        break;
    }
    result.push_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetProviders::Results::Create(result)));
}

FileManagerPrivateAddProvidedFileSystemFunction::
    FileManagerPrivateAddProvidedFileSystemFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
FileManagerPrivateAddProvidedFileSystemFunction::Run() {
  using extensions::api::file_manager_private::AddProvidedFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using chromeos::file_system_provider::Service;
  using chromeos::file_system_provider::ProvidingExtensionInfo;
  using chromeos::file_system_provider::ProviderId;
  Service* const service = Service::Get(chrome_details_.GetProfile());

  if (!service->RequestMount(ProviderId::FromString(params->provider_id)))
    return RespondNow(Error("Failed to request a new mount."));

  return RespondNow(NoArguments());
}

FileManagerPrivateConfigureVolumeFunction::
    FileManagerPrivateConfigureVolumeFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
FileManagerPrivateConfigureVolumeFunction::Run() {
  using extensions::api::file_manager_private::ConfigureVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager =
      VolumeManager::Get(chrome_details_.GetProfile());
  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return RespondNow(Error("Volume not found."));
  if (!volume->configurable())
    return RespondNow(Error("Volume not configurable."));

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_PROVIDED: {
      using chromeos::file_system_provider::Service;
      Service* const service = Service::Get(chrome_details_.GetProfile());
      DCHECK(service);

      using chromeos::file_system_provider::ProvidedFileSystemInterface;
      ProvidedFileSystemInterface* const file_system =
          service->GetProvidedFileSystem(volume->provider_id(),
                                         volume->file_system_id());
      if (file_system)
        file_system->Configure(base::BindOnce(
            &FileManagerPrivateConfigureVolumeFunction::OnCompleted, this));
      break;
    }
    default:
      NOTIMPLEMENTED();
  }

  return RespondLater();
}

void FileManagerPrivateConfigureVolumeFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to complete configuration."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateMountCrostiniFunction::
    FileManagerPrivateMountCrostiniFunction() {
  // Mounting crostini shares may require the crostini VM to be started.
  SetWarningThresholds(kMountCrostiniSlowOperationThreshold,
                       kMountCrostiniVerySlowOperationThreshold);
}

FileManagerPrivateMountCrostiniFunction::
    ~FileManagerPrivateMountCrostiniFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateMountCrostiniFunction::Run() {
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::ContainerId::GetDefault(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::RestartCallback,
                     this));
  return RespondLater();
}

void FileManagerPrivateMountCrostiniFunction::RestartCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(
        base::StringPrintf("Error mounting crostini container: %d", result)));
    return;
  }
  Respond(NoArguments());
}

FileManagerPrivateInternalImportCrostiniImageFunction::
    FileManagerPrivateInternalImportCrostiniImageFunction() = default;

FileManagerPrivateInternalImportCrostiniImageFunction::
    ~FileManagerPrivateInternalImportCrostiniImageFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalImportCrostiniImageFunction::Run() {
  using extensions::api::file_manager_private_internal::ImportCrostiniImage::
      Params;

  const auto params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  base::FilePath path = file_system_context->CrackURL(GURL(params->url)).path();

  crostini::CrostiniExportImport::GetForProfile(profile)->ImportContainer(
      crostini::ContainerId::GetDefault(), path,
      base::BindOnce(
          [](base::FilePath path, crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              LOG(ERROR) << "Error importing crostini image " << path.value()
                         << ": " << (int)result;
            }
          },
          path));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSharePathsWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::SharePathsWithCrostini::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  std::vector<base::FilePath> paths;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    storage::FileSystemURL cracked =
        file_system_context->CrackURL(GURL(params->urls[i]));
    paths.emplace_back(cracked.path());
  }

  guest_os::GuestOsSharePath::GetForProfile(profile)->SharePaths(
      params->vm_name, std::move(paths), params->persist,
      base::BindOnce(&FileManagerPrivateInternalSharePathsWithCrostiniFunction::
                         SharePathsCallback,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalSharePathsWithCrostiniFunction::
    SharePathsCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalUnsharePathWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::
      UnsharePathWithCrostini::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  storage::FileSystemURL cracked =
      file_system_context->CrackURL(GURL(params->url));
  guest_os::GuestOsSharePath::GetForProfile(profile)->UnsharePath(
      params->vm_name, cracked.path(), /*unpersist=*/true,
      base::BindOnce(
          &FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
              UnsharePathCallback,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
    UnsharePathCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCrostiniSharedPathsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCrostiniSharedPaths::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(crbug.com/1057591): Unexpected crashes in
  // GuestOsSharePath::GetPersistedSharedPaths with null profile_.
  CHECK(browser_context());
  Profile* profile = Profile::FromBrowserContext(browser_context());
  CHECK(profile);

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile);
  CHECK(guest_os_share_path);
  bool first_for_session = params->observe_first_for_session &&
                           guest_os_share_path->GetAndSetFirstForSession();
  auto shared_paths =
      guest_os_share_path->GetPersistedSharedPaths(params->vm_name);
  auto entries = std::make_unique<base::ListValue>();
  for (const base::FilePath& path : shared_paths) {
    std::string mount_name;
    std::string file_system_name;
    std::string full_path;
    if (!file_manager::util::ExtractMountNameFileSystemNameFullPath(
            path, &mount_name, &file_system_name, &full_path)) {
      LOG(ERROR) << "Error extracting mount name and path from "
                 << path.value();
      continue;
    }
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(
        "fileSystemRoot",
        storage::GetExternalFileSystemRootURIString(
            extensions::Extension::GetBaseURLFromExtensionId(extension_id()),
            mount_name));
    entry->SetString("fileSystemName", file_system_name);
    entry->SetString("fileFullPath", full_path);
    // All shared paths should be directories.  Even if this is not true,
    // it is fine for foreground/js/crostini.js class to think so. We
    // verify that the paths are in fact valid directories before calling
    // seneschal/9p in GuestOsSharePath::CallSeneschalSharePath().
    entry->SetBoolean("fileIsDirectory", true);
    entries->Append(std::move(entry));
  }
  return RespondNow(TwoArguments(
      std::move(entries), std::make_unique<base::Value>(first_for_session)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetLinuxPackageInfoFunction::Run() {
  using api::file_manager_private_internal::GetLinuxPackageInfo::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)->GetLinuxPackageInfo(
      crostini::ContainerId::GetDefault(),
      file_system_context->CrackURL(GURL(params->url)),
      base::BindOnce(&FileManagerPrivateInternalGetLinuxPackageInfoFunction::
                         OnGetLinuxPackageInfo,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalGetLinuxPackageInfoFunction::
    OnGetLinuxPackageInfo(
        const crostini::LinuxPackageInfo& linux_package_info) {
  api::file_manager_private::LinuxPackageInfo result;
  if (!linux_package_info.success) {
    Respond(Error(linux_package_info.failure_reason));
    return;
  }

  result.name = linux_package_info.name;
  result.version = linux_package_info.version;
  result.summary = std::make_unique<std::string>(linux_package_info.summary);
  result.description =
      std::make_unique<std::string>(linux_package_info.description);

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           GetLinuxPackageInfo::Results::Create(result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalInstallLinuxPackageFunction::Run() {
  using extensions::api::file_manager_private_internal::InstallLinuxPackage::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)
      ->QueueInstallLinuxPackage(
          crostini::ContainerId::GetDefault(),
          file_system_context->CrackURL(GURL(params->url)),
          base::BindOnce(
              &FileManagerPrivateInternalInstallLinuxPackageFunction::
                  OnInstallLinuxPackage,
              this));
  return RespondLater();
}

void FileManagerPrivateInternalInstallLinuxPackageFunction::
    OnInstallLinuxPackage(crostini::CrostiniResult result) {
  extensions::api::file_manager_private::InstallLinuxPackageResponse response;
  switch (result) {
    case crostini::CrostiniResult::SUCCESS:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_STARTED;
      break;
    case crostini::CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_FAILED;
      break;
    case crostini::CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_INSTALL_ALREADY_ACTIVE;
      break;
    default:
      NOTREACHED();
  }
  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           InstallLinuxPackage::Results::Create(response)));
}

FileManagerPrivateInternalGetCustomActionsFunction::
    FileManagerPrivateInternalGetCustomActionsFunction()
    : chrome_details_(this) {}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCustomActionsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCustomActions::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  std::vector<base::FilePath> paths;
  chromeos::file_system_provider::ProvidedFileSystemInterface* file_system =
      nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->GetActions(
      paths,
      base::Bind(
          &FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted(
    const chromeos::file_system_provider::Actions& actions,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to fetch actions."));
    return;
  }

  using api::file_system_provider::Action;
  std::vector<Action> items;
  for (const auto& action : actions) {
    Action item;
    item.id = action.id;
    item.title = std::make_unique<std::string>(action.title);
    items.push_back(std::move(item));
  }

  Respond(ArgumentList(
      api::file_manager_private_internal::GetCustomActions::Results::Create(
          items)));
}

FileManagerPrivateInternalExecuteCustomActionFunction::
    FileManagerPrivateInternalExecuteCustomActionFunction()
    : chrome_details_(this) {}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteCustomActionFunction::Run() {
  using extensions::api::file_manager_private_internal::ExecuteCustomAction::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  std::vector<base::FilePath> paths;
  chromeos::file_system_provider::ProvidedFileSystemInterface* file_system =
      nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->ExecuteAction(
      paths, params->action_id,
      base::BindOnce(
          &FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to execute the action."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateInternalGetRecentFilesFunction::
    FileManagerPrivateInternalGetRecentFilesFunction()
    : chrome_details_(this) {}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetRecentFilesFunction::Run() {
  using extensions::api::file_manager_private_internal::GetRecentFiles::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  chromeos::RecentModel* model =
      chromeos::RecentModel::GetForProfile(chrome_details_.GetProfile());

  chromeos::RecentModel::FileType file_type;
  switch (params->file_type) {
    case api::file_manager_private::RECENT_FILE_TYPE_ALL:
      file_type = chromeos::RecentModel::FileType::kAll;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_AUDIO:
      file_type = chromeos::RecentModel::FileType::kAudio;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_IMAGE:
      file_type = chromeos::RecentModel::FileType::kImage;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_VIDEO:
      file_type = chromeos::RecentModel::FileType::kVideo;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error("Unknown recent file type is specified."));
  }

  model->GetRecentFiles(
      file_system_context.get(),
      Extension::GetBaseURLFromExtensionId(extension_id()), file_type,
      base::BindOnce(
          &FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles,
          this, params->restriction));
  return RespondLater();
}

void FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles(
    api::file_manager_private::SourceRestriction restriction,
    const std::vector<chromeos::RecentFile>& files) {
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : files) {
    // Filter out files from non-allowed sources.
    // We do this filtering here rather than in RecentModel so that the set of
    // files returned with some restriction is a subset of what would be
    // returned without restriction. Anyway, the maximum number of files
    // returned from RecentModel is large enough.
    if (!IsAllowedSource(file.url().type(), restriction))
      continue;

    file_manager::util::FileDefinition file_definition;
    // Recent file system only lists regular files, not directories.
    file_definition.is_directory = false;
    if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            chrome_details_.GetProfile(), extension_id(), file.url().path(),
            &file_definition.virtual_path)) {
      file_definition_list.emplace_back(std::move(file_definition));
    }
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      chrome_details_.GetProfile(), extension_id(),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(&FileManagerPrivateInternalGetRecentFilesFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this));
}

void FileManagerPrivateInternalGetRecentFilesFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK(entry_definition_list);

  Respond(OneArgument(file_manager::util::ConvertEntryDefinitionListToListValue(
      *entry_definition_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateDetectCharacterEncodingFunction::Run() {
  using extensions::api::file_manager_private::DetectCharacterEncoding::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string input;
  if (!base::HexStringToString(params->bytes, &input))
    input.clear();

  std::string encoding;
  bool success = base::DetectEncoding(input, &encoding);
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      success ? std::move(encoding) : std::string())));
}

FileManagerPrivateInternalGetThumbnailFunction::
    FileManagerPrivateInternalGetThumbnailFunction() = default;

FileManagerPrivateInternalGetThumbnailFunction::
    ~FileManagerPrivateInternalGetThumbnailFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetThumbnailFunction::Run() {
  using extensions::api::file_manager_private_internal::GetThumbnail::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const ChromeExtensionFunctionDetails chrome_details(this);
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details.GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeNativeLocal:
      return GetLocalThumbnail(chrome_details, file_system_url,
                               params->crop_to_square);
    case storage::kFileSystemTypeDriveFs:
      return GetDrivefsThumbnail(chrome_details, file_system_url,
                                 params->crop_to_square);
    default:
      return RespondNow(Error(base::StringPrintf(
          "Unsupported file system type: %d", file_system_url.type())));
  }
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetThumbnailFunction::GetLocalThumbnail(
    const ChromeExtensionFunctionDetails& chrome_details,
    const storage::FileSystemURL& url,
    bool crop_to_square) {
  base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), chrome_details.GetProfile(), url.ToGURL());
  if (path.empty() ||
      base::FilePath::CompareIgnoreCase(path.Extension(), ".pdf") != 0) {
    return RespondNow(Error("Can only handle PDF files"));
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadLocalPdf, std::move(path)),
      base::BindOnce(
          &FileManagerPrivateInternalGetThumbnailFunction::FetchPdfThumbnail,
          this, crop_to_square));
  return RespondLater();
}

void FileManagerPrivateInternalGetThumbnailFunction::FetchPdfThumbnail(
    bool crop_to_square,
    const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (content.empty()) {
    Respond(Error("Failed to read PDF file"));
    return;
  }
  auto pdf_region = base::ReadOnlySharedMemoryRegion::Create(content.size());
  if (!pdf_region.IsValid()) {
    Respond(Error("Failed allocate memory for PDF file"));
    return;
  }
  memcpy(pdf_region.mapping.memory(), content.data(), content.size());
  DCHECK(!pdf_thumbnailer_.is_bound());
  GetPrintingService()->BindPdfThumbnailer(
      pdf_thumbnailer_.BindNewPipeAndPassReceiver());
  pdf_thumbnailer_.set_disconnect_handler(base::BindOnce(
      &FileManagerPrivateInternalGetThumbnailFunction::PdfThumbnailDisconected,
      base::Unretained(this)));
  gfx::Size thumb_size =
      crop_to_square
          ? gfx::Size(FileManagerPrivateInternalGetThumbnailFunction::kSize,
                      FileManagerPrivateInternalGetThumbnailFunction::kSize)
          : gfx::Size(FileManagerPrivateInternalGetThumbnailFunction::kWidth,
                      FileManagerPrivateInternalGetThumbnailFunction::kHeight);
  auto params = printing::mojom::ThumbParams::New(
      thumb_size,
      gfx::Size(FileManagerPrivateInternalGetThumbnailFunction::kDpi,
                FileManagerPrivateInternalGetThumbnailFunction::kDpi),
      /*stretch_to_bounds=*/false, /*keep_aspect_ratio=*/true);
  pdf_thumbnailer_->GetThumbnail(
      std::move(params), std::move(pdf_region.region),
      base::BindOnce(
          &FileManagerPrivateInternalGetThumbnailFunction::GotPdfThumbnail,
          this));
}

void FileManagerPrivateInternalGetThumbnailFunction::PdfThumbnailDisconected() {
  DLOG(WARNING) << "PDF thumbnail disconnected";
  Respond(Error("PDF service disconnected"));
}

void FileManagerPrivateInternalGetThumbnailFunction::GotPdfThumbnail(
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pdf_thumbnailer_.reset();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ConvertAndEncode, bitmap),
      base::BindOnce(
          &FileManagerPrivateInternalGetThumbnailFunction::SendEncodedThumbnail,
          this));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetThumbnailFunction::GetDrivefsThumbnail(
    const ChromeExtensionFunctionDetails& chrome_details,
    const storage::FileSystemURL& url,
    bool crop_to_square) {
  // If the thumbnail is generated by drivefs give it a bit more time
  // before issuing warnings about slow operation.
  SetWarningThresholds(base::TimeDelta::FromSeconds(5),
                       base::TimeDelta::FromMinutes(1));
  if (url.type() != storage::kFileSystemTypeDriveFs) {
    return RespondNow(Error("Invalid URL"));
  }
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          chrome_details.GetProfile());
  if (!drive_integration_service) {
    return RespondNow(Error("Drive service not available"));
  }
  base::FilePath path;
  if (!drive_integration_service->GetRelativeDrivePath(url.path(), &path)) {
    return RespondNow(Error("File not found"));
  }
  auto* drivefs_interface = drive_integration_service->GetDriveFsInterface();
  if (!drivefs_interface) {
    return RespondNow(Error("Drivefs not available"));
  }
  drivefs_interface->GetThumbnail(
      path, crop_to_square,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&FileManagerPrivateInternalGetThumbnailFunction::
                             GotDriveThumbnail,
                         this),
          base::nullopt));
  return RespondLater();
}

void FileManagerPrivateInternalGetThumbnailFunction::GotDriveThumbnail(
    const base::Optional<std::vector<uint8_t>>& data) {
  if (!data) {
    Respond(OneArgument(std::make_unique<base::Value>("")));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&MakeThumbnailDataUrlOnThreadPool, *data),
      base::BindOnce(
          &FileManagerPrivateInternalGetThumbnailFunction::SendEncodedThumbnail,
          this));
}

void FileManagerPrivateInternalGetThumbnailFunction::SendEncodedThumbnail(
    std::string thumbnail_data_url) {
  Respond(OneArgument(
      std::make_unique<base::Value>(std::move(thumbnail_data_url))));
}
}  // namespace extensions
