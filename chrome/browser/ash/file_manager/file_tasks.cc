// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/arc_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_browser_handlers.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/guest_os_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/drive/drive_api_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "net/base/mime_util.h"
#include "pdf/buildflags.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

using ash::file_manager::kChromeUIFileManagerURL;
using extensions::Extension;
using extensions::api::file_manager_private::Verb;
using storage::FileSystemURL;

namespace file_manager {
namespace file_tasks {

const char kActionIdView[] = "view";
const char kActionIdSend[] = "send";
const char kActionIdSendMultiple[] = "send_multiple";
const char kActionIdWebDriveOfficeWord[] = "open-web-drive-office-word";
const char kActionIdWebDriveOfficeExcel[] = "open-web-drive-office-excel";
const char kActionIdWebDriveOfficePowerPoint[] =
    "open-web-drive-office-powerpoint";

namespace {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
const char kFileBrowserHandlerTaskType[] = "file";
const char kFileHandlerTaskType[] = "app";
const char kArcAppTaskType[] = "arc";
const char kCrostiniAppTaskType[] = "crostini";
const char kPluginVmAppTaskType[] = "pluginvm";
const char kWebAppTaskType[] = "web";

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kPdfFileExtension[] = ".pdf";

void RecordChangesInDefaultPdfApp(const std::string& new_default_app_id,
                                  const std::set<std::string>& mime_types,
                                  const std::set<std::string>& suffixes) {
  bool hasPdfMimeType = base::Contains(mime_types, kPdfMimeType);
  bool hasPdfSuffix = base::Contains(suffixes, kPdfFileExtension);
  if (!hasPdfMimeType || !hasPdfSuffix) {
    return;
  }

  if (new_default_app_id == web_app::kMediaAppId) {
    base::RecordAction(
        base::UserMetricsAction("MediaApp.PDF.DefaultApp.SwitchedTo"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MediaApp.PDF.DefaultApp.SwitchedAway"));
  }
}

// Returns True if the `app_id` belongs to Files app either extension or SWA.
inline bool isFilesAppId(const std::string& app_id) {
  return app_id == kFileManagerAppId || app_id == kFileManagerSwaAppId;
}

// The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID, just the
// sub-string compatible with the extension/legacy e.g.: "view-pdf".
std::string parseFilesAppActionId(const std::string& action_id) {
  if (base::StartsWith(action_id, kChromeUIFileManagerURL)) {
    std::string result(action_id);
    base::ReplaceFirstSubstringAfterOffset(
        &result, 0, base::StrCat({kChromeUIFileManagerURL, "?"}), "");

    return result;
  }

  return action_id;
}

// Returns true if the `task` is a Web Drive Office task.
bool isWebDriveOfficeTask(const FullTaskDescriptor& task) {
  const std::string action_id =
      parseFilesAppActionId(task.task_descriptor.action_id);
  bool is_web_drive_office_action_id =
      action_id == kActionIdWebDriveOfficeWord ||
      action_id == kActionIdWebDriveOfficeExcel ||
      action_id == kActionIdWebDriveOfficePowerPoint;
  return isFilesAppId(task.task_descriptor.app_id) &&
         is_web_drive_office_action_id;
}

// Returns true if path_mime_set contains a Google document.
bool ContainsGoogleDocument(const std::vector<extensions::EntryInfo>& entries) {
  for (const auto& it : entries) {
    if (drive::util::HasHostedDocumentExtension(it.path))
      return true;
  }
  return false;
}

// Removes all tasks except tasks handled by file manager.
void KeepOnlyFileManagerInternalTasks(std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (FullTaskDescriptor& task : *tasks) {
    if (isFilesAppId(task.task_descriptor.app_id))
      filtered.push_back(task);
  }
  tasks->swap(filtered);
}

// Removes task |actions| handled by file manager.
void RemoveFileManagerInternalActions(const std::set<std::string>& actions,
                                      std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (FullTaskDescriptor& task : *tasks) {
    const auto& action = task.task_descriptor.action_id;
    if (!isFilesAppId(task.task_descriptor.app_id)) {
      filtered.push_back(task);
    } else if (actions.find(parseFilesAppActionId(action)) == actions.end()) {
      filtered.push_back(task);
    }
  }

  tasks->swap(filtered);
}

// Adjusts |tasks| to reflect the product decision that chrome://media-app
// should behave more like a user-installed app than a fallback handler.
// Specifically, only apps set as the default in user prefs should be preferred
// over chrome://media-app.
void AdjustTasksForMediaApp(const std::vector<extensions::EntryInfo>& entries,
                            std::vector<FullTaskDescriptor>* tasks) {
  const auto task_for_app = [&](const std::string& app_id) {
    return std::find_if(tasks->begin(), tasks->end(), [&](const auto& task) {
      return task.task_descriptor.app_id == app_id;
    });
  };

  const auto media_app_task = task_for_app(web_app::kMediaAppId);
  if (media_app_task == tasks->end())
    return;

  // TOOD(crbug/1071289): For a while is_file_extension_match would always be
  // false for System Web App manifests, even when specifying extension matches.
  // So this line can be removed once the media app manifest is updated with a
  // full complement of image file extensions.
  media_app_task->is_file_extension_match = true;

  // The logic in ChooseAndSetDefaultTask() also requires the following to hold.
  // This should only fail if the media app is configured for "*".
  // "image/*" does not count as "generic".
  DCHECK(!media_app_task->is_generic_file_handler);

  // Otherwise, build a new list with Media App at the front.
  if (media_app_task == tasks->begin())
    return;

  std::vector<FullTaskDescriptor> new_tasks;
  new_tasks.push_back(*media_app_task);
  for (auto it = tasks->begin(); it != tasks->end(); ++it) {
    if (it != media_app_task)
      new_tasks.push_back(std::move(*it));
  }
  std::swap(*tasks, new_tasks);
}

// Helper class that validates whether a selected WebDriveOffice task can
// properly handle a given set of files.
class WebDriveOfficeValidationHelper {
 public:
  WebDriveOfficeValidationHelper(
      Profile* profile,
      const std::vector<extensions::EntryInfo>& entries,
      std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
      std::set<std::string> disabled_actions)
      : profile(profile),
        entries(entries),
        result_list(std::move(result_list)),
        disabled_actions_(std::move(disabled_actions)) {}

  WebDriveOfficeValidationHelper(const WebDriveOfficeValidationHelper& other) =
      delete;
  WebDriveOfficeValidationHelper& operator=(
      const WebDriveOfficeValidationHelper& other) = delete;

  ~WebDriveOfficeValidationHelper() = default;

  void Run(base::OnceClosure callback) {
    DCHECK(callback);
    DCHECK(!callback_);

    callback_ = std::move(callback);
    AdjustTasks();
  }

  Profile* profile;
  const std::vector<extensions::EntryInfo> entries;
  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list;

 private:
  // Starts processing entries to determine whether a Web Drive Office action
  // should be disabled or not.
  void AdjustTasks() {
    // No checks to perform if no Web Drive Office task has been selected. It is
    // not possible to have multiple Web Drive Office tasks
    // (Word/Excel/PowerPoint) selected simultaneously.
    const auto web_drive_office_task = std::find_if(
        result_list->begin(), result_list->end(),
        [&](const auto& task) { return isWebDriveOfficeTask(task); });
    if (web_drive_office_task == result_list->end()) {
      EndAdjustTasks();
      return;
    }

    DCHECK(web_drive_office_action_id_.empty());
    web_drive_office_action_id_ =
        parseFilesAppActionId(web_drive_office_task->task_descriptor.action_id);

    // Remove Web Drive Office action if Web Drive Office is disabled.
    if (!base::FeatureList::IsEnabled(ash::features::kFilesWebDriveOffice)) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::FLAG_DISABLED);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    // Remove Web Drive Office action if Drive is Offline.
    if (drive::util::GetDriveConnectionStatus(profile) !=
        drive::util::DRIVE_CONNECTED) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::OFFLINE);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    ProcessNextEntryForWebDriveOffice(0);
  }

  // Checks whether an entry is potentially available to be opened and edited in
  // Web Drive, and query its DriveFS metadata.
  void ProcessNextEntryForWebDriveOffice(size_t entry_index) {
    // Web Drive Office is available for all the selected entries.
    if (entry_index == entries.size()) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::AVAILABLE);
      EndAdjustTasks();
      return;
    }

    // Check whether the entry is on Drive.
    if (!::file_manager::util::IsDriveLocalPath(profile,
                                                entries[entry_index].path)) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::NOT_ON_DRIVE);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    // Check whether the DriveIntegrationService is available.
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile);
    base::FilePath relative_drive_path;
    if (!(integration_service && integration_service->IsMounted() &&
          integration_service->GetDriveFsInterface() &&
          integration_service->GetRelativeDrivePath(entries[entry_index].path,
                                                    &relative_drive_path))) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::DRIVE_ERROR);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    // Get Office file's metadata.
    integration_service->GetDriveFsInterface()->GetMetadata(
        relative_drive_path,
        base::BindOnce(&WebDriveOfficeValidationHelper::
                           OnGetDriveFsMetadataForWebDriveOffice,
                       weak_factory_.GetWeakPtr(), entry_index));
  }

  // Checks whether the Web Drive Office task should be disabled based on the
  // entry's alternate URL.
  void OnGetDriveFsMetadataForWebDriveOffice(
      size_t entry_index,
      drive::FileError error,
      drivefs::mojom::FileMetadataPtr metadata) {
    if (error != drive::FILE_ERROR_OK) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::DRIVE_METADATA_ERROR);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    GURL hosted_url(metadata->alternate_url);
    // URLs for editing Office files in Web Drive all have a "docs.google.com"
    // host: Disable the task if the entry doesn't have such alternate URL.
    if (!hosted_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION(
          kWebDriveOfficeMetricName,
          WebDriveOfficeTaskResult::INVALID_ALTERNATE_URL);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    } else if (hosted_url.host() == "drive.google.com") {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::DRIVE_ALTERNATE_URL);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    } else if (hosted_url.host() != "docs.google.com") {
      UMA_HISTOGRAM_ENUMERATION(
          kWebDriveOfficeMetricName,
          WebDriveOfficeTaskResult::UNEXPECTED_ALTERNATE_URL);
      disabled_actions_.emplace(web_drive_office_action_id_);
      EndAdjustTasks();
      return;
    }

    // Check alternate URL for next entry.
    ProcessNextEntryForWebDriveOffice(++entry_index);
  }

  // Ends the recursion that determines whether or not the Web Drive Office
  // action is available.
  void EndAdjustTasks() {
    if (!disabled_actions_.empty())
      RemoveFileManagerInternalActions(disabled_actions_, result_list.get());
    std::move(callback_).Run();
  }

  std::string web_drive_office_action_id_;
  std::set<std::string> disabled_actions_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<WebDriveOfficeValidationHelper> weak_factory_{this};
};

// Returns true if the given task is a handler by built-in apps like the Files
// app itself or QuickOffice etc. They are used as the initial default app.
bool IsFallbackFileHandler(const FullTaskDescriptor& task) {
  if ((task.task_descriptor.task_type !=
           file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER &&
       task.task_descriptor.task_type != file_tasks::TASK_TYPE_FILE_HANDLER &&
       task.task_descriptor.task_type != file_tasks::TASK_TYPE_WEB_APP) ||
      task.is_generic_file_handler) {
    return false;
  }

  // Note that web_app::kMediaAppId does not appear in the
  // list of built-in apps below. Doing so would mean the presence of any other
  // handler of image files (e.g. Keep, Photos) would take precedence. But we
  // want that only to occur if the user has explicitly set the preference for
  // an app other than kMediaAppId to be the default (b/153387960).
  constexpr const char* kBuiltInApps[] = {
      kFileManagerAppId,
      kFileManagerSwaAppId,
      kTextEditorAppId,
      kAudioPlayerAppId,
      extension_misc::kQuickOfficeComponentExtensionId,
      extension_misc::kQuickOfficeInternalExtensionId,
      extension_misc::kQuickOfficeExtensionId};

  return base::Contains(kBuiltInApps, task.task_descriptor.app_id);
}

// Gets the profile in which a file task owned by |extension| should be
// launched - for example, it makes sure that a file task is not handled in OTR
// profile for platform apps (outside a guest session).
Profile* GetProfileForExtensionTask(Profile* profile,
                                    const extensions::Extension& extension) {
  // In guest profile, all available task handlers are in OTR profile.
  if (profile->IsGuestSession()) {
    DCHECK(profile->IsOffTheRecord());
    return profile;
  }

  // Outside guest sessions, if the task is handled by a platform app, launch
  // the handler in the original profile.
  if (extension.is_platform_app())
    return profile->GetOriginalProfile();
  return profile;
}

GURL GetIconURL(Profile* profile, const Extension& extension) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile) &&
      apps::AppServiceProxyFactory::GetForProfile(profile)
              ->AppRegistryCache()
              .GetAppType(extension.id()) != apps::AppType::kUnknown) {
    return apps::AppIconSource::GetIconURL(
        extension.id(), extension_misc::EXTENSION_ICON_SMALL);
  }
  return extensions::ExtensionIconSource::GetIconURL(
      &extension, extension_misc::EXTENSION_ICON_SMALL,
      ExtensionIconSet::MATCH_BIGGER,
      false);  // grayscale
}

void ExecuteTaskAfterMimeTypesCollected(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls,
    FileTaskFinishedCallback done,
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  bool is_arc_share = task.task_type == TASK_TYPE_ARC_APP &&
                      (task.action_id == kActionIdSend ||
                       task.action_id == kActionIdSendMultiple);
  bool is_web_app = task.task_type == TASK_TYPE_WEB_APP;
  bool is_chrome_app = task.task_type == TASK_TYPE_FILE_HANDLER;
  if (is_arc_share || is_web_app || is_chrome_app) {
    ExecuteAppServiceTask(profile, task, file_urls, *mime_types,
                          std::move(done));
    return;
  }

  DCHECK_EQ(task.task_type, TASK_TYPE_ARC_APP);
  apps::RecordAppLaunchMetrics(
      profile, apps::AppType::kArc, task.app_id,
      apps::mojom::LaunchSource::kFromFileManager,
      apps::mojom::LaunchContainer::kLaunchContainerWindow);
  ExecuteArcTask(profile, task, file_urls, *mime_types, std::move(done));
}

void EndPostProcessFoundTasks(std::unique_ptr<WebDriveOfficeValidationHelper>
                                  web_drive_office_validation_helper,
                              FindTasksCallback callback) {
  Profile* profile = web_drive_office_validation_helper.get()->profile;
  const std::vector<extensions::EntryInfo>& entries =
      web_drive_office_validation_helper.get()->entries;
  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list =
      std::move(web_drive_office_validation_helper.get()->result_list);
  ChooseAndSetDefaultTask(*profile->GetPrefs(), entries, result_list.get());
  std::move(callback).Run(std::move(result_list));
}

void PostProcessFoundTasks(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    FindTasksCallback callback,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list) {
  AdjustTasksForMediaApp(entries, result_list.get());

  // Google documents can only be handled by internal handlers.
  if (ContainsGoogleDocument(entries))
    KeepOnlyFileManagerInternalTasks(result_list.get());

  std::set<std::string> disabled_actions;

  // kFilesArchivemount2 controls what subset of filename extensions listed in
  // ui/file_manager/file_manager/manifest.json allows the "mount-archive"
  // action. If kFilesArchivemount2 is enabled, everything listed in
  // manifest.json is allowed.
  if (!base::FeatureList::IsEnabled(ash::features::kFilesArchivemount2)) {
    for (const auto& entry : entries) {
      // Deny-list: "slow-mounter" compressed formats.
      if (entry.path.MatchesFinalExtension(".bz") ||
          entry.path.MatchesFinalExtension(".bz2") ||
          entry.path.MatchesFinalExtension(".gz") ||
          entry.path.MatchesFinalExtension(".lz") ||
          entry.path.MatchesFinalExtension(".lzma") ||
          entry.path.MatchesFinalExtension(".taz") ||
          entry.path.MatchesFinalExtension(".tb2") ||
          entry.path.MatchesFinalExtension(".tbz") ||
          entry.path.MatchesFinalExtension(".tbz2") ||
          entry.path.MatchesFinalExtension(".tgz") ||
          entry.path.MatchesFinalExtension(".tlz") ||
          entry.path.MatchesFinalExtension(".tlzma") ||
          entry.path.MatchesFinalExtension(".txz") ||
          entry.path.MatchesFinalExtension(".tz") ||
          entry.path.MatchesFinalExtension(".tz2") ||
          entry.path.MatchesFinalExtension(".tzst") ||
          entry.path.MatchesFinalExtension(".xz") ||
          entry.path.MatchesFinalExtension(".z") ||
          entry.path.MatchesFinalExtension(".zst")) {
        disabled_actions.emplace("mount-archive");
        break;
      }
    }
  }

#if !BUILDFLAG(ENABLE_PDF)
  disabled_actions.emplace("view-pdf");
#endif  // !BUILDFLAG(ENABLE_PDF)

  std::unique_ptr<WebDriveOfficeValidationHelper>
      web_drive_office_validation_helper =
          std::make_unique<WebDriveOfficeValidationHelper>(
              profile, entries, std::move(result_list),
              std::move(disabled_actions));

  web_drive_office_validation_helper.get()->Run(base::BindOnce(
      &EndPostProcessFoundTasks, std::move(web_drive_office_validation_helper),
      std::move(callback)));
}

// Returns true if |extension_id| and |action_id| indicate that the file
// currently being handled should be opened with the browser. This function
// is used to handle certain action IDs of the file manager.
bool ShouldBeOpenedWithBrowser(const std::string& extension_id,
                               const std::string& action_id) {
  return isFilesAppId(extension_id) &&
         (action_id == "view-pdf" || action_id == "view-in-browser" ||
          action_id == "open-hosted-generic" ||
          action_id == "open-hosted-gdoc" ||
          action_id == "open-hosted-gsheet" ||
          action_id == "open-hosted-gslides" ||
          action_id == kActionIdWebDriveOfficeWord ||
          action_id == kActionIdWebDriveOfficeExcel ||
          action_id == kActionIdWebDriveOfficePowerPoint);
}

// Opens the files specified by |file_urls| with the browser for |profile|.
// Returns true on success. It's a failure if no files are opened.
bool OpenFilesWithBrowser(Profile* profile,
                          const std::vector<FileSystemURL>& file_urls,
                          const std::string& action_id) {
  int num_opened = 0;
  for (const FileSystemURL& file_url : file_urls) {
    if (chromeos::FileSystemBackend::CanHandleURL(file_url)) {
      num_opened +=
          util::OpenFileWithBrowser(profile, file_url, action_id) ? 1 : 0;
    }
  }
  return num_opened > 0;
}

}  // namespace

// Converts a string to a TaskType. Returns TASK_TYPE_UNKNOWN on error.
TaskType StringToTaskType(const std::string& str) {
  if (str == kFileBrowserHandlerTaskType)
    return TASK_TYPE_FILE_BROWSER_HANDLER;
  if (str == kFileHandlerTaskType)
    return TASK_TYPE_FILE_HANDLER;
  if (str == kArcAppTaskType)
    return TASK_TYPE_ARC_APP;
  if (str == kCrostiniAppTaskType)
    return TASK_TYPE_CROSTINI_APP;
  if (str == kWebAppTaskType)
    return TASK_TYPE_WEB_APP;
  if (str == kPluginVmAppTaskType)
    return TASK_TYPE_PLUGIN_VM_APP;
  return TASK_TYPE_UNKNOWN;
}

// Converts a TaskType to a string.
std::string TaskTypeToString(TaskType task_type) {
  switch (task_type) {
    case TASK_TYPE_FILE_BROWSER_HANDLER:
      return kFileBrowserHandlerTaskType;
    case TASK_TYPE_FILE_HANDLER:
      return kFileHandlerTaskType;
    case TASK_TYPE_ARC_APP:
      return kArcAppTaskType;
    case TASK_TYPE_CROSTINI_APP:
      return kCrostiniAppTaskType;
    case TASK_TYPE_WEB_APP:
      return kWebAppTaskType;
    case TASK_TYPE_PLUGIN_VM_APP:
      return kPluginVmAppTaskType;
    case TASK_TYPE_UNKNOWN:
    case DEPRECATED_TASK_TYPE_DRIVE_APP:
    case NUM_TASK_TYPE:
      break;
  }
  NOTREACHED();
  return "";
}

FullTaskDescriptor::FullTaskDescriptor(const TaskDescriptor& in_task_descriptor,
                                       const std::string& in_task_title,
                                       const Verb in_task_verb,
                                       const GURL& in_icon_url,
                                       bool in_is_default,
                                       bool in_is_generic_file_handler,
                                       bool in_is_file_extension_match)
    : task_descriptor(in_task_descriptor),
      task_title(in_task_title),
      task_verb(in_task_verb),
      icon_url(in_icon_url),
      is_default(in_is_default),
      is_generic_file_handler(in_is_generic_file_handler),
      is_file_extension_match(in_is_file_extension_match) {}

FullTaskDescriptor::FullTaskDescriptor(const FullTaskDescriptor& other) =
    default;

FullTaskDescriptor& FullTaskDescriptor::operator=(
    const FullTaskDescriptor& other) = default;

void UpdateDefaultTask(PrefService* pref_service,
                       const TaskDescriptor& task_descriptor,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  if (!pref_service)
    return;

  std::string task_id = TaskDescriptorToId(task_descriptor);
  if (!mime_types.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksByMimeType);
    for (const std::string& mime_type : mime_types) {
      mime_type_pref->SetStringKey(mime_type, task_id);
    }
  }

  std::set<std::string> lowercase_suffixes;
  if (!suffixes.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksBySuffix);
    for (const std::string& suffix : suffixes) {
      // Suffixes are case insensitive.
      std::string lower_suffix = base::ToLowerASCII(suffix);
      lowercase_suffixes.insert(lower_suffix);
      mime_type_pref->SetStringKey(lower_suffix, task_id);
    }
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kMediaAppHandlesPdf)) {
    RecordChangesInDefaultPdfApp(task_descriptor.app_id, mime_types,
                                 lowercase_suffixes);
  }
}

bool GetDefaultTaskFromPrefs(const PrefService& pref_service,
                             const std::string& mime_type,
                             const std::string& suffix,
                             TaskDescriptor* task_out) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
          << " and suffix: " << suffix;
  if (!mime_type.empty()) {
    const base::Value* mime_task_prefs =
        pref_service.GetDictionary(prefs::kDefaultTasksByMimeType);
    DCHECK(mime_task_prefs);
    LOG_IF(ERROR, !mime_task_prefs) << "Unable to open MIME type prefs";
    if (mime_task_prefs) {
      const std::string* task_id = mime_task_prefs->FindStringKey(mime_type);
      if (task_id) {
        VLOG(1) << "Found MIME default handler: " << *task_id;
        return ParseTaskID(*task_id, task_out);
      }
    }
  }

  const base::Value* suffix_task_prefs =
      pref_service.GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  LOG_IF(ERROR, !suffix_task_prefs) << "Unable to open suffix prefs";
  std::string lower_suffix = base::ToLowerASCII(suffix);
  if (!suffix_task_prefs)
    return false;

  const std::string* task_id = suffix_task_prefs->FindStringKey(lower_suffix);

  if (!task_id || task_id->empty())
    return false;

  VLOG(1) << "Found suffix default handler: " << *task_id;
  return ParseTaskID(*task_id, task_out);
}

std::string MakeTaskID(const std::string& app_id,
                       TaskType task_type,
                       const std::string& action_id) {
  return base::StringPrintf("%s|%s|%s", app_id.c_str(),
                            TaskTypeToString(task_type).c_str(),
                            action_id.c_str());
}

std::string TaskDescriptorToId(const TaskDescriptor& task_descriptor) {
  return MakeTaskID(task_descriptor.app_id, task_descriptor.task_type,
                    task_descriptor.action_id);
}

bool ParseTaskID(const std::string& task_id, TaskDescriptor* task) {
  DCHECK(task);

  std::vector<std::string> result = base::SplitString(
      task_id, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Parse a legacy task ID that only contain two parts. The legacy task IDs
  // can be stored in preferences.
  if (result.size() == 2) {
    task->task_type = TASK_TYPE_FILE_BROWSER_HANDLER;
    task->app_id = result[0];
    task->action_id = result[1];

    return true;
  }

  if (result.size() != 3)
    return false;

  TaskType task_type = StringToTaskType(result[1]);
  if (task_type == TASK_TYPE_UNKNOWN)
    return false;

  task->app_id = result[0];
  task->task_type = task_type;
  task->action_id = result[2];

  return true;
}

bool ExecuteFileTask(Profile* profile,
                     const TaskDescriptor& task,
                     const std::vector<FileSystemURL>& file_urls,
                     FileTaskFinishedCallback done) {
  UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType", task.task_type,
                            NUM_TASK_TYPE);
  if (drive::util::GetDriveConnectionStatus(profile) ==
      drive::util::DRIVE_DISCONNECTED_NONETWORK) {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Offline",
                              task.task_type, NUM_TASK_TYPE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Online",
                              task.task_type, NUM_TASK_TYPE);
  }

  // TODO(crbug.com/1005640): Move recording this metric to the App Service when
  // file handling is supported there.
  apps::RecordAppLaunch(task.app_id,
                        apps::mojom::LaunchSource::kFromFileManager);

  if (auto* notifier = FileTasksNotifier::GetForProfile(profile)) {
    notifier->NotifyFileTasks(file_urls);
  }

  // Some action IDs of the file manager's file browser handlers require the
  // files to be directly opened with the browser. In a multiprofile session
  // this will always open on the current desktop, regardless of which profile
  // owns the files, so return TASK_RESULT_OPENED.
  const std::string parsed_action_id(parseFilesAppActionId(task.action_id));
  if (ShouldBeOpenedWithBrowser(task.app_id, parsed_action_id)) {
    const bool result =
        OpenFilesWithBrowser(profile, file_urls, parsed_action_id);
    if (result && done) {
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
    }
    return result;
  }

  // When the FilesSWA is enabled: Open Files SWA if the task is for Files app.
  if (ash::features::IsFileManagerSwaEnabled() && isFilesAppId(task.app_id)) {
    std::u16string title;
    const GURL destination_entry =
        file_urls.size() ? file_urls[0].ToGURL() : GURL();
    ui::SelectFileDialog::FileTypeInfo file_type_info;
    file_type_info.allowed_paths =
        ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
    GURL files_swa_url =
        ::file_manager::util::GetFileManagerMainPageUrlWithParams(
            ui::SelectFileDialog::SELECT_NONE, title,
            /*current_directory_url=*/{},
            /*selection_url=*/destination_entry,
            /*target_name=*/{}, &file_type_info,
            /*file_type_index=*/0,
            /*search_query=*/{},
            /*show_android_picker_apps=*/false,
            /*volume_filter=*/{});

    web_app::SystemAppLaunchParams params;
    params.url = files_swa_url;

    web_app::LaunchSystemWebAppAsync(
        profile, ash::SystemWebAppType::FILE_MANAGER, params);
    if (done) {
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
    }
    return true;
  }

  // ARC apps and web apps need mime types for launching. Retrieve them first.
  if (task.task_type == TASK_TYPE_ARC_APP ||
      task.task_type == TASK_TYPE_WEB_APP ||
      task.task_type == TASK_TYPE_FILE_HANDLER) {
    // TODO(petermarshall): Implement GetProfileForExtensionTask in Lacros if
    // necessary, for Chrome Apps.
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector =
        new extensions::app_file_handler_util::MimeTypeCollector(profile);
    mime_collector->CollectForURLs(
        file_urls, base::BindOnce(&ExecuteTaskAfterMimeTypesCollected, profile,
                                  task, file_urls, std::move(done),
                                  base::Owned(mime_collector)));
    return true;
  }

  if (task.task_type == TASK_TYPE_CROSTINI_APP ||
      task.task_type == TASK_TYPE_PLUGIN_VM_APP) {
    DCHECK_EQ(kGuestOsAppActionID, task.action_id);
    ExecuteGuestOsTask(profile, task, file_urls, std::move(done));
    return true;
  }

  // Execute a file_browser_handler task in an Extension.
  if (task.task_type == TASK_TYPE_FILE_BROWSER_HANDLER) {
    // Get the extension.
    const Extension* extension = extensions::ExtensionRegistry::Get(profile)
                                     ->enabled_extensions()
                                     .GetByID(task.app_id);
    if (!extension)
      return false;

    Profile* extension_task_profile =
        GetProfileForExtensionTask(profile, *extension);
    return file_browser_handlers::ExecuteFileBrowserHandler(
        extension_task_profile, extension, task.action_id, file_urls,
        std::move(done));
  }
  NOTREACHED();
  return false;
}

void FindFileBrowserHandlerTasks(Profile* profile,
                                 const std::vector<GURL>& file_urls,
                                 std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!file_urls.empty());
  DCHECK(result_list);

  file_browser_handlers::FileBrowserHandlerList common_tasks =
      file_browser_handlers::FindFileBrowserHandlers(profile, file_urls);
  if (common_tasks.empty())
    return;

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (const FileBrowserHandler* handler : common_tasks) {
    const std::string extension_id = handler->extension_id();
    const Extension* extension = enabled_extensions.GetByID(extension_id);
    DCHECK(extension);

    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    const GURL icon_url = GetIconURL(profile, *extension);

    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(extension_id, file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                       handler->id()),
        handler->title(), Verb::VERB_NONE /* no verb for FileBrowserHandler */,
        icon_url, false /* is_default */, false /* is_generic_file_handler */,
        false /* is_file_extension_match */));
  }
}

void FindExtensionAndAppTasks(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    const std::vector<GURL>& file_urls,
    FindTasksCallback callback,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list) {
  std::vector<FullTaskDescriptor>* result_list_ptr = result_list.get();

  // 2. Web tasks file_handlers (View/Open With), Chrome app file_handlers, and
  // extension file_browser_handlers.
  FindAppServiceTasks(profile, entries, file_urls, result_list_ptr);

  // 3. Find and append Guest OS tasks.
  FindGuestOsTasks(profile, entries, file_urls, result_list_ptr,
                   // Done. Apply post-filtering and callback.
                   base::BindOnce(PostProcessFoundTasks, profile, entries,
                                  std::move(callback), std::move(result_list)));
}

void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         FindTasksCallback callback) {
  DCHECK(profile);
  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list(
      new std::vector<FullTaskDescriptor>);

  // 1. Find and append ARC handler tasks.
  FindArcTasks(profile, entries, file_urls, std::move(result_list),
               base::BindOnce(&FindExtensionAndAppTasks, profile, entries,
                              file_urls, std::move(callback)));
}

void ChooseAndSetDefaultTask(const PrefService& pref_service,
                             const std::vector<extensions::EntryInfo>& entries,
                             std::vector<FullTaskDescriptor>* tasks) {
  // Collect the default tasks from the preferences into a set.
  std::set<TaskDescriptor> default_tasks;
  for (const extensions::EntryInfo& entry : entries) {
    const base::FilePath& file_path = entry.path;
    const std::string& mime_type = entry.mime_type;
    TaskDescriptor default_task;
    if (file_tasks::GetDefaultTaskFromPrefs(
            pref_service, mime_type, file_path.Extension(), &default_task)) {
      default_tasks.insert(default_task);
    }
  }

  // Go through all the tasks from the beginning and see if there is any
  // default task. If found, pick and set it as default and return.
  for (FullTaskDescriptor& task : *tasks) {
    DCHECK(!task.is_default);
    if (base::Contains(default_tasks, task.task_descriptor)) {
      task.is_default = true;
      return;
    }
  }

  // No default task. If ShadowDocs is available for Office files, set as
  // default.
  for (FullTaskDescriptor& task : *tasks) {
    if (isWebDriveOfficeTask(task)) {
      task.is_default = true;
      return;
    }
  }

  // Check for an explicit file extension match (without MIME match) in the
  // extension manifest and pick that over the fallback handlers below (see
  // crbug.com/803930)
  for (FullTaskDescriptor& task : *tasks) {
    if (task.is_file_extension_match && !task.is_generic_file_handler &&
        !IsFallbackFileHandler(task)) {
      task.is_default = true;
      return;
    }
  }

  // Prefer a fallback app over viewing in the browser (crbug.com/1111399).
  // Unless it's HTML which should open in the browser (crbug.com/1121396).
  for (FullTaskDescriptor& task : *tasks) {
    if (IsFallbackFileHandler(task) &&
        parseFilesAppActionId(task.task_descriptor.action_id) !=
            "view-in-browser") {
      const extensions::EntryInfo entry = entries[0];
      const base::FilePath& file_path = entry.path;

      if (IsHtmlFile(file_path)) {
        break;
      }
      task.is_default = true;
      return;
    }
  }

  // No default tasks found. If there is any fallback file browser handler,
  // make it as default task, so it's selected by default.
  for (FullTaskDescriptor& task : *tasks) {
    DCHECK(!task.is_default);
    if (IsFallbackFileHandler(task)) {
      task.is_default = true;
      return;
    }
  }
}

bool IsHtmlFile(const base::FilePath& path) {
  constexpr const char* kHtmlExtensions[] = {".htm", ".html", ".mhtml",
                                             ".xht", ".xhtm", ".xhtml"};
  for (const char* extension : kHtmlExtensions) {
    if (path.MatchesExtension(extension))
      return true;
  }
  return false;
}

}  // namespace file_tasks
}  // namespace file_manager
