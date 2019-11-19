// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/entry_info.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

bool WebAppFileHandlingDisabled() {
  return !base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI) ||
         !base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI);
}

}  // namespace

void FindWebTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  std::vector<FullTaskDescriptor>* result_list) {
  if (WebAppFileHandlingDisabled())
    return;

  DCHECK(!entries.empty());
  DCHECK(result_list);

  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);
  web_app::AppRegistrar& registrar = provider->registrar();
  web_app::FileHandlerManager& file_handler_manager =
      provider->file_handler_manager();

  auto app_ids = registrar.GetAppIds();
  for (const auto& app_id : app_ids) {
    const auto* file_handlers = file_handler_manager.GetFileHandlers(app_id);

    if (!file_handlers)
      continue;

    std::vector<extensions::FileHandlerMatch> matches =
        extensions::app_file_handler_util::MatchesFromFileHandlersForEntries(
            *file_handlers, entries);

    if (matches.empty())
      continue;

    // WebApps only support "open with", so find the first good file handler
    // match (or the first match, if there is no good match).
    size_t best_index = 0;
    bool is_generic_handler = true;

    for (size_t i = 0; i < matches.size(); ++i) {
      if (IsGoodMatchFileHandler(*matches[i].handler, entries)) {
        best_index = i;
        is_generic_handler = false;
        break;
      }
    }

    GURL icon_url(base::StrCat({chrome::kChromeUIAppIconURL, app_id, "/32"}));

    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(app_id, file_tasks::TASK_TYPE_WEB_APP,
                       matches[best_index].handler->id),
        registrar.GetAppShortName(app_id),
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH, icon_url,
        /* is_default=*/false, is_generic_handler,
        matches[best_index].matched_file_extension));
  }
}

void ExecuteWebTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    FileTaskFinishedCallback done) {
  if (WebAppFileHandlingDisabled()) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Web app file handling is disabled.");
    return;
  }

  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);
  web_app::AppRegistrar& registrar = provider->registrar();

  if (!registrar.IsInstalled(task.app_id)) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        base::StrCat({"Web app ", task.app_id, " is not installed."}));
    return;
  }

  apps::mojom::LaunchContainer launch_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;

  // If the app isn't configured to open in a window, it should open as a tab.
  if (registrar.GetAppUserDisplayMode(task.app_id) !=
      blink::mojom::DisplayMode::kStandalone) {
    DCHECK_EQ(registrar.GetAppUserDisplayMode(task.app_id),
              blink::mojom::DisplayMode::kBrowser);
    launch_container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  }

  apps::AppLaunchParams params(
      task.app_id, launch_container, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceFileHandler);
  for (const auto& file_system_url : file_system_urls)
    params.launch_files.push_back(file_system_url.path());
  apps::LaunchService::Get(profile)->OpenApplication(params);

  std::move(done).Run(
      extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");
}

}  // namespace file_tasks
}  // namespace file_manager
