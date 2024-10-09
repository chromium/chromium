// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_misc.h"

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_export_import_factory.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_package_service_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/screen.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace extensions {
namespace {

namespace fsp = ash::file_system_provider;
namespace fmp = api::file_manager_private;
namespace fmpi = api::file_manager_private_internal;

using fmp::ProfileInfo;
using std::optional;

// Thresholds for mountCrostini() API.
constexpr base::TimeDelta kMountCrostiniSlowOperationThreshold =
    base::Seconds(10);
constexpr base::TimeDelta kMountCrostiniVerySlowOperationThreshold =
    base::Seconds(30);

// Obtains the current app window.
AppWindow* GetCurrentAppWindow(ExtensionFunction* function) {
  content::WebContents* const contents = function->GetSenderWebContents();
  return contents ? AppWindowRegistry::Get(function->browser_context())
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
    if (original_profiles.count(profile)) {
      continue;
    }
    original_profiles.insert(profile);
    const user_manager::User* const user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile);
    if (!user || !user->is_logged_in()) {
      continue;
    }

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
    fsp::ProvidedFileSystemInterface** file_system,
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
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));

    // Convert fusebox URL to its backing (FSP) file system provider URL.
    if (file_system_url.type() == storage::kFileSystemTypeFuseBox) {
      std::string fsp_url(url);
      base::ReplaceFirstSubstringAfterOffset(&fsp_url, 0, "/external/fusebox",
                                             "/external/");
      file_system_url =
          file_system_context->CrackURLInFirstPartyContext(GURL(fsp_url));
    }

    fsp::util::FileSystemURLParser parser(file_system_url);
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
                     fmp::SourceRestriction restriction) {
  switch (restriction) {
    case fmp::SourceRestriction::kNone:
      NOTREACHED_IN_MIGRATION();
      return false;

    case fmp::SourceRestriction::kAnySource:
      return true;

    case fmp::SourceRestriction::kNativeSource:
      return type == storage::kFileSystemTypeLocal;
  }
}

std::string Redact(const std::string& s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

// Gets the default location/volume that the user should use, usually MyFiles,
// based on `path`. When SkyVault is enabled the admin might choose between
// Google Drive and OneDrive. If SkyVault is misconfigured, e.g. local files are
// disabled but the download policy isn't set correctly defaults to MyFiles.
api::file_manager_private::DefaultLocation GetDefaultLocation(
    const std::string& pref) {
  if (policy::local_user_files::LocalUserFilesAllowed()) {
    // If local files are allowed, always default to MyFiles.
    return api::file_manager_private::DefaultLocation::kMyFiles;
  }

  if (pref == download_dir_util::kLocationGoogleDrive) {
    return api::file_manager_private::DefaultLocation::kGoogleDrive;
  }
  if (pref == download_dir_util::kLocationOneDrive) {
    return api::file_manager_private::DefaultLocation::kOnedrive;
  }
  // SkyVault is misconfigured - local files are disabled but no cloud location
  // is enforced as the download location.
  LOG(ERROR) << "SkyVault is misconfigured: Invalid cloud pref: " << pref
             << " defaulting to MyFiles.";
  return api::file_manager_private::DefaultLocation::kMyFiles;
}

// Converts the value of LocalUserFilesMigrationDestination policy to
// api::file_manager_private::CloudProvider. If SkyVault is misconfigured,
// e.g. local files are enabled returns kNotSpecified, regardless of the policy
// value.
api::file_manager_private::CloudProvider GetSkyVaultMigrationDestination() {
  if (policy::local_user_files::LocalUserFilesAllowed()) {
    // If local files are allowed, just return kNotSpecified.
    return api::file_manager_private::CloudProvider::kNotSpecified;
  }

  auto cloud_provider = policy::local_user_files::GetMigrationDestination();
  switch (cloud_provider) {
    case policy::local_user_files::CloudProvider::kNotSpecified:
      return api::file_manager_private::CloudProvider::kNotSpecified;
    case policy::local_user_files::CloudProvider::kGoogleDrive:
      return api::file_manager_private::CloudProvider::kGoogleDrive;
    case policy::local_user_files::CloudProvider::kOneDrive:
      return api::file_manager_private::CloudProvider::kOnedrive;
  }
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateGetPreferencesFunction::Run() {
  fmp::Preferences result;
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  DCHECK(profile);
  const PrefService* const prefs = profile->GetPrefs();
  DCHECK(prefs);
  drive::DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  result.drive_enabled = drive::util::IsDriveEnabledForProfile(profile) &&
                         service && !service->mount_failed();
  result.drive_sync_enabled_on_metered_network =
      !prefs->GetBoolean(drive::prefs::kDisableDriveOverCellular);
  result.drive_fs_bulk_pinning_available =
      drive::util::IsDriveFsBulkPinningAvailable(profile);
  result.drive_fs_bulk_pinning_enabled =
      prefs->GetBoolean(drive::prefs::kDriveFsBulkPinningEnabled);
  result.search_suggest_enabled =
      prefs->GetBoolean(prefs::kSearchSuggestEnabled);
  result.use24hour_clock = prefs->GetBoolean(prefs::kUse24HourClock);
  result.timezone = base::UTF16ToUTF8(
      ash::system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());
  result.arc_enabled = prefs->GetBoolean(arc::prefs::kArcEnabled);
  result.arc_removable_media_access_enabled =
      prefs->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);
  result.trash_enabled = file_manager::trash::IsTrashEnabledForProfile(profile);
  std::vector<std::string> folder_shortcuts;
  const auto& value_list = prefs->GetList(ash::prefs::kFilesAppFolderShortcuts);
  for (const base::Value& value : value_list) {
    folder_shortcuts.push_back(value.is_string() ? value.GetString() : "");
  }
  result.folder_shortcuts = folder_shortcuts;
  result.office_file_moved_one_drive =
      prefs->GetTime(prefs::kOfficeFileMovedToOneDrive)
          .InMillisecondsFSinceUnixEpoch();
  result.office_file_moved_google_drive =
      prefs->GetTime(prefs::kOfficeFileMovedToGoogleDrive)
          .InMillisecondsFSinceUnixEpoch();
  result.local_user_files_allowed =
      policy::local_user_files::LocalUserFilesAllowed();
  result.default_location =
      GetDefaultLocation(prefs->GetString(prefs::kFilesAppDefaultLocation));
  result.sky_vault_migration_destination = GetSkyVaultMigrationDestination();

  return RespondNow(WithArguments(result.ToValue()));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSetPreferencesFunction::Run() {
  using fmp::SetPreferences::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const fmp::PreferencesChange& change = params->change_info;
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  PrefService* const service = profile->GetPrefs();

  if (change.drive_sync_enabled_on_metered_network.has_value()) {
    service->SetBoolean(drive::prefs::kDisableDriveOverCellular,
                        !change.drive_sync_enabled_on_metered_network.value());
  }

  if (drive::util::IsDriveFsBulkPinningAvailable(profile) &&
      change.drive_fs_bulk_pinning_enabled.has_value()) {
    service->SetBoolean(drive::prefs::kDriveFsBulkPinningEnabled,
                        change.drive_fs_bulk_pinning_enabled.value());
    drivefs::pinning::RecordBulkPinningEnabledSource(
        drivefs::pinning::BulkPinningEnabledSource::kBanner);
  }

  if (change.arc_enabled.has_value()) {
    service->SetBoolean(arc::prefs::kArcEnabled, change.arc_enabled.value());
  }

  if (change.arc_removable_media_access_enabled.has_value()) {
    service->SetBoolean(arc::prefs::kArcHasAccessToRemovableMedia,
                        change.arc_removable_media_access_enabled.value());
  }

  if (change.folder_shortcuts.has_value()) {
    base::Value::List folder_shortcuts;
    for (const std::string& shortcut : change.folder_shortcuts.value()) {
      folder_shortcuts.Append(shortcut);
    }
    service->SetList(ash::prefs::kFilesAppFolderShortcuts,
                     std::move(folder_shortcuts));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateZoomFunction::Run() {
  using fmp::Zoom::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::PageZoom zoom_type;
  switch (params->operation) {
    case fmp::ZoomOperationType::kIn:
      zoom_type = content::PAGE_ZOOM_IN;
      break;
    case fmp::ZoomOperationType::kOut:
      zoom_type = content::PAGE_ZOOM_OUT;
      break;
    case fmp::ZoomOperationType::kReset:
      zoom_type = content::PAGE_ZOOM_RESET;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  zoom::PageZoom::Zoom(GetSenderWebContents(), zoom_type);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateGetProfilesFunction::Run() {
  fmp::ProfilesResponse response;
  response.profiles = GetLoggedInProfileInfoList();

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

  response.current_profile_id = current_profile_id.GetUserEmail();
  response.displayed_profile_id = display_profile_id.is_valid()
                                      ? display_profile_id.GetUserEmail()
                                      : current_profile_id.GetUserEmail();

  return RespondNow(WithArguments(response.ToValue()));
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenInspectorFunction::Run() {
  using fmp::OpenInspector::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  switch (params->type) {
    case fmp::InspectionType::kNormal:
      // Open inspector for foreground page.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents(),
                                         DevToolsOpenedByAction::kUnknown);
      break;
    case fmp::InspectionType::kConsole:
      // Open inspector for foreground page and bring focus to the console.
      DevToolsWindow::OpenDevToolsWindow(
          GetSenderWebContents(), DevToolsToggleAction::ShowConsolePanel(),
          DevToolsOpenedByAction::kUnknown);
      break;
    case fmp::InspectionType::kElement:
      // Open inspector for foreground page in inspect element mode.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents(),
                                         DevToolsToggleAction::Inspect(),
                                         DevToolsOpenedByAction::kUnknown);
      break;
    case fmp::InspectionType::kBackground:
      // Open inspector for background page if extension pointer is not null.
      // Files app SWA is not an extension and thus has no associated background
      // page.
      if (extension()) {
        devtools_util::InspectBackgroundPage(
            extension(), Profile::FromBrowserContext(browser_context()),
            DevToolsOpenedByAction::kUnknown);
      } else {
        return RespondNow(
            Error(base::StringPrintf("Inspection type(%d) not supported.",
                                     static_cast<int>(params->type))));
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return RespondNow(Error(
          base::StringPrintf("Unexpected inspection type(%d) is specified.",
                             static_cast<int>(params->type))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenSettingsSubpageFunction::Run() {
  using fmp::OpenSettingsSubpage::Params;
  const optional<Params> params = Params::Create(args());
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

FileManagerPrivateGetMimeTypeFunction::FileManagerPrivateGetMimeTypeFunction() =
    default;

FileManagerPrivateGetMimeTypeFunction::
    ~FileManagerPrivateGetMimeTypeFunction() = default;

ExtensionFunction::ResponseAction FileManagerPrivateGetMimeTypeFunction::Run() {
  using fmp::GetMimeType::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert file url to local path.
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)));

  app_file_handler_util::GetMimeTypeForLocalPath(
      profile, file_system_url.path(),
      base::BindOnce(&FileManagerPrivateGetMimeTypeFunction::OnGetMimeType,
                     this));

  return RespondLater();
}

void FileManagerPrivateGetMimeTypeFunction::OnGetMimeType(
    const std::string& mimeType) {
  Respond(WithArguments(mimeType));
}

FileManagerPrivateGetProvidersFunction::
    FileManagerPrivateGetProvidersFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetProvidersFunction::Run() {
  using fsp::Capabilities;
  using fsp::IconSet;
  using fsp::ProviderId;
  using fsp::ProviderInterface;
  using fsp::Service;
  const Service* const service = Service::Get(browser_context());

  using fmp::Provider;
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
        result_item.source = fmp::ProviderSource::kFile;
        break;
      case SOURCE_DEVICE:
        result_item.source = fmp::ProviderSource::kDevice;
        break;
      case SOURCE_NETWORK:
        result_item.source = fmp::ProviderSource::kNetwork;
        break;
    }
    result.push_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(fmp::GetProviders::Results::Create(result)));
}

FileManagerPrivateAddProvidedFileSystemFunction::
    FileManagerPrivateAddProvidedFileSystemFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateAddProvidedFileSystemFunction::Run() {
  using fmp::AddProvidedFileSystem::Params;
  using fsp::ProviderId;
  using fsp::Service;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  Service* const service = Service::Get(browser_context());
  ProviderId provider_id = ProviderId::FromString(params->provider_id);

  auto file_systems = service->GetProvidedFileSystemInfoList(provider_id);
  bool first_file_system = file_systems.empty();
  // Show Connect To OneDrive dialog only when mounting ODFS for the first time.
  // There will already a ODFS mount if the user is requesting a new mount to
  // replace the unauthenticated one.
  if (ash::cloud_upload::
          IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled(
              profile) &&
      params->provider_id == extension_misc::kODFSExtensionId &&
      first_file_system) {
    // Get Files App window, if it exists.
    Browser* browser =
        FindSystemWebAppBrowser(profile, ash::SystemWebAppType::FILE_MANAGER);
    gfx::NativeWindow modal_parent =
        browser ? browser->window()->GetNativeWindow() : nullptr;

    // This will call into service->RequestMount() if necessary. This is 'fire
    // and forget' as Files app doesn't do anything if this succeeds or fails.
    bool started = ash::cloud_upload::ShowConnectOneDriveDialog(modal_parent);
    return RespondNow(started ? NoArguments()
                              : Error("Failed to request a new mount."));
  }

  if (!service->RequestMount(provider_id, base::DoNothing())) {
    return RespondNow(Error("Failed to request a new mount."));
  }

  return RespondNow(NoArguments());
}

FileManagerPrivateConfigureVolumeFunction::
    FileManagerPrivateConfigureVolumeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateConfigureVolumeFunction::Run() {
  using fmp::ConfigureVolume::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(volume_manager);

  std::string volume_id = params->volume_id;
  volume_manager->ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(&volume_id);

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume) {
    return RespondNow(
        Error("ConfigureVolume: volume with ID * not found.", volume_id));
  }

  if (!volume->configurable()) {
    return RespondNow(Error("Volume not configurable."));
  }

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_PROVIDED: {
      using fsp::Service;
      Service* const service = Service::Get(browser_context());
      DCHECK(service);

      using fsp::ProvidedFileSystemInterface;
      ProvidedFileSystemInterface* const file_system =
          service->GetProvidedFileSystem(volume->provider_id(),
                                         volume->file_system_id());
      if (file_system) {
        file_system->Configure(base::BindOnce(
            &FileManagerPrivateConfigureVolumeFunction::OnCompleted, this));
      }
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
      crostini::DefaultContainerId(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::RestartCallback,
                     this));
  return RespondLater();
}

void FileManagerPrivateMountCrostiniFunction::RestartCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(base::StringPrintf("Error mounting crostini container: %d",
                                     static_cast<int>(result))));
    return;
  }
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->MountCrostiniFiles(
      crostini::DefaultContainerId(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::MountCallback,
                     this),
      false);
}

void FileManagerPrivateMountCrostiniFunction::MountCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(base::StringPrintf("Error mounting crostini container: %d",
                                     static_cast<int>(result))));
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
  using fmpi::ImportCrostiniImage::Params;

  const auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  base::FilePath path =
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url))
          .path();

  crostini::CrostiniExportImportFactory::GetForProfile(profile)
      ->ImportContainer(
          crostini::DefaultContainerId(), path,
          base::BindOnce(
              [](base::FilePath path, crostini::CrostiniResult result) {
                if (result != crostini::CrostiniResult::SUCCESS) {
                  LOG(ERROR) << "Error importing crostini image "
                             << Redact(path) << ": " << (int)result;
                }
              },
              path));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSharePathsWithCrostiniFunction::Run() {
  using fmpi::SharePathsWithCrostini::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  std::vector<base::FilePath> paths;
  for (const auto& url : params->urls) {
    storage::FileSystemURL cracked =
        file_system_context->CrackURLInFirstPartyContext(GURL(url));
    paths.emplace_back(cracked.path());
  }

  auto vm_info =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)->GetVmInfo(
          params->vm_name);
  auto* share_service =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile);

  share_service->RegisterPersistedPaths(params->vm_name, paths);
  if (vm_info) {
    // The share service will mount persistent shares at VM boot, but if the VM
    // is already running we need to trigger the first mount ourselves.
    share_service->SharePaths(
        params->vm_name, vm_info->seneschal_server_handle(), std::move(paths),
        base::BindOnce(
            &FileManagerPrivateInternalSharePathsWithCrostiniFunction::
                SharePathsCallback,
            this));
    return RespondLater();
  } else {
    return RespondNow(NoArguments());
  }
}

void FileManagerPrivateInternalSharePathsWithCrostiniFunction::
    SharePathsCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalUnsharePathWithCrostiniFunction::Run() {
  using fmpi::UnsharePathWithCrostini::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  storage::FileSystemURL cracked =
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url));
  guest_os::GuestOsSharePathFactory::GetForProfile(profile)->UnsharePath(
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
  using fmpi::GetCrostiniSharedPaths::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  auto* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile);
  bool first_for_session =
      params->observe_first_for_session &&
      guest_os_share_path->GetAndSetFirstForSession(params->vm_name);
  auto shared_paths =
      guest_os_share_path->GetPersistedSharedPaths(params->vm_name);
  base::Value::List entries;
  fmpi::CrostiniSharedPathResponse response;
  response.first_for_session = first_for_session;
  for (const base::FilePath& path : shared_paths) {
    std::string mount_name;
    std::string file_system_name;
    std::string full_path;
    if (!file_manager::util::ExtractMountNameFileSystemNameFullPath(
            path, &mount_name, &file_system_name, &full_path)) {
      LOG(ERROR) << "Error extracting mount name and path from "
                 << Redact(path);
      continue;
    }

    auto& entry = response.entries.emplace_back();
    entry.file_system_root =
        storage::GetExternalFileSystemRootURIString(source_url(), mount_name);
    entry.file_system_name = file_system_name;
    entry.file_full_path = full_path;
    // All shared paths should be directories.  Even if this is not true,
    // it is fine for foreground/js/crostini.js class to think so. We
    // verify that the paths are in fact valid directories before calling
    // seneschal/9p in GuestOsSharePath::CallSeneschalSharePath().
    entry.file_is_directory = true;
  }
  return RespondNow(WithArguments(response.ToValue()));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetLinuxPackageInfoFunction::Run() {
  using fmpi::GetLinuxPackageInfo::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageServiceFactory::GetForProfile(profile)
      ->GetLinuxPackageInfo(
          crostini::DefaultContainerId(),
          file_system_context->CrackURLInFirstPartyContext(GURL(params->url)),
          base::BindOnce(
              &FileManagerPrivateInternalGetLinuxPackageInfoFunction::
                  OnGetLinuxPackageInfo,
              this));
  return RespondLater();
}

void FileManagerPrivateInternalGetLinuxPackageInfoFunction::
    OnGetLinuxPackageInfo(
        const crostini::LinuxPackageInfo& linux_package_info) {
  fmp::LinuxPackageInfo result;
  if (!linux_package_info.success) {
    Respond(Error(linux_package_info.failure_reason));
    return;
  }

  result.name = linux_package_info.name;
  result.version = linux_package_info.version;
  result.summary = linux_package_info.summary;
  result.description = linux_package_info.description;

  Respond(ArgumentList(fmpi::GetLinuxPackageInfo::Results::Create(result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalInstallLinuxPackageFunction::Run() {
  using fmpi::InstallLinuxPackage::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageServiceFactory::GetForProfile(profile)
      ->QueueInstallLinuxPackage(
          crostini::DefaultContainerId(),
          file_system_context->CrackURLInFirstPartyContext(GURL(params->url)),
          base::BindOnce(
              &FileManagerPrivateInternalInstallLinuxPackageFunction::
                  OnInstallLinuxPackage,
              this));
  return RespondLater();
}

void FileManagerPrivateInternalInstallLinuxPackageFunction::
    OnInstallLinuxPackage(crostini::CrostiniResult result) {
  fmp::InstallLinuxPackageStatus response;
  switch (result) {
    case crostini::CrostiniResult::SUCCESS:
      response = fmp::InstallLinuxPackageStatus::kStarted;
      break;
    case crostini::CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED:
      response = fmp::InstallLinuxPackageStatus::kFailed;
      break;
    case crostini::CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE:
      response = fmp::InstallLinuxPackageStatus::kInstallAlreadyActive;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  Respond(ArgumentList(fmpi::InstallLinuxPackage::Results::Create(response)));
}

FileManagerPrivateInternalGetCustomActionsFunction::
    FileManagerPrivateInternalGetCustomActionsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCustomActionsFunction::Run() {
  using fmpi::GetCustomActions::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  fsp::ProvidedFileSystemInterface* file_system = nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->GetActions(
      paths,
      base::BindOnce(
          &FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted(
    const fsp::Actions& actions,
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
    item.title = action.title;
    items.push_back(std::move(item));
  }

  Respond(ArgumentList(fmpi::GetCustomActions::Results::Create(items)));
}

FileManagerPrivateInternalExecuteCustomActionFunction::
    FileManagerPrivateInternalExecuteCustomActionFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteCustomActionFunction::Run() {
  using fmpi::ExecuteCustomAction::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  fsp::ProvidedFileSystemInterface* file_system = nullptr;
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
    FileManagerPrivateInternalGetRecentFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetRecentFilesFunction::Run() {
  using fmpi::GetRecentFiles::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  ash::RecentModel* model = ash::RecentModelFactory::GetForProfile(profile);
  if (!model) {
    return RespondNow(Error("Failed to get recent model"));
  }

  ash::RecentModel::FileType file_type;
  if (!file_manager::util::ToRecentSourceFileType(params->file_category,
                                                  &file_type)) {
    return RespondNow(Error("Cannot convert category to file type"));
  }

  ash::RecentModelOptions options;
  options.now_delta = base::Days(params->cutoff_days);
  options.max_files = 1000u;
  options.invalidate_cache = params->invalidate_cache;
  options.file_type = file_type;
  options.source_specs = {
      {.volume_type = fmp::VolumeType::kCrostini},
      {.volume_type = fmp::VolumeType::kMediaView},
      {.volume_type = fmp::VolumeType::kDownloads},
      {.volume_type = fmp::VolumeType::kDrive},
      {.volume_type = fmp::VolumeType::kProvided},
  };

  // We set the maximum latency to be 3s due to File System Provider volumes.
  // As these types of volumes may be located in the cloud, they may be slow.
  // 3s is based on "User Preference and Search Engine Latency" paper, which
  // stated that "[...] once latency exceeds 3 seconds for the slower engine,
  // users are 1.5 times as likely to choose the faster engine."
  options.scan_timeout = base::Milliseconds(3000);

  model->GetRecentFiles(
      file_system_context.get(), source_url(), params->query, options,
      base::BindOnce(
          &FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles,
          this, params->restriction));
  return RespondLater();
}

void FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles(
    fmp::SourceRestriction restriction,
    const std::vector<ash::RecentFile>& files) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : files) {
    // Filter out files from non-allowed sources.
    // We do this filtering here rather than in RecentModel so that the set of
    // files returned with some restriction is a subset of what would be
    // returned without restriction. Anyway, the maximum number of files
    // returned from RecentModel is large enough.
    if (!IsAllowedSource(file.url().type(), restriction)) {
      continue;
    }

    file_manager::util::FileDefinition file_definition;
    // Recent file system only lists regular files, not directories.
    file_definition.is_directory = false;
    if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, source_url(), file.url().path(),
            &file_definition.virtual_path)) {
      file_definition_list.emplace_back(std::move(file_definition));
    }
  }

  // During this conversion a GET_METADATA_FIELD_IS_DIRECTORY will be triggered
  // in file_system_context.cc DidOpenFileSystemForResolveURL() which will
  // might not use file_definition.is_directory in some scenarios, which will
  // make entry_definition's is_directory become true, so in the callback we
  // need to filter out is_directory=true.
  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      url::Origin::Create(source_url().DeprecatedGetOriginAsURL()),
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

  // Remove all directories entries.
  std::erase_if(*entry_definition_list,
                [](const file_manager::util::EntryDefinition& e) {
                  return e.is_directory;
                });

  Respond(
      WithArguments(file_manager::util::ConvertEntryDefinitionListToListValue(
          *entry_definition_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateIsTabletModeEnabledFunction::Run() {
  return RespondNow(
      WithArguments(display::Screen::GetScreen()->InTabletMode()));
}

ExtensionFunction::ResponseAction FileManagerPrivateOpenURLFunction::Run() {
  using fmp::OpenURL::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const GURL url(params->url);

  if (!ash::NewWindowDelegate::GetPrimary()) {
    return RespondNow(
        Error("Could not get NewWindowDelegate's primary browser"));
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateOpenWindowFunction::Run() {
  using fmp::OpenWindow::Params;
  const optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL destination_folder(params->params.current_directory_url
                                    ? (*params->params.current_directory_url)
                                    : "");
  const GURL selection_url(
      params->params.selection_url ? (*params->params.selection_url) : "");

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  GURL files_swa_url =
      ::file_manager::util::GetFileManagerMainPageUrlWithParams(
          ui::SelectFileDialog::SELECT_NONE, /*title=*/{}, destination_folder,
          selection_url,
          /*target_name=*/{}, &file_type_info,
          /*file_type_index=*/0,
          /*search_query=*/{},
          /*show_android_picker_apps=*/false,
          /*volume_filter=*/{});

  ash::SystemAppLaunchParams launch_params;
  launch_params.url = files_swa_url;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               launch_params);

  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSendFeedbackFunction::Run() {
  GURL url;
  if (GetSenderWebContents()) {
    url = GetSenderWebContents()->GetVisibleURL();
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  chrome::ShowFeedbackPage(url, profile, feedback::kFeedbackSourceFilesApp,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/"chromeos-files-app",
                           /*extra_diagnostics=*/std::string());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDeviceConnectionStateFunction::Run() {
  fmp::DeviceConnectionState result =
      content::GetNetworkConnectionTracker()->IsOffline()
          ? fmp::DeviceConnectionState::kOffline
          : fmp::DeviceConnectionState::kOnline;

  return RespondNow(
      ArgumentList(fmp::GetDeviceConnectionState::Results::Create(result)));
}

}  // namespace extensions
