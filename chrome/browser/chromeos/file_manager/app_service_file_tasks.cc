// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {
// TODO(crbug/1092784): Only going to support ARC app and web app
// for now.
TaskType GetTaskType(apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      return TASK_TYPE_ARC_APP;
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb:
      return TASK_TYPE_WEB_APP;
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kLacros:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kBorealis:
      return TASK_TYPE_UNKNOWN;
  }
}

}  // namespace

void FindAppServiceTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         std::vector<FullTaskDescriptor>* result_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), file_urls.size());

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return;

  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> mime_types;
  for (auto& entry : entries)
    mime_types.push_back(entry.mime_type);
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      proxy->GetAppsForFiles(file_urls, mime_types);

  std::string task_action_id =
      entries.size() == 1 ? kActionIdSend : kActionIdSendMultiple;
  using extensions::api::file_manager_private::Verb;
  // TODO(crbug/1092784): Support file open with in the future.
  for (auto& launch_entry : intent_launch_info) {
    apps::mojom::AppType app_type =
        proxy->AppRegistryCache().GetAppType(launch_entry.app_id);
    // TODO(crbug/1092784): Only going to support ARC app and web app.
    if (!(app_type == apps::mojom::AppType::kArc ||
          app_type == apps::mojom::AppType::kWeb)) {
      continue;
    }

    constexpr int kIconSize = 32;
    GURL icon_url =
        apps::AppIconSource::GetIconURL(launch_entry.app_id, kIconSize);
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(launch_entry.app_id, GetTaskType(app_type),
                       task_action_id),
        launch_entry.activity_label, Verb::VERB_SHARE_WITH, icon_url,
        /* is_default=*/false,
        /* is_generic=*/true,
        /* is_file_extension_match=*/false));
  }
}

void ExecuteAppServiceTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(file_system_urls.size(), mime_types.size());

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return;

  constexpr auto launch_source = apps::mojom::LaunchSource::kFromFileManager;
  constexpr auto launch_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;

  std::vector<GURL> file_urls;

  for (auto& file_system_url : file_system_urls)
    file_urls.push_back(file_system_url.ToGURL());

  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithFileUrls(
      task.app_id,
      apps::GetEventFlags(launch_container, WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true),
      launch_source, file_urls, mime_types);

  // TODO(benwells): return the correct code here, depending on how the app will
  // be opened in multiprofile.
  std::move(done).Run(
      extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");
}

}  // namespace file_tasks
}  // namespace file_manager
