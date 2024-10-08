// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks.h"

#include <stddef.h>

#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_browser_handlers.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_manager/uma_enums.gen.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_set.h"
#include "net/base/mime_util.h"
#include "pdf/buildflags.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

using ash::file_manager::kChromeUIFileManagerURL;
using extensions::Extension;

namespace file_manager::file_tasks {

namespace {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
constexpr char kFileBrowserHandlerTaskType[] = "file";
constexpr char kFileHandlerTaskType[] = "app";
constexpr char kArcAppTaskType[] = "arc";
constexpr char kBruschettaAppTaskType[] = "bruschetta";
constexpr char kCrostiniAppTaskType[] = "crostini";
constexpr char kPluginVmAppTaskType[] = "pluginvm";
constexpr char kWebAppTaskType[] = "web";

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kPdfFileExtension[] = ".pdf";


base::Value::Dict& GetDebugBaseValueDictForExecuteFileTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<base::Value::Dict> instance;
  return *instance;
}

void UpdateDebugBaseValue(const TaskDescriptor& task,
                          const std::vector<FileSystemURL>& file_urls) {
  auto urls_list = base::Value::List::with_capacity(file_urls.size());
  for (const auto& url : file_urls) {
    urls_list.Append(url.ToGURL().spec());
  }
  GetDebugBaseValueDictForExecuteFileTask() =
      base::Value::Dict()
          .Set("task", base::Value::Dict()
                           .Set("action_id", task.action_id)
                           .Set("app_id", task.app_id)
                           .Set("type", TaskTypeToString(task.task_type)))
          .Set("urls", std::move(urls_list));
}

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
inline bool IsFilesAppId(const std::string& app_id) {
  return app_id == kFileManagerAppId || app_id == kFileManagerSwaAppId;
}

// Returns true if path_mime_set contains a Google document.
bool ContainsGoogleDocument(const std::vector<extensions::EntryInfo>& entries) {
  return base::ranges::any_of(entries, &drive::util::HasHostedDocumentExtension,
                              &extensions::EntryInfo::path);
}

// Removes all tasks except tasks handled by file manager.
void KeepOnlyFileManagerInternalTasks(std::vector<FullTaskDescriptor>* tasks) {
  std::erase_if(*tasks, [](const auto& task) {
    return !IsFilesAppId(task.task_descriptor.app_id);
  });
}

// Removes task |actions| handled by file manager.
void RemoveFileManagerInternalActions(const std::set<std::string>& actions,
                                      std::vector<FullTaskDescriptor>* tasks) {
  std::erase_if(*tasks, [&actions](const auto& task) {
    const auto& td = task.task_descriptor;
    return IsFilesAppId(td.app_id) &&
           base::Contains(actions, ParseFilesAppActionId(td.action_id));
  });
}

// Removes tasks handled by |app_id|".
void RemoveActionsForApp(const std::string& app_id,
                         std::vector<FullTaskDescriptor>* tasks) {
  std::erase_if(*tasks, [&](const auto& task) {
    return task.task_descriptor.app_id == app_id;
  });
}

// Adjusts |tasks| to reflect the product decision that chrome://media-app
// should behave more like a user-installed app than a fallback handler.
// Specifically, only apps set as the default in user prefs should be preferred
// over chrome://media-app.
void AdjustTasksForMediaApp(const std::vector<extensions::EntryInfo>& entries,
                            std::vector<FullTaskDescriptor>* tasks) {
  const auto media_app_task = base::ranges::find(
      *tasks, web_app::kMediaAppId,
      [](const auto& task) { return task.task_descriptor.app_id; });

  if (media_app_task == tasks->end()) {
    return;
  }

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
  if (media_app_task == tasks->begin()) {
    return;
  }

  auto task = *media_app_task;
  tasks->erase(media_app_task);
  tasks->insert(tasks->begin(), std::move(task));
}

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
  constexpr auto kBuiltInApps = base::MakeFixedFlatSet<std::string_view>({
      // clang-format off
      kFileManagerAppId,
      kFileManagerSwaAppId,
      kTextEditorAppId,
      extension_misc::kQuickOfficeComponentExtensionId,
      extension_misc::kQuickOfficeInternalExtensionId,
      extension_misc::kQuickOfficeExtensionId,
      // clang-format on
  });

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
  if (extension.is_platform_app()) {
    return profile->GetOriginalProfile();
  }
  return profile;
}

void ExecuteTaskAfterMimeTypesCollected(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls,
    FileTaskFinishedCallback done,
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  ExecuteAppServiceTask(profile, task, file_urls, *mime_types, std::move(done));
}

void PostProcessFoundTasks(Profile* profile,
                           const std::vector<extensions::EntryInfo>& entries,
                           FindTasksCallback callback,
                           std::unique_ptr<ResultingTasks> resulting_tasks) {
  AdjustTasksForMediaApp(entries, &resulting_tasks->tasks);

  // Google documents can only be handled by internal handlers.
  if (ContainsGoogleDocument(entries)) {
    KeepOnlyFileManagerInternalTasks(&resulting_tasks->tasks);
  }

  std::set<std::string> disabled_actions;

#if !BUILDFLAG(ENABLE_PDF)
  disabled_actions.emplace("view-pdf");
#endif  // !BUILDFLAG(ENABLE_PDF)

  if (chromeos::IsEligibleAndEnabledUploadOfficeToCloud(profile)) {
    // Hide the MS365 PWA File Handler.
    RemoveActionsForApp(web_app::kMicrosoft365AppId, &resulting_tasks->tasks);
  }

  if (!disabled_actions.empty()) {
    RemoveFileManagerInternalActions(disabled_actions, &resulting_tasks->tasks);
  }

  ChooseAndSetDefaultTask(profile, entries, resulting_tasks.get());
  std::move(callback).Run(std::move(resulting_tasks));
}

// Returns true if |extension_id| and |action_id| indicate that the file
// currently being handled should be opened with the browser. This function
// is used to handle certain action IDs of the file manager.
bool ShouldBeOpenedWithBrowser(const std::string& extension_id,
                               const std::string& action_id) {
  constexpr auto kOpenWithBrowserActions =
      base::MakeFixedFlatSet<std::string_view>({
          // clang-format off
          "view-pdf",
          "view-in-browser",
          "open-encrypted",
          "open-hosted-generic",
          "open-hosted-gdoc",
          "open-hosted-gsheet",
          "open-hosted-gslides",
          // clang-format on
      });
  return IsFilesAppId(extension_id) &&
         base::Contains(kOpenWithBrowserActions, action_id);
}

// Opens the files specified by |file_urls| with the browser for |profile|.
// Returns true on success. It's a failure if no files are opened.
bool OpenFilesWithBrowser(Profile* profile,
                          const std::vector<FileSystemURL>& file_urls,
                          const std::string& action_id) {
  int num_opened = 0;
  for (const FileSystemURL& file_url : file_urls) {
    if (ash::FileSystemBackend::CanHandleURL(file_url)) {
      num_opened +=
          util::OpenFileWithAppOrBrowser(profile, file_url, action_id) ? 1 : 0;
    }
  }
  return num_opened > 0;
}

void RecordDriveOfflineUMAsGotDocsOfflineStats(
    bool open_available,
    drive::FileError error,
    drivefs::mojom::DocsOfflineStatsPtr stats) {
  // Adjust counts. Record 0 if docs offline extension was not available,
  // otherwise add 1 to distinguish from error.
  int total = 0;
  int available = 0;
  int unavailable = 0;
  if (error == drive::FileError::FILE_ERROR_OK) {
    total = stats->total + 1;
    available = stats->available_offline + 1;
    unavailable = stats->total - stats->available_offline + 1;
  }

  std::string name_prefix =
      base::StrCat({"FileBrowser.DriveOfflineHostedCount.OpenFile",
                    open_available ? "Available" : "Unavailable"});
  base::UmaHistogramCounts100000(name_prefix + ".Total", total);
  base::UmaHistogramCounts100000(name_prefix + ".Available", available);
  base::UmaHistogramCounts100000(name_prefix + ".Unavailable", unavailable);

  // Record percentage using unadjusted values when total > 0.
  if (stats->total > 0) {
    base::UmaHistogramPercentage(name_prefix + ".AvailablePercent",
                                 stats->available_offline * 100 / stats->total);
  }
}

void RecordDriveOfflineUMAsGotMetadata(
    Profile* profile,
    ViewFileType type,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  bool open_available = false;
  bool hosted = false;
  if (error == drive::FileError::FILE_ERROR_OK) {
    open_available = metadata->available_offline;
    hosted = metadata->type == drivefs::mojom::FileMetadata::Type::kHosted;
  }
  std::string name =
      base::StrCat({"FileBrowser.DriveOfflineOpen.",
                    open_available ? "Available" : "Unavailable"});
  base::UmaHistogramEnumeration(name, type);
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  // Collect docs offline stats for hosted files.
  if (integration_service && integration_service->IsMounted() && hosted) {
    integration_service->GetDocsOfflineStats(base::BindOnce(
        &RecordDriveOfflineUMAsGotDocsOfflineStats, open_available));
  }
}

void RecordDriveOfflineUMAs(Profile* profile,
                            const std::vector<FileSystemURL>& file_urls) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->IsMounted()) {
    return;
  }

  for (const FileSystemURL& file_url : file_urls) {
    if (file_url.type() == storage::kFileSystemTypeDriveFs) {
      ViewFileType type = GetViewFileType(file_url.path());
      integration_service->GetMetadata(
          file_url.path(),
          base::BindOnce(&RecordDriveOfflineUMAsGotMetadata, profile, type));
      if (!integration_service->IsOnline() &&
          drive::util::IsDriveFsBulkPinningAvailable(profile) &&
          profile->GetPrefs()->GetBoolean(
              drive::prefs::kDriveFsBulkPinningEnabled)) {
        base::UmaHistogramEnumeration(
            "FileBrowser.GoogleDrive.BulkPinning.OfflineOpen", type);
      }
    }
  }
}

// Files encrypted with Google Drive CSE have a specific MIME type; this helper
// returns whether the given MIME type denotes such a file.
bool IsEncryptedEntry(const extensions::EntryInfo& entry) {
  return drive::util::IsEncryptedMimeType(entry.mime_type);
}

}  // namespace

ResultingTasks::ResultingTasks() = default;
ResultingTasks::~ResultingTasks() = default;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Default handlers according to policy.
  registry->RegisterDictionaryPref(prefs::kDefaultHandlersForFileExtensions);

  // Dictionaries to keep track of default tasks in the file browser.
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  RegisterOfficeProfilePrefs(registry);
}

// Converts a string to a TaskType. Returns TASK_TYPE_UNKNOWN on error.
TaskType StringToTaskType(const std::string& str) {
  constexpr auto kStringToTaskTypeMapping =
      base::MakeFixedFlatMap<std::string_view, TaskType>({
          // clang-format off
          {kFileBrowserHandlerTaskType, TASK_TYPE_FILE_BROWSER_HANDLER},
          {kFileHandlerTaskType,        TASK_TYPE_FILE_HANDLER},
          {kArcAppTaskType,             TASK_TYPE_ARC_APP},
          {kBruschettaAppTaskType,      TASK_TYPE_BRUSCHETTA_APP},
          {kCrostiniAppTaskType,        TASK_TYPE_CROSTINI_APP},
          {kWebAppTaskType,             TASK_TYPE_WEB_APP},
          {kPluginVmAppTaskType,        TASK_TYPE_PLUGIN_VM_APP},
          // clang-format on
      });
  auto itr = kStringToTaskTypeMapping.find(str);
  if (itr != kStringToTaskTypeMapping.end()) {
    return itr->second;
  }
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
    case TASK_TYPE_BRUSCHETTA_APP:
      return kBruschettaAppTaskType;
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
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string ParseFilesAppActionId(const std::string& action_id) {
  if (base::StartsWith(action_id, kChromeUIFileManagerURL)) {
    // Length of `chrome://file-manager/?`.
    constexpr static size_t kOffset =
        std::char_traits<char>::length(kChromeUIFileManagerURL) + 1;
    return action_id.substr(kOffset);
  }

  return action_id;
}

std::string ToSwaActionId(std::string_view action_id) {
  return base::StrCat(
      {ash::file_manager::kChromeUIFileManagerURL, "?", action_id});
}

bool TaskDescriptor::operator<(const TaskDescriptor& other) const {
  return std::make_tuple(app_id, task_type, action_id) <
         std::make_tuple(other.app_id, other.task_type, other.action_id);
}

bool TaskDescriptor::operator==(const TaskDescriptor& other) const {
  return std::make_tuple(app_id, task_type, action_id) ==
         std::make_tuple(other.app_id, other.task_type, other.action_id);
}

FullTaskDescriptor::FullTaskDescriptor(const TaskDescriptor& in_task_descriptor,
                                       const std::string& in_task_title,
                                       const GURL& in_icon_url,
                                       bool in_is_default,
                                       bool in_is_generic_file_handler,
                                       bool in_is_file_extension_match,
                                       bool is_dlp_blocked)
    : task_descriptor(in_task_descriptor),
      task_title(in_task_title),
      icon_url(in_icon_url),
      is_default(in_is_default),
      is_generic_file_handler(in_is_generic_file_handler),
      is_file_extension_match(in_is_file_extension_match),
      is_dlp_blocked(is_dlp_blocked) {}

FullTaskDescriptor::FullTaskDescriptor(const FullTaskDescriptor& other) =
    default;

FullTaskDescriptor& FullTaskDescriptor::operator=(
    const FullTaskDescriptor& other) = default;

void UpdateDefaultTask(Profile* profile,
                       const TaskDescriptor& task_descriptor,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types,
                       bool replace_existing) {
  PrefService* pref_service = profile->GetPrefs();
  if (!pref_service) {
    return;
  }

  std::string task_id = TaskDescriptorToId(task_descriptor);
  if (task_descriptor.task_type == TASK_TYPE_ARC_APP) {
    // Task IDs for Android apps are stored in a legacy format (app id:
    // "<package>/<activity>", task id: "view"). For ARC app task descriptors
    // (which use app id: "<app service id>", action id: "<activity>"), we
    // generate Task IDs in the legacy format.
    std::string package;
    DCHECK(
        apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    if (proxy) {
      proxy->AppRegistryCache().ForOneApp(
          task_descriptor.app_id, [&package](const apps::AppUpdate& update) {
            package = update.PublisherId();
          });
    }
    if (!package.empty()) {
      std::string new_app_id = package + "/" + task_descriptor.action_id;
      task_id = MakeTaskID(new_app_id, TASK_TYPE_ARC_APP, kActionIdView);
    }
  }

  std::set<std::string> mime_types_to_set = mime_types;
  std::set<std::string> suffixes_to_set;
  // Suffixes are case insensitive.
  base::ranges::transform(
      suffixes, std::inserter(suffixes_to_set, suffixes_to_set.begin()),
      [](const std::string& suffix) { return base::ToLowerASCII(suffix); });

  // In the special case where we are setting the default for one type of Office
  // file only, set defaults for the entire group as well.
  if (mime_types.size() == 1 && suffixes.size() == 1) {
    if (base::Contains(WordGroupExtensions(), *suffixes.begin())) {
      suffixes_to_set = WordGroupExtensions();
      mime_types_to_set = WordGroupMimeTypes();
    } else if (base::Contains(ExcelGroupExtensions(), *suffixes.begin())) {
      suffixes_to_set = ExcelGroupExtensions();
      mime_types_to_set = ExcelGroupMimeTypes();
    } else if (base::Contains(PowerPointGroupExtensions(), *suffixes.begin())) {
      suffixes_to_set = PowerPointGroupExtensions();
      mime_types_to_set = PowerPointGroupMimeTypes();
    }
  }

  if (!mime_types_to_set.empty()) {
    ScopedDictPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksByMimeType);
    for (const std::string& mime_type : mime_types_to_set) {
      if (!replace_existing && mime_type_pref->contains(mime_type)) {
        continue;
      }
      mime_type_pref->Set(mime_type, task_id);
    }
  }

  if (!suffixes.empty()) {
    ScopedDictPrefUpdate suffix_pref(pref_service,
                                     prefs::kDefaultTasksBySuffix);
    for (const std::string& suffix : suffixes_to_set) {
      if (!replace_existing && suffix_pref->contains(suffix)) {
        continue;
      }
      suffix_pref->Set(suffix, task_id);
    }
  }

  RecordChangesInDefaultPdfApp(task_descriptor.app_id, mime_types_to_set,
                               suffixes_to_set);
}

void RemoveDefaultTask(Profile* profile,
                       const TaskDescriptor& task_descriptor,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  PrefService* pref_service = profile->GetPrefs();
  std::string task_id = TaskDescriptorToId(task_descriptor);

  std::set<std::string> suffixes_to_remove;
  // Suffixes are case insensitive.
  base::ranges::transform(
      suffixes, std::inserter(suffixes_to_remove, suffixes_to_remove.begin()),
      [](const std::string& suffix) { return base::ToLowerASCII(suffix); });

  ScopedDictPrefUpdate mime_type_pref(pref_service,
                                      prefs::kDefaultTasksByMimeType);
  for (const auto& mime_type : mime_types) {
    std::string* pref_value = mime_type_pref->FindString(mime_type);
    if (pref_value && *pref_value == task_id) {
      mime_type_pref->Remove(mime_type);
    }
  }

  ScopedDictPrefUpdate suffix_pref(pref_service, prefs::kDefaultTasksBySuffix);
  for (const auto& suffix : suffixes_to_remove) {
    std::string* pref_value = suffix_pref->FindString(suffix);
    if (pref_value && *pref_value == task_id) {
      suffix_pref->Remove(suffix);
    }
  }
}

std::optional<TaskDescriptor> GetDefaultTaskFromPrefs(
    const PrefService& pref_service,
    const std::string& mime_type,
    const std::string& suffix) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
          << " and suffix: " << suffix;
  if (!mime_type.empty()) {
    const base::Value::Dict& mime_task_prefs =
        pref_service.GetDict(prefs::kDefaultTasksByMimeType);
    const std::string* task_id = mime_task_prefs.FindString(mime_type);
    if (task_id) {
      VLOG(1) << "Found MIME default handler: " << *task_id;
      return ParseTaskID(*task_id);
    }
  }

  const base::Value::Dict& suffix_task_prefs =
      pref_service.GetDict(prefs::kDefaultTasksBySuffix);
  std::string lower_suffix = base::ToLowerASCII(suffix);

  const std::string* task_id = suffix_task_prefs.FindString(lower_suffix);

  if (!task_id || task_id->empty()) {
    return std::nullopt;
  }

  VLOG(1) << "Found suffix default handler: " << *task_id;
  return ParseTaskID(*task_id);
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

std::optional<TaskDescriptor> ParseTaskID(const std::string& task_id) {
  std::vector<std::string> result = base::SplitString(
      task_id, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Parse a legacy task ID that only contain two parts. The legacy task IDs
  // can be stored in preferences.
  if (result.size() == 2) {
    return TaskDescriptor(/*in_app_id=*/result[0],
                          TASK_TYPE_FILE_BROWSER_HANDLER,
                          /*in_action_id=*/result[1]);
  }

  if (result.size() != 3) {
    return std::nullopt;
  }

  TaskType task_type = StringToTaskType(result[1]);
  if (task_type == TASK_TYPE_UNKNOWN) {
    return std::nullopt;
  }

  return TaskDescriptor(/*in_app_id=*/result[0], task_type,
                        /*in_action_id=*/result[2]);
}

bool ExecuteFileTask(Profile* profile,
                     const TaskDescriptor& task,
                     const std::vector<FileSystemURL>& file_urls,
                     FileTaskFinishedCallback done) {
  // Save some of the arguments of "the most recent ExecuteFileTask" in JSON
  // (base::Value) format.
  UpdateDebugBaseValue(task, file_urls);

  UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType", task.task_type,
                            NUM_TASK_TYPE);
  if (drive::util::GetDriveConnectionStatus(profile) ==
      drive::util::ConnectionStatus::kNoNetwork) {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Offline",
                              task.task_type, NUM_TASK_TYPE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Online",
                              task.task_type, NUM_TASK_TYPE);
  }

  // TODO(crbug.com/40099553): Move recording this metric to the App Service
  // when file handling is supported there.
  apps::RecordAppLaunch(task.app_id, apps::LaunchSource::kFromFileManager);
  RecordDriveOfflineUMAs(profile, file_urls);

  if (auto* notifier = FileTasksNotifierFactory::GetForProfile(profile)) {
    notifier->NotifyFileTasks(file_urls);
  }

  const std::string parsed_action_id(ParseFilesAppActionId(task.action_id));

  if (IsWebDriveOfficeTask(task)) {
    UMA_HISTOGRAM_ENUMERATION(
        ash::cloud_upload::kOpenInitialCloudProviderMetric,
        ash::cloud_upload::CloudProvider::kGoogleDrive);
  } else if (IsOpenInOfficeTask(task)) {
    UMA_HISTOGRAM_ENUMERATION(
        ash::cloud_upload::kOpenInitialCloudProviderMetric,
        ash::cloud_upload::CloudProvider::kOneDrive);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        ash::cloud_upload::kOpenInitialCloudProviderMetric,
        ash::cloud_upload::CloudProvider::kNone);
  }
  // TODO(b/284800493): Add a test that VirtualTasks get run.
  if (IsVirtualTask(task)) {
    const bool started = ExecuteVirtualTask(profile, task, file_urls);
    if (done) {
      if (started) {
        std::move(done).Run(
            extensions::api::file_manager_private::TaskResult::kOpened, "");
      } else {
        std::move(done).Run(
            extensions::api::file_manager_private::TaskResult::kFailed, "");
      }
    }
    return true;
  }

  // Some action IDs of the file manager's file browser handlers require the
  // files to be directly opened with the browser. In a multiprofile session
  // this will always open on the current desktop, regardless of which profile
  // owns the files, so return TASK_RESULT_OPENED.
  if (ShouldBeOpenedWithBrowser(task.app_id, parsed_action_id)) {
    const bool result =
        OpenFilesWithBrowser(profile, file_urls, parsed_action_id);
    if (result && done) {
      std::move(done).Run(
          extensions::api::file_manager_private::TaskResult::kOpened, "");
    }
    return result;
  }

  for (const FileSystemURL& file_url : file_urls) {
    if (file_manager::util::IsDriveLocalPath(profile, file_url.path()) &&
        file_manager::file_tasks::IsOfficeFile(file_url.path())) {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::
              OPEN_FROM_FILES_APP);
    }
  }

  // Open Files SWA if the task is for Files app.
  if (IsFilesAppId(task.app_id)) {
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

    ash::SystemAppLaunchParams params;
    params.url = files_swa_url;

    ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                                 params);
    if (done) {
      std::move(done).Run(
          extensions::api::file_manager_private::TaskResult::kOpened, "");
    }
    return true;
  }

  // Apps from App Service need mime types for launching. Retrieve them first.
  if (task.task_type == TASK_TYPE_ARC_APP ||
      task.task_type == TASK_TYPE_WEB_APP ||
      task.task_type == TASK_TYPE_FILE_HANDLER ||
      task.task_type == TASK_TYPE_BRUSCHETTA_APP ||
      task.task_type == TASK_TYPE_CROSTINI_APP ||
      task.task_type == TASK_TYPE_PLUGIN_VM_APP) {
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

  // Execute a file_browser_handler task in an Extension.
  if (task.task_type == TASK_TYPE_FILE_BROWSER_HANDLER) {
    // Get the extension.
    const Extension* extension = extensions::ExtensionRegistry::Get(profile)
                                     ->enabled_extensions()
                                     .GetByID(task.app_id);
    if (!extension) {
      return false;
    }

    Profile* extension_task_profile =
        GetProfileForExtensionTask(profile, *extension);
    return file_browser_handlers::ExecuteFileBrowserHandler(
        extension_task_profile, extension, task.action_id, file_urls,
        std::move(done));
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GetDebugJSONForKeyForExecuteFileTask(
    std::string_view key,
    base::OnceCallback<void(std::pair<std::string_view, base::Value>)>
        callback) {
  std::move(callback).Run(std::make_pair(
      key, base::Value(GetDebugBaseValueDictForExecuteFileTask().Clone())));
}

void FindExtensionAndAppTasks(Profile* profile,
                              const std::vector<extensions::EntryInfo>& entries,
                              const std::vector<GURL>& file_urls,
                              const std::vector<std::string>& dlp_source_urls,
                              FindTasksCallback callback,
                              std::unique_ptr<ResultingTasks> resulting_tasks) {
  auto* tasks = &resulting_tasks->tasks;

  // Web tasks file_handlers (View/Open With), Chrome app file_handlers, and
  // extension file_browser_handlers.
  FindAppServiceTasks(profile, entries, file_urls, dlp_source_urls, tasks);

  // Done. Apply post-filtering and callback.
  PostProcessFoundTasks(profile, entries, std::move(callback),
                        std::move(resulting_tasks));
}

void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         const std::vector<std::string>& dlp_source_urls,
                         FindTasksCallback callback) {
  DCHECK(profile);
  auto resulting_tasks = std::make_unique<ResultingTasks>();
  bool has_encrypted_item = base::ranges::any_of(entries, &IsEncryptedEntry);
  bool all_encrypted_items = base::ranges::all_of(entries, &IsEncryptedEntry);
  if (has_encrypted_item) {
    if (all_encrypted_items) {
      resulting_tasks->tasks.emplace_back(FullTaskDescriptor(
          TaskDescriptor(kFileManagerAppId, TASK_TYPE_FILE_HANDLER,
                         "open-encrypted"),
          "", GURL(), false, false, false));
    }
    std::move(callback).Run(std::move(resulting_tasks));
    return;
  }

  // TODO(b/284800493): Add a test that VirtualTasks are found.
  MatchVirtualTasks(profile, entries, file_urls, dlp_source_urls,
                    &resulting_tasks->tasks);

  FindExtensionAndAppTasks(profile, entries, file_urls, dlp_source_urls,
                           std::move(callback), std::move(resulting_tasks));
}

void ChooseAndSetDefaultTask(Profile* profile,
                             const std::vector<extensions::EntryInfo>& entries,
                             ResultingTasks* resulting_tasks) {
  if (ChooseAndSetDefaultTaskFromPolicyPrefs(profile, entries,
                                             resulting_tasks)) {
    // If the function returns true, then the default selection has been
    // affected by policy. Check that |policy_default_handler_status| is set.
    DCHECK(resulting_tasks->policy_default_handler_status);
    return;
  }

  // Otherwise check that |policy_default_handler_status| is not set.
  DCHECK(!resulting_tasks->policy_default_handler_status);

  // Collect the default tasks from the preferences into a set.
  base::flat_set<TaskDescriptor> default_tasks;
  for (const extensions::EntryInfo& entry : entries) {
    const base::FilePath& file_path = entry.path;
    const std::string& mime_type = entry.mime_type;
    if (std::optional<TaskDescriptor> default_task =
            file_tasks::GetDefaultTaskFromPrefs(*profile->GetPrefs(), mime_type,
                                                file_path.Extension())) {
      default_tasks.insert(*default_task);
      if (default_task->task_type == TASK_TYPE_ARC_APP) {
        // Default preference Task Descriptors for Android apps are stored in a
        // legacy format (app id: "<package>/<activity>", action id: "view"). To
        // match against ARC app task descriptors (which use app id: "<app
        // service id>", action id: "<activity>"), we translate the default Task
        // Descriptors into the new format.
        std::vector<std::string> app_id_info =
            base::SplitString(default_task->app_id, "/", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY);
        if (app_id_info.size() != 2) {
          continue;
        }
        const std::string& package = app_id_info[0];
        const std::string& activity = app_id_info[1];

        Profile* profile_with_app_service = GetProfileWithAppService(profile);
        if (profile_with_app_service) {
          // Add possible alternative forms of this task descriptor to our list
          // of default tasks.
          apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service)
              ->AppRegistryCache()
              .ForEachApp([&default_tasks, package,
                           activity](const apps::AppUpdate& update) {
                if (update.PublisherId() == package) {
                  TaskDescriptor alternate_default_task(
                      update.AppId(), TASK_TYPE_ARC_APP, activity);
                  default_tasks.insert(alternate_default_task);
                }
              });
        }
      }
    }
  }

  auto& tasks = resulting_tasks->tasks;

  // Go through all the tasks from the beginning and see if there is any
  // default task. If found, pick and set it as default and return.
  for (FullTaskDescriptor& task : tasks) {
    DCHECK(!task.is_default);
    if (base::Contains(default_tasks, task.task_descriptor)) {
      task.is_default = true;
      return;
    }
  }

  // No default task. If the "Open in Docs/Sheets/Slides through Drive" workflow
  // is available for Office files, set as default.
  for (FullTaskDescriptor& task : tasks) {
    if (IsWebDriveOfficeTask(task.task_descriptor)) {
      task.is_default = true;
      return;
    }
  }

  // No default task. If the "Open in Microsoft 365" workflow is available for
  // Office files, set as default.
  // This step only makes sense in the enterprise environment when the
  // corresponding Google workflow above is disabled by policy.
  for (FullTaskDescriptor& task : tasks) {
    if (IsOpenInOfficeTask(task.task_descriptor)) {
      task.is_default = true;
      return;
    }
  }

  // Check for an explicit file extension match (without MIME match) in the
  // extension manifest and pick that over the fallback handlers below (see
  // crbug.com/803930)
  for (FullTaskDescriptor& task : tasks) {
    if (task.is_file_extension_match && !task.is_generic_file_handler &&
        !IsFallbackFileHandler(task)) {
      task.is_default = true;
      return;
    }
  }

  // Prefer a fallback app over viewing in the browser (crbug.com/1111399).
  // Unless it's HTML which should open in the browser (crbug.com/1121396).
  for (FullTaskDescriptor& task : tasks) {
    if (IsFallbackFileHandler(task) &&
        ParseFilesAppActionId(task.task_descriptor.action_id) !=
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
  for (FullTaskDescriptor& task : tasks) {
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
    if (path.MatchesExtension(extension)) {
      return true;
    }
  }
  return false;
}

}  // namespace file_manager::file_tasks
