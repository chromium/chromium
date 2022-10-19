// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/arc_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

constexpr char kAppIdSeparator = '/';

// Converts an Android intent action (see kIntentAction* in
// components/arc/intent_helper/intent_constants.h) to a file task action ID
// (see chrome/browser/ash/file_manager/file_tasks.h).
std::string ArcActionToFileTaskActionId(const std::string& action) {
  if (action == arc::kIntentActionView)
    return kActionIdView;
  else if (action == arc::kIntentActionSend)
    return kActionIdSend;
  else if (action == arc::kIntentActionSendMultiple)
    return kActionIdSendMultiple;
  NOTREACHED() << "Unhandled ARC action \"" << action << "\"";
  return "";
}

// TODO(derat): Replace this with a FileTaskActionIdToArcAction method once
// HandleUrlList has been updated to take a string action rather than an
// ArcActionType.
arc::mojom::ActionType FileTaskActionIdToArcActionType(const std::string& id) {
  if (id == kActionIdView)
    return arc::mojom::ActionType::VIEW;
  if (id == kActionIdSend)
    return arc::mojom::ActionType::SEND;
  if (id == kActionIdSendMultiple)
    return arc::mojom::ActionType::SEND_MULTIPLE;
  NOTREACHED() << "Unhandled file task action ID \"" << id << "\"";
  return arc::mojom::ActionType::VIEW;
}

std::string ActivityNameToAppId(const std::string& package_name,
                                const std::string& activity_name) {
  return package_name + kAppIdSeparator + activity_name;
}

arc::mojom::ActivityNamePtr AppIdToActivityName(const std::string& id) {
  arc::mojom::ActivityNamePtr name = arc::mojom::ActivityName::New();

  const size_t separator = id.find(kAppIdSeparator);
  if (separator == std::string::npos) {
    name->package_name = id;
    name->activity_name = std::string();
  } else {
    name->package_name = id.substr(0, separator);
    name->activity_name = id.substr(separator + 1);
  }
  return name;
}

// Constructs an OpenUrlsRequest to be passed to
// FileSystemInstance.DEPRECATED_OpenUrlsWithPermission.
arc::mojom::OpenUrlsRequestPtr ConstructOpenUrlsRequest(
    const TaskDescriptor& task,
    const std::vector<GURL>& content_urls,
    const std::vector<std::string>& mime_types) {
  arc::mojom::OpenUrlsRequestPtr request = arc::mojom::OpenUrlsRequest::New();
  request->action_type = FileTaskActionIdToArcActionType(task.action_id);
  request->activity_name = AppIdToActivityName(task.app_id);
  for (size_t i = 0; i < content_urls.size(); ++i) {
    arc::mojom::ContentUrlWithMimeTypePtr url_with_type =
        arc::mojom::ContentUrlWithMimeType::New();
    url_with_type->content_url = content_urls[i];
    url_with_type->mime_type = mime_types[i];
    request->urls.push_back(std::move(url_with_type));
  }
  return request;
}

// Below is the sequence of thread-hopping for loading ARC file tasks.
void OnArcHandlerList(Profile* profile,
                      std::unique_ptr<ResultingTasks> resulting_tasks,
                      FindTasksCallback callback,
                      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

void OnArcIconLoaded(
    std::unique_ptr<ResultingTasks> resulting_tasks,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

// Called after the handlers from ARC is obtained. Proceeds to OnArcIconLoaded.
void OnArcHandlerList(Profile* profile,
                      std::unique_ptr<ResultingTasks> resulting_tasks,
                      FindTasksCallback callback,
                      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  if (!intent_helper_bridge) {
    LOG(ERROR) << "Failed to get ArcIntentHelperBridge";
    std::move(callback).Run(std::move(resulting_tasks));
    return;
  }

  std::vector<arc::mojom::IntentHandlerInfoPtr> handlers_filtered =
      arc::ArcIntentHelperBridge::FilterOutIntentHelper(std::move(handlers));
  std::vector<arc::ArcIntentHelperBridge::ActivityName> activity_names;
  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers_filtered)
    activity_names.emplace_back(handler->package_name, handler->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activity_names,
      base::BindOnce(&OnArcIconLoaded, std::move(resulting_tasks),
                     std::move(callback), std::move(handlers_filtered)));
}

// Called after icon data for ARC apps are loaded. Proceeds to OnArcIconEncoded.
void OnArcIconLoaded(
    std::unique_ptr<ResultingTasks> resulting_tasks,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers) {
    std::string action(arc::kIntentActionView);
    if (handler->action.has_value())
      action = *handler->action;
    std::string name(handler->name);
    if (action == arc::kIntentActionSend ||
        action == arc::kIntentActionSendMultiple) {
      // Use app service to get send tasks.
      continue;
    }
    auto it = icons->find(arc::ArcIntentHelperBridge::ActivityName(
        handler->package_name, handler->activity_name));
    const GURL& icon_url =
        (it == icons->end() ? GURL::EmptyGURL()
                            : it->second.icon16_dataurl->data);
    resulting_tasks->tasks.emplace_back(
        TaskDescriptor(
            ActivityNameToAppId(handler->package_name, handler->activity_name),
            TASK_TYPE_ARC_APP, ArcActionToFileTaskActionId(action)),
        name, icon_url, false /* is_default */,
        action != arc::kIntentActionView /* is_generic */,
        false /* is_file_extension_match */);
  }
  std::move(callback).Run(std::move(resulting_tasks));
}

// |ignore_paths_to_share| contains the paths to be shared to
// ARCVM via Seneschal. For FindArcTasksAfterContentUrlsResolved(),
// this can be ignored because the paths are not yet accessed.
void FindArcTasksAfterContentUrlsResolved(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    std::unique_ptr<ResultingTasks> resulting_tasks,
    FindTasksCallback callback,
    const std::vector<GURL>& content_urls,
    const std::vector<base::FilePath>& ignore_paths_to_share) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), content_urls.size());

  arc::mojom::IntentHelperInstance* arc_intent_helper = nullptr;
  // File manager in secondary profile cannot access ARC.
  if (ash::ProfileHelper::IsPrimaryProfile(profile)) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      arc_intent_helper = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          RequestUrlListHandlerList);
    } else {
      LOG(ERROR) << "Failed to get ArcServiceManager";
    }
  }
  if (!arc_intent_helper) {
    LOG(ERROR) << "Failed to get arc_intent_helper";
    std::move(callback).Run(std::move(resulting_tasks));
    return;
  }

  std::vector<arc::mojom::UrlWithMimeTypePtr> urls;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    const GURL& content_url = content_urls[i];

    if (entry.is_directory) {  // ARC apps don't support directories.
      std::move(callback).Run(std::move(resulting_tasks));
      return;
    }

    if (!content_url.is_valid()) {
      std::move(callback).Run(std::move(resulting_tasks));
      return;
    }

    arc::mojom::UrlWithMimeTypePtr url_with_type =
        arc::mojom::UrlWithMimeType::New();
    url_with_type->url = content_url.spec();
    url_with_type->mime_type = entry.mime_type;
    urls.push_back(std::move(url_with_type));
  }
  // The callback will be invoked on UI thread, so |profile| should be alive.
  arc_intent_helper->RequestUrlListHandlerList(
      std::move(urls),
      base::BindOnce(&OnArcHandlerList, base::Unretained(profile),
                     std::move(resulting_tasks), std::move(callback)));
}

void ExecuteArcTaskAfterContentUrlsResolved(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done,
    const std::vector<GURL>& content_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(content_urls.size(), mime_types.size());

  for (const GURL& content_url : content_urls) {
    if (!content_url.is_valid()) {
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_FAILED,
          "Invalid url: " + content_url.possibly_invalid_spec());
      return;
    }
  }

  // File manager in secondary profile cannot access ARC.
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Not primary profile");
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(ERROR) << "Failed to get ArcServiceManager";
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "No ArcServiceManager");
    return;
  }

  arc::mojom::FileSystemInstance* arc_file_system = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->file_system(),
      DEPRECATED_OpenUrlsWithPermission);
  if (!arc_file_system) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "OpenUrlsWithPermission is not supported");
    return;
  }

  arc::mojom::OpenUrlsRequestPtr request =
      ConstructOpenUrlsRequest(task, content_urls, mime_types);
  arc_file_system->DEPRECATED_OpenUrlsWithPermission(std::move(request),
                                                     base::DoNothing());
  // TODO(benwells): return the correct code here, depending on how the app
  // will be opened in multiprofile.
  std::move(done).Run(
      extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");

  arc::ArcMetricsService::RecordArcUserInteraction(
      profile, arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER);
}

}  // namespace

void FindArcTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  const std::vector<GURL>& file_urls,
                  std::unique_ptr<ResultingTasks> resulting_tasks,
                  FindTasksCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), file_urls.size());

  storage::FileSystemContext* file_system_context =
      util::GetFileManagerFileSystemContext(profile);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const GURL& file_url : file_urls) {
    file_system_urls.push_back(
        file_system_context->CrackURLInFirstPartyContext(file_url));
  }

  // Using base::Unretained(profile) is safe because callback will be invoked on
  // UI thread, where |profile| should be alive.
  file_manager::util::ConvertToContentUrls(
      ProfileManager::GetPrimaryUserProfile(), file_system_urls,
      base::BindOnce(&FindArcTasksAfterContentUrlsResolved,
                     base::Unretained(profile), entries,
                     std::move(resulting_tasks), std::move(callback)));
}

void ExecuteArcTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    const std::vector<std::string>& mime_types,
                    FileTaskFinishedCallback done) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(file_system_urls.size(), mime_types.size());

  // Using base::Unretained(profile) is safe because callback will be invoked on
  // UI thread, where |profile| should be alive.
  arc::ConvertToContentUrlsAndShare(
      ProfileManager::GetPrimaryUserProfile(), file_system_urls,
      base::BindOnce(&ExecuteArcTaskAfterContentUrlsResolved,
                     base::Unretained(profile), task, mime_types,
                     std::move(done)));
}

}  // namespace file_tasks
}  // namespace file_manager
