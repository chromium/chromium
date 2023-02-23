// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks.h"

#include <stddef.h>

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/drive/drive_api_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "content/public/browser/network_service_instance.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

using ash::file_manager::kChromeUIFileManagerURL;
using extensions::Extension;

namespace file_manager::file_tasks {

const char kActionIdView[] = "view";
const char kActionIdSend[] = "send";
const char kActionIdSendMultiple[] = "send_multiple";
const char kActionIdQuickOffice[] = "qo_documents";
const char kActionIdWebDriveOfficeWord[] = "open-web-drive-office-word";
const char kActionIdWebDriveOfficeExcel[] = "open-web-drive-office-excel";
const char kActionIdWebDriveOfficePowerPoint[] =
    "open-web-drive-office-powerpoint";
const char kActionIdOpenInOffice[] = "open-in-office";
const char kActionIdOpenWeb[] = "OPEN_WEB";

const char kODFSExtensionId[] = "ajdgmkbkgifbokednjgbmieaemeighkg";

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
inline bool IsFilesAppId(const std::string& app_id) {
  return app_id == kFileManagerAppId || app_id == kFileManagerSwaAppId;
}

// The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID, just the
// sub-string compatible with the extension/legacy e.g.: "view-pdf".
std::string ParseFilesAppActionId(const std::string& action_id) {
  if (base::StartsWith(action_id, kChromeUIFileManagerURL)) {
    std::string result(action_id);
    base::ReplaceFirstSubstringAfterOffset(
        &result, 0, base::StrCat({kChromeUIFileManagerURL, "?"}), "");

    return result;
  }

  return action_id;
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
    if (IsFilesAppId(task.task_descriptor.app_id))
      filtered.push_back(std::move(task));
  }
  tasks->swap(filtered);
}

// Removes task |actions| handled by file manager.
void RemoveFileManagerInternalActions(const std::set<std::string>& actions,
                                      std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (FullTaskDescriptor& task : *tasks) {
    const auto& action = task.task_descriptor.action_id;
    if (!IsFilesAppId(task.task_descriptor.app_id)) {
      filtered.push_back(std::move(task));
    } else if (actions.find(ParseFilesAppActionId(action)) == actions.end()) {
      filtered.push_back(std::move(task));
    }
  }

  tasks->swap(filtered);
}

// Removes tasks handled by |app_id|".
void RemoveActionsForApp(const std::string& app_id,
                         std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (FullTaskDescriptor& task : *tasks) {
    if (app_id != task.task_descriptor.app_id) {
      filtered.push_back(std::move(task));
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
    return base::ranges::find(*tasks, app_id, [](const auto& task) {
      return task.task_descriptor.app_id;
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

void ExecuteTaskAfterMimeTypesCollected(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls,
    FileTaskFinishedCallback done,
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  if (task.task_type == TASK_TYPE_ARC_APP &&
      !ash::features::ShouldArcFileTasksUseAppService()) {
    apps::RecordAppLaunchMetrics(profile, apps::AppType::kArc, task.app_id,
                                 apps::LaunchSource::kFromFileManager,
                                 apps::LaunchContainer::kLaunchContainerWindow);
    ExecuteArcTask(profile, task, file_urls, *mime_types, std::move(done));
  } else {
    ExecuteAppServiceTask(profile, task, file_urls, *mime_types,
                          std::move(done));
  }
}

void PostProcessFoundTasks(Profile* profile,
                           const std::vector<extensions::EntryInfo>& entries,
                           FindTasksCallback callback,
                           std::unique_ptr<ResultingTasks> resulting_tasks) {
  AdjustTasksForMediaApp(entries, &resulting_tasks->tasks);

  // Google documents can only be handled by internal handlers.
  if (ContainsGoogleDocument(entries))
    KeepOnlyFileManagerInternalTasks(&resulting_tasks->tasks);

  std::set<std::string> disabled_actions;

#if !BUILDFLAG(ENABLE_PDF)
  disabled_actions.emplace("view-pdf");
#endif  // !BUILDFLAG(ENABLE_PDF)

  if (!ash::cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud()) {
    disabled_actions.emplace(kActionIdWebDriveOfficeWord);
    disabled_actions.emplace(kActionIdWebDriveOfficeExcel);
    disabled_actions.emplace(kActionIdWebDriveOfficePowerPoint);
  } else {
    // Hide the office PWA File Handler.
    RemoveActionsForApp(extension_misc::kOfficePwaAppId,
                        &resulting_tasks->tasks);

    // Hack around the fact that App Service will only return one task for each
    // app. We want both tasks to be available, so add the office task if the
    // WebDrive task is available.
    // TODO(petermarshall): Find a better way to enable both tasks.
    auto it = base::ranges::find_if(
        resulting_tasks->tasks, [](const FullTaskDescriptor& task) {
          if (!IsFilesAppId(task.task_descriptor.app_id)) {
            return false;
          }
          std::string action_id =
              ParseFilesAppActionId(task.task_descriptor.action_id);
          return action_id == kActionIdWebDriveOfficeWord ||
                 action_id == kActionIdWebDriveOfficeExcel ||
                 action_id == kActionIdWebDriveOfficePowerPoint;
        });
    if (it != resulting_tasks->tasks.end()) {
      FullTaskDescriptor office_task(*it);
      office_task.task_descriptor.action_id =
          base::StrCat({kChromeUIFileManagerURL, "?", kActionIdOpenInOffice});
      resulting_tasks->tasks.push_back(office_task);
    }
  }

  if (!disabled_actions.empty())
    RemoveFileManagerInternalActions(disabled_actions, &resulting_tasks->tasks);

  ChooseAndSetDefaultTask(profile, entries, resulting_tasks.get());
  std::move(callback).Run(std::move(resulting_tasks));
}

// Returns true if |extension_id| and |action_id| indicate that the file
// currently being handled should be opened with the browser. This function
// is used to handle certain action IDs of the file manager.
bool ShouldBeOpenedWithBrowser(const std::string& extension_id,
                               const std::string& action_id) {
  return IsFilesAppId(extension_id) &&
         (action_id == "view-pdf" || action_id == "view-in-browser" ||
          action_id == "open-hosted-generic" ||
          action_id == "open-hosted-gdoc" ||
          action_id == "open-hosted-gsheet" ||
          action_id == "open-hosted-gslides");
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
          util::OpenFileWithBrowser(profile, file_url, action_id) ? 1 : 0;
    }
  }
  return num_opened > 0;
}

bool ExecuteWebDriveOfficeTask(Profile* profile,
                               const TaskDescriptor& task,
                               const std::vector<FileSystemURL>& file_urls) {
  bool offline = drive::util::GetDriveConnectionStatus(profile) !=
                 drive::util::DRIVE_CONNECTED;
  if (offline) {
    UMA_HISTOGRAM_ENUMERATION(kDriveErrorMetricName,
                              OfficeDriveErrors::OFFLINE);
    // TODO(petermarshall): Quick Office vs. other default handler.
    return GetUserFallbackChoice(
        profile, task, file_urls,
        ash::office_fallback::FallbackReason::kOffline);
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service && integration_service->IsMounted() &&
      integration_service->GetDriveFsInterface()) {
    return ash::cloud_upload::OpenFilesWithCloudProvider(
        profile, file_urls, ash::cloud_upload::CloudProvider::kGoogleDrive);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kDriveErrorMetricName,
                              OfficeDriveErrors::DRIVEFS_INTERFACE);

    return GetUserFallbackChoice(
        profile, task, file_urls,
        ash::office_fallback::FallbackReason::kDriveUnavailable);
  }
}

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;

bool ExecuteOpenInOfficeTask(Profile* profile,
                             const TaskDescriptor& task,
                             const std::vector<FileSystemURL>& file_urls) {
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    return GetUserFallbackChoice(
        profile, task, file_urls,
        ash::office_fallback::FallbackReason::kOffline);
    // TODO(petermarshall): UMAs.
  }

  return ash::cloud_upload::OpenFilesWithCloudProvider(
      profile, file_urls, ash::cloud_upload::CloudProvider::kOneDrive);
}

}  // namespace

ResultingTasks::ResultingTasks() = default;
ResultingTasks::~ResultingTasks() = default;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDefaultHandlersForFileExtensions);
  registry->RegisterBooleanPref(
      prefs::kOfficeSetupComplete, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(prefs::kOfficeFilesAlwaysMove, false);
  registry->RegisterTimePref(prefs::kOfficeFileMovedToOneDrive, base::Time());
  registry->RegisterTimePref(prefs::kOfficeFileMovedToGoogleDrive,
                             base::Time());
}

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

bool TaskDescriptor::operator<(const TaskDescriptor& other) const {
  if (app_id < other.app_id) {
    return true;
  } else if (app_id > other.app_id) {
    return false;
  }

  // If we're here, it's because app_id == other.app_id.
  if (task_type < other.task_type) {
    return true;
  } else if (task_type > other.task_type) {
    return false;
  }

  // If we're here, it's because task_type == other.task_type.
  if (action_id < other.action_id) {
    return true;
  } else {
    return false;
  }
}

bool TaskDescriptor::operator==(const TaskDescriptor& other) const {
  if (app_id != other.app_id) {
    return false;
  }
  if (task_type != other.task_type) {
    return false;
  }
  if (action_id != other.action_id) {
    return false;
  }
  return true;
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
                       const std::set<std::string>& mime_types) {
  PrefService* pref_service = profile->GetPrefs();
  if (!pref_service)
    return;

  std::string task_id = TaskDescriptorToId(task_descriptor);
  if (ash::features::ShouldArcFileTasksUseAppService() &&
      task_descriptor.task_type == TASK_TYPE_ARC_APP) {
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

  if (!mime_types.empty()) {
    ScopedDictPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksByMimeType);
    for (const std::string& mime_type : mime_types) {
      mime_type_pref->Set(mime_type, task_id);
    }
  }

  std::set<std::string> lowercase_suffixes;
  if (!suffixes.empty()) {
    ScopedDictPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksBySuffix);
    for (const std::string& suffix : suffixes) {
      // Suffixes are case insensitive.
      std::string lower_suffix = base::ToLowerASCII(suffix);
      lowercase_suffixes.insert(lower_suffix);
      mime_type_pref->Set(lower_suffix, task_id);
    }
  }

  RecordChangesInDefaultPdfApp(task_descriptor.app_id, mime_types,
                               lowercase_suffixes);
}

bool GetDefaultTaskFromPrefs(const PrefService& pref_service,
                             const std::string& mime_type,
                             const std::string& suffix,
                             TaskDescriptor* task_out) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
          << " and suffix: " << suffix;
  if (!mime_type.empty()) {
    const base::Value::Dict& mime_task_prefs =
        pref_service.GetDict(prefs::kDefaultTasksByMimeType);
    const std::string* task_id = mime_task_prefs.FindString(mime_type);
    if (task_id) {
      VLOG(1) << "Found MIME default handler: " << *task_id;
      return ParseTaskID(*task_id, task_out);
    }
  }

  const base::Value::Dict& suffix_task_prefs =
      pref_service.GetDict(prefs::kDefaultTasksBySuffix);
  std::string lower_suffix = base::ToLowerASCII(suffix);

  const std::string* task_id = suffix_task_prefs.FindString(lower_suffix);

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
  apps::RecordAppLaunch(task.app_id, apps::LaunchSource::kFromFileManager);

  if (auto* notifier = FileTasksNotifier::GetForProfile(profile)) {
    notifier->NotifyFileTasks(file_urls);
  }

  const std::string parsed_action_id(ParseFilesAppActionId(task.action_id));

  if (IsWebDriveOfficeTask(task)) {
    const bool started = ExecuteWebDriveOfficeTask(profile, task, file_urls);
    if (done) {
      if (started) {
        std::move(done).Run(
            extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
      } else {
        std::move(done).Run(
            extensions::api::file_manager_private::TASK_RESULT_FAILED, "");
      }
    }
    return true;
  }
  if (IsOpenInOfficeTask(task)) {
    const bool started = ExecuteOpenInOfficeTask(profile, task, file_urls);
    if (done) {
      if (started) {
        std::move(done).Run(
            extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
      } else {
        std::move(done).Run(
            extensions::api::file_manager_private::TASK_RESULT_FAILED, "");
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
          extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
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
          extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
    }
    return true;
  }

  // Apps from App Service need mime types for launching. Retrieve them first.
  if (task.task_type == TASK_TYPE_ARC_APP ||
      task.task_type == TASK_TYPE_WEB_APP ||
      task.task_type == TASK_TYPE_FILE_HANDLER ||
      (ash::features::ShouldGuestOsFileTasksUseAppService() &&
       (task.task_type == TASK_TYPE_CROSTINI_APP ||
        task.task_type == TASK_TYPE_PLUGIN_VM_APP))) {
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

  if (!ash::features::ShouldGuestOsFileTasksUseAppService() &&
      (task.task_type == TASK_TYPE_CROSTINI_APP ||
       task.task_type == TASK_TYPE_PLUGIN_VM_APP)) {
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

void LaunchQuickOffice(Profile* profile,
                       const std::vector<FileSystemURL>& file_urls) {
  UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                            OfficeTaskResult::FALLBACK_QUICKOFFICE);

  const TaskDescriptor quick_office_task(
      extension_misc::kQuickOfficeComponentExtensionId, TASK_TYPE_FILE_HANDLER,
      kActionIdQuickOffice);

  file_tasks::ExecuteFileTask(
      profile, quick_office_task, file_urls,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {
            if (!error_message.empty()) {
              LOG(ERROR) << "Fallback to QuickOffice for opening office file "
                            "with error message: "
                         << error_message << " and result: " << result;
            }
          }));

  return;
}

void OnDialogChoiceReceived(Profile* profile,
                            const TaskDescriptor& task,
                            const std::vector<FileSystemURL>& file_urls,
                            const std::string& choice) {
  if (choice == ash::office_fallback::kDialogChoiceQuickOffice) {
    LaunchQuickOffice(profile, file_urls);
  } else if (choice == ash::office_fallback::kDialogChoiceTryAgain) {
    if (IsWebDriveOfficeTask(task)) {
      ExecuteWebDriveOfficeTask(profile, task, file_urls);
    } else if (IsOpenInOfficeTask(task)) {
      ExecuteOpenInOfficeTask(profile, task, file_urls);
    }
  }
}

bool GetUserFallbackChoice(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls,
    ash::office_fallback::FallbackReason fallback_reason) {
  // If QuickOffice is not installed, don't launch dialog.
  if (!IsExtensionInstalled(profile,
                            extension_misc::kQuickOfficeComponentExtensionId)) {
    LOG(ERROR) << "Cannot fallback to QuickOffice when it is not installed";
    return false;
  }
  // TODO(b/242685536) Add support for multi-file
  // selection so the OfficeFallbackDialog can display multiple file names and
  // `OnDialogChoiceReceived()` can open multiple files.
  std::vector<storage::FileSystemURL> first_url{file_urls.front()};

  ash::office_fallback::DialogChoiceCallback callback =
      base::BindOnce(&OnDialogChoiceReceived, profile, task, first_url);

  const std::string parsed_action_id = ParseFilesAppActionId(task.action_id);

  return ash::office_fallback::OfficeFallbackDialog::Show(
      first_url, fallback_reason, parsed_action_id, std::move(callback));
}

void FindExtensionAndAppTasks(Profile* profile,
                              const std::vector<extensions::EntryInfo>& entries,
                              const std::vector<GURL>& file_urls,
                              const std::vector<std::string>& dlp_source_urls,
                              FindTasksCallback callback,
                              std::unique_ptr<ResultingTasks> resulting_tasks) {
  auto* tasks = &resulting_tasks->tasks;

  // 2. Web tasks file_handlers (View/Open With), Chrome app file_handlers, and
  // extension file_browser_handlers.
  FindAppServiceTasks(profile, entries, file_urls, dlp_source_urls, tasks);

  // 3. Find and append Guest OS tasks directly if Guest OS file tasks aren't
  // provided by App Service.
  if (!ash::features::ShouldGuestOsFileTasksUseAppService()) {
    FindGuestOsTasks(
        profile, entries, file_urls, tasks,
        // Done. Apply post-filtering and callback.
        base::BindOnce(PostProcessFoundTasks, profile, entries,
                       std::move(callback), std::move(resulting_tasks)));
  } else {
    PostProcessFoundTasks(profile, entries, std::move(callback),
                          std::move(resulting_tasks));
  }
}

void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         const std::vector<std::string>& dlp_source_urls,
                         FindTasksCallback callback) {
  DCHECK(profile);
  auto resulting_tasks = std::make_unique<ResultingTasks>();
  if (!ash::features::ShouldArcFileTasksUseAppService()) {
    // 1. Find and append ARC handler tasks if ARC file tasks aren't
    // provided by App Service.
    FindArcTasks(
        profile, entries, file_urls, std::move(resulting_tasks),
        base::BindOnce(&FindExtensionAndAppTasks, profile, entries, file_urls,
                       dlp_source_urls, std::move(callback)));
  } else {
    FindExtensionAndAppTasks(profile, entries, file_urls, dlp_source_urls,
                             std::move(callback), std::move(resulting_tasks));
  }
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
    TaskDescriptor default_task;
    if (file_tasks::GetDefaultTaskFromPrefs(*profile->GetPrefs(), mime_type,
                                            file_path.Extension(),
                                            &default_task)) {
      default_tasks.insert(default_task);
      if (ash::features::ShouldArcFileTasksUseAppService() &&
          default_task.task_type == TASK_TYPE_ARC_APP) {
        // Default preference Task Descriptors for Android apps are stored in a
        // legacy format (app id: "<package>/<activity>", action id: "view"). To
        // match against ARC app task descriptors (which use app id: "<app
        // service id>", action id: "<activity>"), we translate the default Task
        // Descriptors into the new format.
        std::vector<std::string> app_id_info =
            base::SplitString(default_task.app_id, "/", base::KEEP_WHITESPACE,
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

bool IsWebDriveOfficeTask(const TaskDescriptor& task) {
  const std::string action_id = ParseFilesAppActionId(task.action_id);
  bool is_web_drive_office_action_id =
      action_id == kActionIdWebDriveOfficeWord ||
      action_id == kActionIdWebDriveOfficeExcel ||
      action_id == kActionIdWebDriveOfficePowerPoint;
  return IsFilesAppId(task.app_id) && is_web_drive_office_action_id;
}

bool IsOpenInOfficeTask(const TaskDescriptor& task) {
  const std::string action_id = ParseFilesAppActionId(task.action_id);
  return IsFilesAppId(task.app_id) && action_id == kActionIdOpenInOffice;
}

bool IsExtensionInstalled(Profile* profile, const std::string& extension_id) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  return registry->GetExtensionById(extension_id,
                                    extensions::ExtensionRegistry::ENABLED);
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

bool IsOfficeFile(const base::FilePath& path) {
  constexpr const char* kOfficeExtensions[] = {".doc",  ".docx", ".xls",
                                               ".xlsx", ".ppt",  ".pptx"};
  for (const char* extension : kOfficeExtensions) {
    if (path.MatchesExtension(extension))
      return true;
  }
  return false;
}

namespace {

std::string ToSwaActionId(const std::string& action_id) {
  return std::string(ash::file_manager::kChromeUIFileManagerURL) + "?" +
         action_id;
}

}  // namespace

void SetWordFileHandler(Profile* profile, TaskDescriptor& task) {
  UpdateDefaultTask(
      profile, task, {".doc", ".docx"},
      {"application/msword",
       "application/"
       "vnd.openxmlformats-officedocument.wordprocessingml.document"});
}
void SetWordFileHandlerToFilesSWA(Profile* profile,
                                  const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetWordFileHandler(profile, task);
}

void SetExcelFileHandler(Profile* profile, TaskDescriptor& task) {
  UpdateDefaultTask(
      profile, task, {".xls", ".xlsx"},
      {"application/vnd.ms-excel",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"});
}

void SetExcelFileHandlerToFilesSWA(Profile* profile,
                                   const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetExcelFileHandler(profile, task);
}

void SetPowerPointFileHandler(Profile* profile, TaskDescriptor& task) {
  UpdateDefaultTask(
      profile, task, {".ppt", ".pptx"},
      {"application/vnd.ms-powerpoint",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"});
}

void SetPowerPointFileHandlerToFilesSWA(Profile* profile,
                                        const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetPowerPointFileHandler(profile, task);
}

void SetOfficeSetupComplete(Profile* profile, bool complete) {
  profile->GetPrefs()->SetBoolean(prefs::kOfficeSetupComplete, complete);
}

bool OfficeSetupComplete(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kOfficeSetupComplete);
}

void SetAlwaysMoveOfficeFiles(Profile* profile, bool always_move) {
  profile->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMove, always_move);
}

bool AlwaysMoveOfficeFiles(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMove);
}

void SetOfficeFileMovedToOneDrive(Profile* profile, base::Time moved) {
  profile->GetPrefs()->SetTime(prefs::kOfficeFileMovedToOneDrive, moved);
}

void SetOfficeFileMovedToGoogleDrive(Profile* profile, base::Time moved) {
  profile->GetPrefs()->SetTime(prefs::kOfficeFileMovedToGoogleDrive, moved);
}

}  // namespace file_manager::file_tasks
