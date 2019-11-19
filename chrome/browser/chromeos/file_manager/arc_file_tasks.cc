// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/arc_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
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
// (see chrome/browser/chromeos/file_manager/file_tasks.h).
std::string ArcActionToFileTaskActionId(const std::string& action) {
  if (action == arc::kIntentActionView)
    return "view";
  else if (action == arc::kIntentActionSend)
    return "send";
  else if (action == arc::kIntentActionSendMultiple)
    return "send_multiple";
  NOTREACHED() << "Unhandled ARC action \"" << action << "\"";
  return "";
}

// TODO(derat): Replace this with a FileTaskActionIdToArcAction method once
// HandleUrlList has been updated to take a string action rather than an
// ArcActionType.
arc::mojom::ActionType FileTaskActionIdToArcActionType(const std::string& id) {
  if (id == "view")
    return arc::mojom::ActionType::VIEW;
  if (id == "send")
    return arc::mojom::ActionType::SEND;
  if (id == "send_multiple")
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

// Constructs a vector of UrlWithMimeType to be passed to
// IntentHelperInstance.HandleUrlListDeprecated.
std::vector<arc::mojom::UrlWithMimeTypePtr> ConstructUrlWithMimeTypeList(
    const std::vector<GURL>& content_urls,
    const std::vector<std::string>& mime_types) {
  std::vector<arc::mojom::UrlWithMimeTypePtr> urls;
  for (size_t i = 0; i < content_urls.size(); ++i) {
    arc::mojom::UrlWithMimeTypePtr url_with_type =
        arc::mojom::UrlWithMimeType::New();
    url_with_type->url = content_urls[i].spec();
    url_with_type->mime_type = mime_types[i];
    urls.push_back(std::move(url_with_type));
  }
  return urls;
}

// Constructs an OpenUrlsRequest to be passed to
// FileSystemInstance.OpenUrlsWithPermission.
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
void OnArcHandlerList(
    Profile* profile,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

void OnArcIconLoaded(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

// Called after the handlers from ARC is obtained. Proceeds to OnArcIconLoaded.
void OnArcHandlerList(
    Profile* profile,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  if (!intent_helper_bridge) {
    std::move(callback).Run(std::move(result_list));
    return;
  }

  std::vector<arc::mojom::IntentHandlerInfoPtr> handlers_filtered =
      arc::ArcIntentHelperBridge::FilterOutIntentHelper(std::move(handlers));
  std::vector<arc::ArcIntentHelperBridge::ActivityName> activity_names;
  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers_filtered)
    activity_names.emplace_back(handler->package_name, handler->activity_name);

  intent_helper_bridge->GetActivityIcons(
      activity_names,
      base::BindOnce(&OnArcIconLoaded, std::move(result_list),
                     std::move(callback), std::move(handlers_filtered)));
}

// Called after icon data for ARC apps are loaded. Proceeds to OnArcIconEncoded.
void OnArcIconLoaded(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    FindTasksCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using extensions::api::file_manager_private::Verb;
  for (const arc::mojom::IntentHandlerInfoPtr& handler : handlers) {
    std::string action(arc::kIntentActionView);
    if (handler->action.has_value())
      action = *handler->action;
    std::string name(handler->name);
    Verb handler_verb = Verb::VERB_NONE;
    if (action == arc::kIntentActionSend ||
        action == arc::kIntentActionSendMultiple) {
      handler_verb = Verb::VERB_SHARE_WITH;
    }
    auto it = icons->find(arc::ArcIntentHelperBridge::ActivityName(
        handler->package_name, handler->activity_name));
    const GURL& icon_url =
        (it == icons->end() ? GURL::EmptyGURL()
                            : it->second.icon16_dataurl->data);
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(
            ActivityNameToAppId(handler->package_name, handler->activity_name),
            TASK_TYPE_ARC_APP, ArcActionToFileTaskActionId(action)),
        name, handler_verb, icon_url, false /* is_default */,
        action != arc::kIntentActionView /* is_generic */,
        false /* is_file_extension_match */));
  }
  std::move(callback).Run(std::move(result_list));
}

void FindArcTasksAfterContentUrlsResolved(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    FindTasksCallback callback,
    const std::vector<GURL>& content_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), content_urls.size());

  arc::mojom::IntentHelperInstance* arc_intent_helper = nullptr;
  // File manager in secondary profile cannot access ARC.
  if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      arc_intent_helper = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          RequestUrlListHandlerList);
    }
  }
  if (!arc_intent_helper) {
    std::move(callback).Run(std::move(result_list));
    return;
  }

  std::vector<arc::mojom::UrlWithMimeTypePtr> urls;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    const GURL& content_url = content_urls[i];

    if (entry.is_directory) {  // ARC apps don't support directories.
      std::move(callback).Run(std::move(result_list));
      return;
    }

    if (!content_url.is_valid()) {
      std::move(callback).Run(std::move(result_list));
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
                     std::move(result_list), std::move(callback)));
}

void ExecuteArcTaskAfterContentUrlsResolved(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done,
    const std::vector<GURL>& content_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(content_urls.size(), mime_types.size());

  for (size_t i = 0; i < content_urls.size(); ++i) {
    if (!content_urls[i].is_valid()) {
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_FAILED,
          "Invalid url: " + content_urls[i].possibly_invalid_spec());
      return;
    }
  }

  // File manager in secondary profile cannot access ARC.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Not primary profile");
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "No ArcServiceManager");
    return;
  }

  // Try FileSystemInstance.OpenUrlsWithPermission first.
  arc::mojom::FileSystemInstance* arc_file_system = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->file_system(),
      OpenUrlsWithPermission);
  if (arc_file_system) {
    arc::mojom::OpenUrlsRequestPtr request =
        ConstructOpenUrlsRequest(task, content_urls, mime_types);
    arc_file_system->OpenUrlsWithPermission(std::move(request),
                                            base::DoNothing());
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");

    UMA_HISTOGRAM_ENUMERATION(
        "Arc.UserInteraction",
        arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER);

    return;
  }

  // Use IntentHelperInstance.HandleUrlListDeprecated as a fallback if
  // OpenUrlsWithPermission is not supported yet.
  // TODO(niwa): Remove this once we complete migration.
  arc::mojom::IntentHelperInstance* arc_intent_helper =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrlListDeprecated);
  if (arc_intent_helper) {
    LOG(WARNING) << "Using HandleUrlListDeprecated because "
                 << "OpenUrlsWithPermission is not supported yet.";
    arc_intent_helper->HandleUrlListDeprecated(
        ConstructUrlWithMimeTypeList(content_urls, mime_types),
        AppIdToActivityName(task.app_id),
        FileTaskActionIdToArcActionType(task.action_id));
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");

    UMA_HISTOGRAM_ENUMERATION(
        "Arc.UserInteraction",
        arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER);

    return;
  }

  std::move(done).Run(extensions::api::file_manager_private::TASK_RESULT_FAILED,
                      "No android app to run task");
}

}  // namespace

void FindArcTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  const std::vector<GURL>& file_urls,
                  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
                  FindTasksCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), file_urls.size());

  storage::FileSystemContext* file_system_context =
      util::GetFileSystemContextForExtensionId(profile, kFileManagerAppId);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const GURL& file_url : file_urls) {
    file_system_urls.push_back(file_system_context->CrackURL(file_url));
  }

  // Using base::Unretained(profile) is safe because callback will be invoked on
  // UI thread, where |profile| should be alive.
  file_manager::util::ConvertToContentUrls(
      file_system_urls,
      base::BindOnce(&FindArcTasksAfterContentUrlsResolved,
                     base::Unretained(profile), entries, std::move(result_list),
                     std::move(callback)));
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
  file_manager::util::ConvertToContentUrls(
      file_system_urls, base::BindOnce(&ExecuteArcTaskAfterContentUrlsResolved,
                                       base::Unretained(profile), task,
                                       mime_types, std::move(done)));
}

}  // namespace file_tasks
}  // namespace file_manager
