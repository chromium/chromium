// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/files_internals_ui_delegate.h"

#include "base/barrier_callback.h"
#include "base/files/file_enumerator.h"
#include "base/strings/escape.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/external_mount_points.h"

ChromeFilesInternalsUIDelegate::ChromeFilesInternalsUIDelegate(
    content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeFilesInternalsUIDelegate::~ChromeFilesInternalsUIDelegate() = default;

void ChromeFilesInternalsUIDelegate::GetDebugJSON(
    base::OnceCallback<void(const base::Value&)> callback) const {
  using JSONKeyValuePair =
      ash::FilesInternalsDebugJSONProvider::JSONKeyValuePair;

  struct NamedProvider {
    std::string_view key;
    ash::FilesInternalsDebugJSONProvider::FunctionPointerType function_ptr;
    raw_ptr<ash::FilesInternalsDebugJSONProvider> object_ptr;
  };

  const NamedProvider kNamedProviders[] = {
      {
          "execute_file_task",
          &file_manager::file_tasks::GetDebugJSONForKeyForExecuteFileTask,
          nullptr,
      },
      {
          "external_mount_points",
          &storage::ExternalMountPoints::GetDebugJSONForKey,
          nullptr,
      },
      {
          "fusebox",
          nullptr,
          fusebox::Server::GetInstance(),
      },
  };

  base::RepeatingCallback<void(JSONKeyValuePair)> barrier_callback =
      base::BarrierCallback<JSONKeyValuePair>(
          std::size(kNamedProviders),
          base::BindOnce(
              [](base::OnceCallback<void(const base::Value&)> callback,
                 std::vector<JSONKeyValuePair> key_value_pairs) {
                base::Value::Dict dict;
                for (auto& kvp : key_value_pairs) {
                  dict.Set(kvp.first, std::move(kvp.second));
                }
                std::move(callback).Run(base::Value(std::move(dict)));
              },
              std::move(callback)));

  for (auto& np : kNamedProviders) {
    if (np.function_ptr) {
      (*np.function_ptr)(np.key, barrier_callback);
    } else if (np.object_ptr) {
      np.object_ptr->GetDebugJSONForKey(np.key, barrier_callback);
    } else {
      barrier_callback.Run(std::make_pair(np.key, base::Value()));
    }
  }
}

void ChromeFilesInternalsUIDelegate::GetDownloadsFSURLs(
    base::OnceCallback<void(const std::string_view)> callback) const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  base::FilePath downloads_folder =
      file_manager::util::GetDownloadsFolderForProfile(profile);

  static constexpr auto enumerate_on_worker_thread =
      [](base::FilePath downloads_folder) -> std::vector<base::FilePath> {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    std::vector<base::FilePath> files;
    base::FileEnumerator e(downloads_folder, /*recursive=*/false,
                           base::FileEnumerator::FILES);
    for (base::FilePath path = e.Next(); !path.empty(); path = e.Next()) {
      files.push_back(path);
    }

    std::sort(files.begin(), files.end(),
              [](const base::FilePath& l, const base::FilePath& r) {
                return l.BaseName() < r.BaseName();
              });

    return files;
  };

  static constexpr auto reply_on_ui_thread =
      [](Profile* profile,
         base::OnceCallback<void(const std::string_view)> callback,
         std::vector<base::FilePath> files) {
        std::string out;
        out.reserve(4096 * (1 + files.size()));
        out +=
            "<html><body><p>Files (not directories; non-recursive) in the "
            "Downloads folder:</p><ul>\n\n";

        GURL gurl;
        for (const auto& file : files) {
          if (file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
                  profile, file, file_manager::util::GetFileManagerURL(),
                  &gurl)) {
            out += "<li>";
            out += base::EscapeForHTML(gurl.spec());
            out += "\n";
          }
        }

        out += "</ul></body></html>\n";
        std::move(callback).Run(out);
      };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(enumerate_on_worker_thread, downloads_folder),
      base::BindOnce(reply_on_ui_thread, profile, std::move(callback)));
}

void ChromeFilesInternalsUIDelegate::GetFileTasks(
    const std::string& file_system_url,
    base::OnceCallback<void(const std::string_view)> callback) const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  storage::FileSystemURL fs_url =
      file_manager::util::GetFileManagerFileSystemContext(profile)
          ->CrackURLInFirstPartyContext(GURL(file_system_url));
  if (!fs_url.is_valid()) {
    std::move(callback).Run("GetFileTasks: invalid FileSystemURL");
    return;
  }

  static constexpr auto on_find_all_types_of_tasks =
      [](storage::FileSystemURL fs_url,
         base::OnceCallback<void(const std::string_view)> callback,
         std::unique_ptr<file_manager::file_tasks::ResultingTasks>
             resulting_tasks) {
        std::string out;
        out.reserve(4096 * (1 + resulting_tasks->tasks.size()));
        out += "<html><body>\n";
        out +=
            "<p>See also <a href=\"chrome://app-service-internals\">App "
            "Service Internals</a> and <a "
            "href=\"chrome://extensions-internals/\">Extensions "
            "Internals</a>.</p>\n<p>FileSystemURL: <b>";
        out += base::EscapeForHTML(fs_url.ToGURL().spec());
        out +=
            "</b></p>\n<p>Note that some Task icons and titles are separately "
            "<a "
            "href=\"https://source.chromium.org/chromium/chromium/src/+/"
            "main:ui/file_manager/file_manager/foreground/js/"
            "file_tasks.ts;l=913;drc="
            "9c9022199a6b2e7411a3cafdc347efeb6f229785\">overriden in the Files "
            "app frontend</a>.</p>\n";

        for (const auto& task : resulting_tasks->tasks) {
          out += "<hr><table><tr><td><img width=64px height=64px src=\"";
          out += base::EscapeForHTML(task.icon_url.spec());
          out += "\"></td><td>";
          out += "<ul><li>TaskTitle: <b>";
          out += base::EscapeForHTML(task.task_title);
          out += "</b>";
          if (task.is_default) {
            out += " (default)";
          }
          out += "<li>ActionID: ";
          out += base::EscapeForHTML(task.task_descriptor.action_id);
          out += "<li>AppID: ";
          out += base::EscapeForHTML(task.task_descriptor.app_id);
          out += "<li>TaskType: ";
          out += TaskTypeToString(task.task_descriptor.task_type);
          out += "</ul></td></tr></table>\n";
        }

        out += "</body></html>\n";
        std::move(callback).Run(out);
      };

  static constexpr auto on_get_is_directory =
      [](Profile* profile, storage::FileSystemURL fs_url,
         base::FilePath local_path, std::string mime_type,
         base::OnceCallback<void(const std::string_view)> callback,
         bool is_directory) {
        std::vector<extensions::EntryInfo> entries;
        entries.emplace_back(local_path, mime_type, is_directory);

        std::vector<GURL> file_urls;
        file_urls.push_back(fs_url.ToGURL());

        std::vector<std::string> dlp_source_urls;
        dlp_source_urls.emplace_back("");

        file_manager::file_tasks::FindAllTypesOfTasks(
            profile, entries, file_urls, dlp_source_urls,
            base::BindOnce(on_find_all_types_of_tasks, fs_url,
                           std::move(callback)));
      };

  static constexpr auto on_get_mime_type =
      [](Profile* profile, storage::FileSystemURL fs_url,
         base::FilePath local_path,
         base::OnceCallback<void(const std::string_view)> callback,
         const std::string& mime_type) {
        extensions::app_file_handler_util::GetIsDirectoryForLocalPath(
            profile, local_path,
            base::BindOnce(on_get_is_directory, profile, fs_url, local_path,
                           mime_type, std::move(callback)));
      };

  base::FilePath local_path = fs_url.path();
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile, local_path,
      base::BindOnce(on_get_mime_type, profile, fs_url, local_path,
                     std::move(callback)));
}

bool ChromeFilesInternalsUIDelegate::GetSmbfsEnableVerboseLogging() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && profile->GetPrefs()->GetBoolean(
                        file_manager::prefs::kSmbfsEnableVerboseLogging);
}

void ChromeFilesInternalsUIDelegate::SetSmbfsEnableVerboseLogging(
    bool enabled) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    profile->GetPrefs()->SetBoolean(
        file_manager::prefs::kSmbfsEnableVerboseLogging, enabled);
  }
}

std::string ChromeFilesInternalsUIDelegate::GetOfficeFileHandlers() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  const base::Value::Dict& extension_task_prefs =
      profile->GetPrefs()->GetDict(prefs::kDefaultTasksBySuffix);
  base::Value::Dict filtered_prefs;

  for (const std::string& extension :
       file_manager::file_tasks::WordGroupExtensions()) {
    if (extension_task_prefs.contains(extension)) {
      filtered_prefs.Set(extension,
                         *extension_task_prefs.FindString(extension));
    }
  }
  for (const std::string& extension :
       file_manager::file_tasks::ExcelGroupExtensions()) {
    if (extension_task_prefs.contains(extension)) {
      filtered_prefs.Set(extension,
                         *extension_task_prefs.FindString(extension));
    }
  }
  for (const std::string& extension :
       file_manager::file_tasks::PowerPointGroupExtensions()) {
    if (extension_task_prefs.contains(extension)) {
      filtered_prefs.Set(extension,
                         *extension_task_prefs.FindString(extension));
    }
  }
  return filtered_prefs.DebugString();
}

void ChromeFilesInternalsUIDelegate::ClearOfficeFileHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (!profile) {
    return;
  }
  // Do not allow cleaning office handlers when automated Clippy flows are in
  // place.
  if (chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile) ||
      chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAutomated(profile)) {
    return;
  }
  ScopedDictPrefUpdate mime_type_pref(profile->GetPrefs(),
                                      prefs::kDefaultTasksByMimeType);
  for (const std::string& mime_type :
       file_manager::file_tasks::WordGroupMimeTypes()) {
    mime_type_pref->Remove(mime_type);
  }
  for (const std::string& mime_type :
       file_manager::file_tasks::ExcelGroupMimeTypes()) {
    mime_type_pref->Remove(mime_type);
  }
  for (const std::string& mime_type :
       file_manager::file_tasks::PowerPointGroupMimeTypes()) {
    mime_type_pref->Remove(mime_type);
  }

  ScopedDictPrefUpdate extension_pref(profile->GetPrefs(),
                                      prefs::kDefaultTasksBySuffix);
  for (const std::string& extension :
       file_manager::file_tasks::WordGroupExtensions()) {
    extension_pref->Remove(extension);
  }
  for (const std::string& extension :
       file_manager::file_tasks::ExcelGroupExtensions()) {
    extension_pref->Remove(extension);
  }
  for (const std::string& extension :
       file_manager::file_tasks::PowerPointGroupExtensions()) {
    extension_pref->Remove(extension);
  }

  // Also update the preferences to signal that the move confirmation dialog
  // has never been shown.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForDrive(profile,
                                                                   false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForOneDrive(profile,
                                                                      false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToDrive(
      profile, false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToOneDrive(
      profile, false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(
      profile, false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(
      profile, false);
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
             profile);
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForOneDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
             profile);
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForLocalToDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && file_manager::file_tasks::
                        GetOfficeMoveConfirmationShownForLocalToDrive(profile);
}

bool ChromeFilesInternalsUIDelegate::
    GetMoveConfirmationShownForLocalToOneDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::
             GetOfficeMoveConfirmationShownForLocalToOneDrive(profile);
}

bool ChromeFilesInternalsUIDelegate::GetMoveConfirmationShownForCloudToDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile && file_manager::file_tasks::
                        GetOfficeMoveConfirmationShownForCloudToDrive(profile);
}

bool ChromeFilesInternalsUIDelegate::
    GetMoveConfirmationShownForCloudToOneDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::
             GetOfficeMoveConfirmationShownForCloudToOneDrive(profile);
}

bool ChromeFilesInternalsUIDelegate::GetAlwaysMoveOfficeFilesToDrive() const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetAlwaysMoveOfficeFilesToDrive(profile);
}

void ChromeFilesInternalsUIDelegate::SetAlwaysMoveOfficeFilesToDrive(
    bool always_move) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile,
                                                              always_move);
    // Also clear up the timestamp for when files are moved to the Cloud.
    file_manager::file_tasks::SetOfficeFileMovedToGoogleDrive(profile,
                                                              base::Time());
    // Spawn the Files app Window so it clears up its localStorage.
    auto url = ::file_manager::util::GetFileManagerURL().Resolve("");
    ::ash::SystemAppLaunchParams params;
    params.url = url;
    ::ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                                   params);
  }
}

bool ChromeFilesInternalsUIDelegate::GetAlwaysMoveOfficeFilesToOneDrive()
    const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  return profile &&
         file_manager::file_tasks::GetAlwaysMoveOfficeFilesToOneDrive(profile);
}

void ChromeFilesInternalsUIDelegate::SetAlwaysMoveOfficeFilesToOneDrive(
    bool always_move) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  if (profile) {
    file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile,
                                                                 always_move);
    // Also clear up the timestamp for when files are moved to the Cloud.
    file_manager::file_tasks::SetOfficeFileMovedToOneDrive(profile,
                                                           base::Time());
    // Spawn the Files app Window so it clears up its localStorage.
    auto url = ::file_manager::util::GetFileManagerURL().Resolve("");
    ::ash::SystemAppLaunchParams params;
    params.url = url;
    ::ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                                   params);
  }
}
