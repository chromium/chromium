// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/trusted_sources_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/download/public/common/download_item.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/save_page_type.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chromeos/dbus/cros_disks_client.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using safe_browsing::FileTypePolicies;

namespace {

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
bool DownloadPathIsDangerous(const base::FilePath& download_path) {
#if defined(OS_LINUX)
  base::FilePath home_dir = base::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

#if defined(OS_ANDROID)
  // Android does not have a desktop dir.
  return false;
#else
  base::FilePath desktop_dir;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
#endif
}

class DefaultDownloadDirectory {
 public:
  const base::FilePath& path() const { return path_; }

  void Initialize() {
    if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path_)) {
      NOTREACHED();
    }
    if (DownloadPathIsDangerous(path_)) {
      // This is only useful on platforms that support
      // DIR_DEFAULT_DOWNLOADS_SAFE.
      if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path_)) {
        NOTREACHED();
      }
    }
  }

 private:
  friend class base::NoDestructor<DefaultDownloadDirectory>;

  DefaultDownloadDirectory() { Initialize(); }

  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDownloadDirectory);
};

DefaultDownloadDirectory& GetDefaultDownloadDirectorySingleton() {
  static base::NoDestructor<DefaultDownloadDirectory> instance;
  return *instance;
}

}  // namespace

DownloadPrefs::DownloadPrefs(Profile* profile) : profile_(profile) {
  PrefService* prefs = profile->GetPrefs();

#if defined(OS_CHROMEOS)
  // On Chrome OS, the default download directory is different for each profile.
  // If the profile-unaware default path (from GetDefaultDownloadDirectory())
  // is set (this happens during the initial preference registration in static
  // RegisterProfilePrefs()), alter by GetDefaultDownloadDirectoryForProfile().
  // file_manager::util::MigratePathFromOldFormat will do this.
  const char* path_pref[] = {
      prefs::kSaveFileDefaultDirectory,
      prefs::kDownloadDefaultDirectory
  };
  for (size_t i = 0; i < base::size(path_pref); ++i) {
    const base::FilePath current = prefs->GetFilePath(path_pref[i]);
    base::FilePath migrated;
    if (!current.empty() &&
        file_manager::util::MigratePathFromOldFormat(
            profile_, GetDefaultDownloadDirectory(), current, &migrated)) {
      prefs->SetFilePath(path_pref[i], migrated);

      // In M73 migrate /home/chronos/u-<hash>/Downloads to
      // /home/chronos/u-<hash>/MyFiles/Downloads.  This code can be removed
      // when M72 and earlier is no longer supported.
    } else if (file_manager::util::MigrateFromDownloadsToMyFiles(
                   profile_, current, &migrated)) {
      prefs->SetFilePath(path_pref[i], migrated);
    } else if (file_manager::util::MigrateToDriveFs(profile_, current,
                                                    &migrated)) {
      prefs->SetFilePath(path_pref[i], migrated);
    } else if (download_dir_util::ExpandDrivePolicyVariable(profile_, current,
                                                            &migrated)) {
      prefs->SetFilePath(path_pref[i], migrated);
    }
  }

  // Ensure that the default download directory exists.
  content::DownloadManager::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                                GetDefaultDownloadDirectoryForProfile()));
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  should_open_pdf_in_system_reader_ =
      prefs->GetBoolean(prefs::kOpenPdfDownloadInSystemReader);
#endif

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

  prompt_for_download_.Init(prefs::kPromptForDownload, prefs);
#if defined(OS_ANDROID)
  prompt_for_download_android_.Init(prefs::kPromptForDownloadAndroid, prefs);

  // If |kDownloadsLocationChange| is not enabled, always uses the default
  // download location, in case that the feature is enabled and then disabled
  // from finch config and the user may stuck at other download locations.
  if (!base::FeatureList::IsEnabled(features::kDownloadsLocationChange)) {
    prefs->SetFilePath(prefs::kDownloadDefaultDirectory,
                       GetDefaultDownloadDirectoryForProfile());
    prefs->SetFilePath(prefs::kSaveFileDefaultDirectory,
                       GetDefaultDownloadDirectoryForProfile());
  }
#endif
  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs);
  save_file_path_.Init(prefs::kSaveFileDefaultDirectory, prefs);
  save_file_type_.Init(prefs::kSaveFileType, prefs);
  safebrowsing_for_trusted_sources_enabled_.Init(
      prefs::kSafeBrowsingForTrustedSourcesEnabled, prefs);
  download_restriction_.Init(prefs::kDownloadRestrictions, prefs);

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  std::string extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);

  for (const auto& extension_string : base::SplitString(
           extensions_to_open, ":",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
#if defined(OS_POSIX)
    base::FilePath::StringType extension = extension_string;
#elif defined(OS_WIN)
    base::FilePath::StringType extension = base::UTF8ToWide(extension_string);
#endif
    // If it's empty or malformed or not allowed to open automatically, then
    // skip the entry. Any such entries will be dropped from preferences the
    // next time SaveAutoOpenState() is called.
    if (extension.empty() ||
        *extension.begin() == base::FilePath::kExtensionSeparator)
      continue;
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
      auto_open_.insert(extension);
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
  registry->RegisterBooleanPref(prefs::kDownloadDirUpgraded, false);
  registry->RegisterIntegerPref(prefs::kSaveFileType,
                                content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
  registry->RegisterIntegerPref(prefs::kDownloadRestrictions, 0);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingForTrustedSourcesEnabled,
                                true);

  const base::FilePath& default_download_path = GetDefaultDownloadDirectory();
  registry->RegisterFilePathPref(prefs::kDownloadDefaultDirectory,
                                 default_download_path);
  registry->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                 default_download_path);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  registry->RegisterBooleanPref(prefs::kOpenPdfDownloadInSystemReader, false);
#endif
#if defined(OS_ANDROID)
  DownloadPromptStatus download_prompt_status =
      base::FeatureList::IsEnabled(features::kDownloadsLocationChange)
          ? DownloadPromptStatus::SHOW_INITIAL
          : DownloadPromptStatus::DONT_SHOW;
  registry->RegisterIntegerPref(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(download_prompt_status),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowMissingSdCardErrorAndroid,
      base::FeatureList::IsEnabled(features::kDownloadsLocationChange));
#endif
}

base::FilePath DownloadPrefs::GetDefaultDownloadDirectoryForProfile() const {
#if defined(OS_CHROMEOS)
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
  return FromDownloadManager(BrowserContext::GetDownloadManager(context));
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
#if defined(OS_ANDROID)
  // As long as they haven't indicated in preferences they do not want the
  // dialog shown, show the dialog.
  return *prompt_for_download_android_ !=
         static_cast<int>(DownloadPromptStatus::DONT_SHOW);
#endif

  return *prompt_for_download_;
}

bool DownloadPrefs::IsDownloadPathManaged() const {
  return download_path_.IsManaged();
}

bool DownloadPrefs::IsAutoOpenUsed() const {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  if (ShouldOpenPdfInSystemReader())
    return true;
#endif
  return !auto_open_.empty();
}

bool DownloadPrefs::IsAutoOpenEnabledBasedOnExtension(
    const base::FilePath& path) const {
  base::FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  if (base::FilePath::CompareEqualIgnoreCase(extension,
                                             FILE_PATH_LITERAL("pdf")) &&
      ShouldOpenPdfInSystemReader())
    return true;
#endif

  return auto_open_.find(extension) != auto_open_.end();
}

bool DownloadPrefs::EnableAutoOpenBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (!FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
          file_name)) {
    return false;
  }

  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);

  auto_open_.insert(extension);
  SaveAutoOpenState();
  return true;
}

void DownloadPrefs::DisableAutoOpenBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (extension.empty())
    return;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  auto_open_.erase(extension);
  SaveAutoOpenState();
}

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
void DownloadPrefs::SetShouldOpenPdfInSystemReader(bool should_open) {
  if (should_open_pdf_in_system_reader_ == should_open)
    return;
  should_open_pdf_in_system_reader_ = should_open;
  profile_->GetPrefs()->SetBoolean(prefs::kOpenPdfDownloadInSystemReader,
                                   should_open);
}

bool DownloadPrefs::ShouldOpenPdfInSystemReader() const {
#if defined(OS_WIN)
  if (IsAdobeReaderDefaultPDFViewer() &&
      !DownloadTargetDeterminer::IsAdobeReaderUpToDate()) {
      return false;
  }
#endif
  return should_open_pdf_in_system_reader_;
}
#endif

void DownloadPrefs::ResetAutoOpen() {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  SetShouldOpenPdfInSystemReader(false);
#endif
  auto_open_.clear();
  SaveAutoOpenState();
}

void DownloadPrefs::SkipSanitizeDownloadTargetPathForTesting() {
  skip_sanitize_download_target_path_for_testing_ = true;
}

void DownloadPrefs::SaveAutoOpenState() {
  std::string extensions;
  for (auto it = auto_open_.begin(); it != auto_open_.end(); ++it) {
#if defined(OS_POSIX)
    std::string this_extension = *it;
#elif defined(OS_WIN)
    // TODO(phajdan.jr): Why we're using Sys conversion here, but not in ctor?
    std::string this_extension = base::SysWideToUTF8(*it);
#endif
    extensions += this_extension + ":";
  }
  if (!extensions.empty())
    extensions.erase(extensions.size() - 1);

  profile_->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, extensions);
}

base::FilePath DownloadPrefs::SanitizeDownloadTargetPath(
    const base::FilePath& path) const {
#if defined(OS_CHROMEOS)
  if (skip_sanitize_download_target_path_for_testing_)
    return path;

  base::FilePath migrated_drive_path;
  // Managed prefs may force a legacy Drive path as the download path. Ensure
  // the path is valid when DriveFS is enabled.
  if (file_manager::util::MigrateToDriveFs(profile_, path,
                                           &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
  }
  if (download_dir_util::ExpandDrivePolicyVariable(profile_, path,
                                                   &migrated_drive_path)) {
    return SanitizeDownloadTargetPath(migrated_drive_path);
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

  // Allow removable media.
  if (chromeos::CrosDisksClient::GetRemovableDiskMountPoint().IsParent(path))
    return path;

  // Allow paths under the Android files mount point.
  if (base::FilePath(file_manager::util::kAndroidFilesPath).IsParent(path))
    return path;

  // Allow Linux files mount point and subdirs.
  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile_);
  if (linux_files == path || linux_files.IsParent(path))
    return path;

  // Fall back to the default download directory for all other paths.
  return GetDefaultDownloadDirectoryForProfile();
#endif
  // If the stored download directory is an absolute path, we presume it's
  // correct; there's not really much more validation we can do here.
  if (path.IsAbsolute())
    return path;

  // When the default download directory is *not* an absolute path, we use the
  // profile directory as a safe default.
  return GetDefaultDownloadDirectoryForProfile();
}

bool DownloadPrefs::AutoOpenCompareFunctor::operator()(
    const base::FilePath::StringType& a,
    const base::FilePath::StringType& b) const {
  return base::FilePath::CompareLessIgnoreCase(a, b);
}
