// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/trusted_sources_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/save_page_type.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/json/values_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using safe_browsing::FileTypePolicies;

namespace {

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
bool DownloadPathIsDangerous(const base::FilePath& download_path) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::FilePath home_dir = base::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  // Neither Fuchsia nor Android have a desktop dir.
  return false;
#else
  base::FilePath desktop_dir;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    return false;
  }
  return (download_path == desktop_dir);
#endif
}

base::FilePath::StringType StringToFilePathString(const std::string& src) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(src);
#else
  return src;
#endif
}

class DefaultDownloadDirectory {
 public:
  DefaultDownloadDirectory(const DefaultDownloadDirectory&) = delete;
  DefaultDownloadDirectory& operator=(const DefaultDownloadDirectory&) = delete;

  const base::FilePath& path() const { return path_; }

  void Initialize() {
    if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path_)) {
      base::GetTempDir(&path_);
    }
    if (DownloadPathIsDangerous(path_)) {
      // This is only useful on platforms that support
      // DIR_DEFAULT_DOWNLOADS_SAFE.
      base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path_);
    }
  }

 private:
  friend class base::NoDestructor<DefaultDownloadDirectory>;

  DefaultDownloadDirectory() { Initialize(); }

  base::FilePath path_;
};

DefaultDownloadDirectory& GetDefaultDownloadDirectorySingleton() {
  static base::NoDestructor<DefaultDownloadDirectory> instance;
  return *instance;
}

}  // namespace

DownloadPrefs::DownloadPrefs(Profile* profile) : profile_(profile) {
  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, the default download directory is different for each profile.
  // If the profile-unaware default path (from GetDefaultDownloadDirectory())
  // is set (this happens during the initial preference registration in static
  // RegisterProfilePrefs()), alter by GetDefaultDownloadDirectoryForProfile().
  // file_manager::util::MigratePathFromOldFormat will do this.
  const char* const kPathPrefs[] = {prefs::kSaveFileDefaultDirectory,
                                    prefs::kDownloadDefaultDirectory};
  for (const char* path_pref : kPathPrefs) {
    const PrefService::Preference* pref = prefs->FindPreference(path_pref);
    // Update the download directory if the pref is from user pref store or
    // default pref.
    if (pref->IsUserControlled()) {
      const base::FilePath current = prefs->GetFilePath(path_pref);
      base::FilePath migrated;
      if (!current.empty() &&
          file_manager::util::MigratePathFromOldFormat(
              profile_, GetDefaultDownloadDirectory(), current, &migrated)) {
        prefs->SetFilePath(path_pref, migrated);
      } else if (file_manager::util::MigrateToDriveFs(profile_, current,
                                                      &migrated)) {
        prefs->SetFilePath(path_pref, migrated);
      } else if (download_dir_util::ExpandDrivePolicyVariable(profile_, current,
                                                              &migrated)) {
        prefs->SetFilePath(path_pref, migrated);
      }
    } else if (pref->IsDefaultValue()) {
      // For default pref, the default download dir is set when profile is not
      // initialized. As a result, reset the default pref value now.
      prefs->SetDefaultPrefValue(
          path_pref,
          base::FilePathToValue(GetDefaultDownloadDirectoryForProfile()));
    }
  }

  // Ensure that the default download directory exists.
  content::DownloadManager::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                                GetDefaultDownloadDirectoryForProfile()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  should_open_pdf_in_system_reader_ =
      prefs->GetBoolean(prefs::kOpenPdfDownloadInSystemReader);
#endif
  // Update the download directory if the pref is from user pref store.
  if (prefs->FindPreference(prefs::kDownloadDefaultDirectory)
          ->IsUserControlled()) {
    base::FilePath current_download_dir =
        prefs->GetFilePath(prefs::kDownloadDefaultDirectory);
    if (!current_download_dir.IsAbsolute()) {
      // If we have a relative path or an empty path, we should reset to a safe,
      // well-known path.
      prefs->SetFilePath(prefs::kDownloadDefaultDirectory,
                         GetDefaultDownloadDirectoryForProfile());
    } else if (!prefs->GetBoolean(prefs::kDownloadDirUpgraded)) {
      // If the download path is dangerous we forcefully reset it. But if we do
      // so we set a flag to make sure we only do it once, to avoid fighting
      // the user if they really want it on an unsafe place such as the desktop.
      if (DownloadPathIsDangerous(current_download_dir)) {
        prefs->SetFilePath(prefs::kDownloadDefaultDirectory,
                           GetDefaultDownloadDirectoryForProfile());
      }
      prefs->SetBoolean(prefs::kDownloadDirUpgraded, true);
    }
  }

  prompt_for_download_.Init(prefs::kPromptForDownload, prefs);
#if BUILDFLAG(IS_ANDROID)
  prompt_for_download_android_.Init(prefs::kPromptForDownloadAndroid, prefs);
  RecordDownloadPromptStatus(
      static_cast<DownloadPromptStatus>(*prompt_for_download_android_));
  auto_open_pdf_enabled_.Init(prefs::kAutoOpenPdfEnabled, prefs);
#endif
  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs);
  save_file_path_.Init(prefs::kSaveFileDefaultDirectory, prefs);
  save_file_type_.Init(prefs::kSaveFileType, prefs);
  safebrowsing_for_trusted_sources_enabled_.Init(
      prefs::kSafeBrowsingForTrustedSourcesEnabled, prefs);
  download_restriction_.Init(prefs::kDownloadRestrictions, prefs);

  pref_change_registrar_.Add(
      prefs::kDownloadExtensionsToOpenByPolicy,
      base::BindRepeating(&DownloadPrefs::UpdateAutoOpenByPolicy,
                          // This unretained is safe since this callback is
                          // only held while this instance is alive, so this
                          // will always be valid.
                          base::Unretained(this)));
  UpdateAutoOpenByPolicy();

  pref_change_registrar_.Add(
      prefs::kDownloadAllowedURLsForOpenByPolicy,
      base::BindRepeating(&DownloadPrefs::UpdateAllowedURLsForOpenByPolicy,
                          // This unretained is safe since this callback is
                          // only held while this instance is alive, so this
                          // will always be valid.
                          base::Unretained(this)));
  UpdateAllowedURLsForOpenByPolicy();

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  std::string user_extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);

  for (const auto& extension_string :
       base::SplitString(user_extensions_to_open, ":", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    base::FilePath::StringType extension =
        StringToFilePathString(extension_string);
    // If it's empty or malformed or not allowed to open automatically, then
    // skip the entry. Any such entries will be dropped from preferences the
    // next time SaveAutoOpenState() is called.
    if (extension.empty() ||
        *extension.begin() == base::FilePath::kExtensionSeparator) {
      continue;
    }
    // Construct something like ".<extension>", since
    // IsAllowedToOpenAutomatically() needs a filename.
    base::FilePath filename_with_extension = base::FilePath(
        base::FilePath::StringType(1, base::FilePath::kExtensionSeparator) +
        extension);

    // Note that the list of file types that are not allowed to open
    // automatically can change in the future. When the list is tightened, it is
    // expected that some entries in the users' auto open list will get dropped
    // permanently as a result.
    if (FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
            filename_with_extension)) {
      auto_open_by_user_.insert(extension);
    }
  }
}

DownloadPrefs::~DownloadPrefs() {}

// static
void DownloadPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPromptForDownload,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDownloadExtensionsToOpen, std::string());
  registry->RegisterListPref(prefs::kDownloadExtensionsToOpenByPolicy, {});
  registry->RegisterListPref(prefs::kDownloadAllowedURLsForOpenByPolicy, {});
  registry->RegisterBooleanPref(prefs::kDownloadDirUpgraded, false);
  registry->RegisterIntegerPref(prefs::kSaveFileType,
                                content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
  registry->RegisterIntegerPref(prefs::kDownloadRestrictions, 0);
  // The following two prefs are ignored on ChromeOS Lacros if SysUI integration
  // is enabled.
  // TODO(chlily): Clean them up once SysUI integration is enabled by default.
  registry->RegisterBooleanPref(prefs::kDownloadBubblePartialViewEnabled, true);
  registry->RegisterIntegerPref(prefs::kDownloadBubblePartialViewImpressions,
                                0);

  registry->RegisterBooleanPref(prefs::kSafeBrowsingForTrustedSourcesEnabled,
                                true);

  const base::FilePath& default_download_path = GetDefaultDownloadDirectory();
  registry->RegisterFilePathPref(prefs::kDownloadDefaultDirectory,
                                 default_download_path);
  registry->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                 default_download_path);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(prefs::kOpenPdfDownloadInSystemReader, false);
#endif
#if BUILDFLAG(IS_ANDROID)
  DownloadPromptStatus download_prompt_status =
      DownloadPromptStatus::SHOW_INITIAL;

  registry->RegisterIntegerPref(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(download_prompt_status),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(prefs::kShowMissingSdCardErrorAndroid, true);
  registry->RegisterBooleanPref(prefs::kAutoOpenPdfEnabled, false);
  registry->RegisterListPref(prefs::kDownloadAppVerificationPromptTimestamps,
                             {});
#endif
}

base::FilePath DownloadPrefs::GetDefaultDownloadDirectoryForProfile() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return file_manager::util::GetDownloadsFolderForProfile(profile_);
#else
  return GetDefaultDownloadDirectory();
#endif
}

// static
const base::FilePath& DownloadPrefs::GetDefaultDownloadDirectory() {
  return GetDefaultDownloadDirectorySingleton().path();
}

// static
DownloadPrefs* DownloadPrefs::FromDownloadManager(
    DownloadManager* download_manager) {
  DCHECK(download_manager->GetBrowserContext());
  DownloadCoreService* service =
      DownloadCoreServiceFactory::GetForBrowserContext(
          download_manager->GetBrowserContext());
  DCHECK(service);
  ChromeDownloadManagerDelegate* delegate =
      service->GetDownloadManagerDelegate();
  DCHECK(delegate);
  return delegate->download_prefs();
}

// static
DownloadPrefs* DownloadPrefs::FromBrowserContext(
    content::BrowserContext* context) {
  return FromDownloadManager(context->GetDownloadManager());
}

bool DownloadPrefs::IsFromTrustedSource(const download::DownloadItem& item) {
  if (!trusted_sources_manager_)
    trusted_sources_manager_ = TrustedSourcesManager::Create();
  return trusted_sources_manager_->IsFromTrustedSource(item.GetURL());
}

base::FilePath DownloadPrefs::DownloadPath() const {
  return SanitizeDownloadTargetPath(*download_path_);
}

void DownloadPrefs::SetDownloadPath(const base::FilePath& path) {
  download_path_.SetValue(path);
  SetSaveFilePath(path);
}

base::FilePath DownloadPrefs::SaveFilePath() const {
  return SanitizeDownloadTargetPath(*save_file_path_);
}

void DownloadPrefs::SetSaveFilePath(const base::FilePath& path) {
  save_file_path_.SetValue(path);
}

void DownloadPrefs::SetSaveFileType(int type) {
  save_file_type_.SetValue(type);
}

bool DownloadPrefs::PromptForDownload() const {
  // If the DownloadDirectory policy is set, then |prompt_for_download_| should
  // always be false.
  DCHECK(!download_path_.IsManaged() || !prompt_for_download_.GetValue());

// Return the Android prompt for download only.
#if BUILDFLAG(IS_ANDROID)
  // Use |prompt_for_download_| preference for enterprise policy.
  if (prompt_for_download_.IsManaged())
    return prompt_for_download_.GetValue();

  // As long as they haven't indicated in preferences they do not want the
  // dialog shown, show the dialog.
  return *prompt_for_download_android_ !=
         static_cast<int>(DownloadPromptStatus::DONT_SHOW);
#else
  return *prompt_for_download_;
#endif
}

bool DownloadPrefs::IsDownloadPathManaged() const {
  return download_path_.IsManaged();
}

bool DownloadPrefs::IsAutoOpenByUserUsed() const {
  return CanPlatformEnableAutoOpenForPdf() || !auto_open_by_user_.empty();
}

bool DownloadPrefs::IsAutoOpenEnabled(const GURL& url,
                                      const base::FilePath& path) const {
  base::FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  if (base::FilePath::CompareEqualIgnoreCase(extension,
                                             FILE_PATH_LITERAL("pdf")) &&
      CanPlatformEnableAutoOpenForPdf())
    return true;

  return auto_open_by_user_.find(extension) != auto_open_by_user_.end() ||
         IsAutoOpenByPolicy(url, path);
}

bool DownloadPrefs::IsAutoOpenByPolicy(const GURL& url,
                                       const base::FilePath& path) const {
  base::FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);

  // if |url| is a blob scheme, use the originating URL for policy evaluation.
  const GURL fixed_url =
      url.SchemeIsBlob() ? url::Origin::Create(url).GetURL() : url;

  return auto_open_by_policy_.find(extension) != auto_open_by_policy_.end() &&
         !auto_open_allowed_by_urls_->IsURLBlocked(fixed_url);
}

bool DownloadPrefs::EnableAutoOpenByUserBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (!FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
          file_name)) {
    return false;
  }

  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);

  auto_open_by_user_.insert(extension);
  SaveAutoOpenState();
  return true;
}

void DownloadPrefs::DisableAutoOpenByUserBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (extension.empty())
    return;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  auto_open_by_user_.erase(extension);
  SaveAutoOpenState();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
void DownloadPrefs::SetShouldOpenPdfInSystemReader(bool should_open) {
  if (should_open_pdf_in_system_reader_ == should_open)
    return;
  should_open_pdf_in_system_reader_ = should_open;
  profile_->GetPrefs()->SetBoolean(prefs::kOpenPdfDownloadInSystemReader,
                                   should_open);
}

bool DownloadPrefs::ShouldOpenPdfInSystemReader() const {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, there is always an "app" to handle PDF files. E.g., a "View"
  // app which configures a file handler to open in a browser tab. However,
  // there is no browser UI to manipulate the kOpenPdfDownloadInSystemReader
  // download pref. Instead, user preference is managed via the Files app "Open
  // with..." UI. Return true here to respect the user's "Open with" preference,
  // and retain consistency with other shelf UI for recent downloads (Tote).
  return true;
#else
  return should_open_pdf_in_system_reader_;
#endif
}
#endif

void DownloadPrefs::ResetAutoOpenByUser() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  SetShouldOpenPdfInSystemReader(false);
#endif
  auto_open_by_user_.clear();
  SaveAutoOpenState();
}

void DownloadPrefs::SkipSanitizeDownloadTargetPathForTesting() {
  skip_sanitize_download_target_path_for_testing_ = true;
}

#if BUILDFLAG(IS_ANDROID)
bool DownloadPrefs::IsAutoOpenPdfEnabled() {
  return *auto_open_pdf_enabled_;
}
#endif

void DownloadPrefs::SaveAutoOpenState() {
  std::string extensions;
  for (auto it : auto_open_by_user_) {
#if BUILDFLAG(IS_WIN)
    // TODO(phajdan.jr): Why we're using Sys conversion here, but not in ctor?
    std::string this_extension = base::SysWideToUTF8(it);
#else  // BUILDFLAG(IS_WIN)
    std::string this_extension = it;
#endif
    extensions += this_extension + ":";
  }
  if (!extensions.empty())
    extensions.erase(extensions.size() - 1);

  profile_->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, extensions);
}

bool DownloadPrefs::CanPlatformEnableAutoOpenForPdf() const {
#if BUILDFLAG(IS_CHROMEOS)
  return false;  // There is no UI for auto-open on ChromeOS.
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  return ShouldOpenPdfInSystemReader();
#else
  return false;
#endif
}

base::FilePath DownloadPrefs::SanitizeDownloadTargetPath(
    const base::FilePath& path) const {
  if (skip_sanitize_download_target_path_for_testing_)
    return path;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40731523): Sort out path sanitization for Lacros.
  // This will require refactoring the ash-only code below so it can be shared.
  base::FilePath migrated_drive_path;
  if (download_dir_util::ExpandDrivePolicyVariable(profile_, path,
                                                   &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
  }

  base::FilePath onedrive_path;
  if (download_dir_util::ExpandOneDrivePolicyVariable(profile_, path,
                                                      &onedrive_path)) {
    return SanitizeDownloadTargetPath(onedrive_path);
  }

  const base::FilePath default_downloads_path =
      GetDefaultDownloadDirectoryForProfile();
  // Relative paths might be unsafe, so use the default path.
  if (!path.IsAbsolute() || path.ReferencesParent())
    return default_downloads_path;

  // Allow downloads directory and subdirectories. Subdirectories may not seem
  // useful, but many tests assume they can download files into a subdirectory,
  // and allowing subdirectories doesn't hurt.
  if (default_downloads_path == path || default_downloads_path.IsParent(path))
    return path;

  // Allow documents directory ("MyFiles") and subdirectories.
  base::FilePath documents_path =
      base::PathService::CheckedGet(chrome::DIR_USER_DOCUMENTS);
  if (documents_path == path || documents_path.IsParent(path))
    return path;

  // Allow paths under the drive mount point.
  base::FilePath drivefs;
  bool drivefs_mounted = chrome::GetDriveFsMountPointPath(&drivefs);
  if (drivefs_mounted && drivefs.IsParent(path))
    return path;

  // Allow paths under OneDrive mount point if the feature flag is enabled.
  base::FilePath odfs_path;
  bool odfs_mounted = chrome::GetOneDriveMountPointPath(&odfs_path);
  if (base::FeatureList::IsEnabled(features::kSkyVault) && odfs_mounted &&
      ((odfs_path == path) || odfs_path.IsParent(path))) {
    return path;
  }

  // Allow paths for removable media devices.
  base::FilePath removable_media_path;
  if (chrome::GetRemovableMediaPath(&removable_media_path) &&
      removable_media_path.IsParent(path)) {
    return path;
  }

  // Allow paths under the Android files mount point.
  base::FilePath android_files_path;
  if (chrome::GetAndroidFilesPath(&android_files_path) &&
      android_files_path.IsParent(path)) {
    return path;
  }

  // Allow Linux files mount point and subdirs.
  base::FilePath linux_files_path;
  if (chrome::GetLinuxFilesPath(&linux_files_path) &&
      (linux_files_path == path || linux_files_path.IsParent(path))) {
    return path;
  }

  // Otherwise, return the safe default.
  return default_downloads_path;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath migrated_drive_path;
  // Managed prefs may force a legacy Drive path as the download path. Ensure
  // the path is valid when DriveFS is enabled.
  if (!path.empty() && file_manager::util::MigratePathFromOldFormat(
                           profile_, GetDefaultDownloadDirectory(), path,
                           &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
  }
  if (file_manager::util::MigrateToDriveFs(profile_, path,
                                           &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
  }
  if (download_dir_util::ExpandDrivePolicyVariable(profile_, path,
                                                   &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
  }

  base::FilePath onedrive_path;
  if (download_dir_util::ExpandOneDrivePolicyVariable(profile_, path,
                                                      &onedrive_path)) {
    return SanitizeDownloadTargetPath(onedrive_path);
  }

  // If |path| isn't absolute, fall back to the default directory.
  base::FilePath profile_myfiles_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  if (!path.IsAbsolute() || path.ReferencesParent())
    return profile_myfiles_path;

  // Allow myfiles directory and subdirs.
  if (profile_myfiles_path == path || profile_myfiles_path.IsParent(path))
    return path;

  // Allow paths under the drive mount point.
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (integration_service && integration_service->is_enabled() &&
      integration_service->GetMountPointPath().IsParent(path)) {
    return path;
  }

  // Allow paths under /tmp if the feature flag is enabled.
  base::FilePath temp_path;
  if (base::FeatureList::IsEnabled(features::kSkyVault) &&
      base::GetTempDir(&temp_path) &&
      ((temp_path == path) || temp_path.IsParent(path))) {
    return path;
  }

  // Allow removable media.
  if (ash::CrosDisksClient::GetRemovableDiskMountPoint().IsParent(path))
    return path;

  // Allow paths under the Android files mount point.
  if (base::FilePath(file_manager::util::GetAndroidFilesPath()).IsParent(path))
    return path;

  // Allow Linux files mount point and subdirs.
  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile_);
  if (linux_files == path || linux_files.IsParent(path))
    return path;

  // Fall back to the default download directory for all other paths.
  return GetDefaultDownloadDirectoryForProfile();
#else
  // If the stored download directory is an absolute path, we presume it's
  // correct; there's not really much more validation we can do here.
  if (path.IsAbsolute())
    return path;

  // When the default download directory is *not* an absolute path, we use the
  // profile directory as a safe default.
  return GetDefaultDownloadDirectoryForProfile();
#endif
}

void DownloadPrefs::UpdateAutoOpenByPolicy() {
  auto_open_by_policy_.clear();

  PrefService* prefs = profile_->GetPrefs();
  for (const auto& extension :
       prefs->GetList(prefs::kDownloadExtensionsToOpenByPolicy)) {
    base::FilePath::StringType extension_string =
        StringToFilePathString(extension.GetString());
    auto_open_by_policy_.insert(extension_string);
  }
}

void DownloadPrefs::UpdateAllowedURLsForOpenByPolicy() {
  std::unique_ptr<policy::URLBlocklist> allowed_urls =
      std::make_unique<policy::URLBlocklist>();

  PrefService* prefs = profile_->GetPrefs();
  const auto& list = prefs->GetList(prefs::kDownloadAllowedURLsForOpenByPolicy);

  // We only need to configure |allowed_urls| if something is set by policy,
  // otherwise the default object does what we want.
  if (list.size() != 0) {
    allowed_urls->Allow(list);

    // Since we only want to auto-open for the specified urls, block everything
    // else.
    auto blocked = base::Value::List();
    blocked.Append("*");
    allowed_urls->Block(blocked);
  }

  auto_open_allowed_by_urls_.swap(allowed_urls);
}

bool DownloadPrefs::AutoOpenCompareFunctor::operator()(
    const base::FilePath::StringType& a,
    const base::FilePath::StringType& b) const {
  return base::FilePath::CompareLessIgnoreCase(a, b);
}
