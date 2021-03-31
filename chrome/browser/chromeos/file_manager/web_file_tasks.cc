// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

void FindWebTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!entries.empty());
  DCHECK(result_list);

  // WebApps only have full support files backed by inodes, so tasks provided by
  // most Web Apps will be skipped if any non-native files are present. "System"
  // Web Apps are an exception: we have more control over what they can do, so
  // tasks provided by System Web Apps are the only ones permitted at present.
  // See https://crbug.com/1079065.
  bool has_special_file = false;
  for (const auto& entry : entries) {
    if (util::IsUnderNonNativeLocalPath(profile, entry.path)) {
      has_special_file = true;
      break;
    }
  }

  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);
  web_app::AppRegistrar& registrar = provider->registrar();
  web_app::OsIntegrationManager& os_integration_manager =
      provider->os_integration_manager();

  std::vector<web_app::AppId> app_ids = registrar.GetAppIds();
  for (const auto& app_id : app_ids) {
    if (has_special_file && app_id != web_app::kMediaAppId)
      continue;

    if (!os_integration_manager.IsFileHandlingAPIAvailable(app_id))
      continue;

    const auto* file_handlers =
        os_integration_manager.GetEnabledFileHandlers(app_id);

    if (!file_handlers)
      continue;

    std::vector<extensions::app_file_handler_util::WebAppFileHandlerMatch>
        matches = extensions::app_file_handler_util::
            MatchesFromWebAppFileHandlersForEntries(*file_handlers, entries);

    if (matches.empty())
      continue;

    // WebApps only support "open with", so find the first good file handler
    // match (or the first match, if there is no good match).
    size_t best_index = 0;
    bool is_generic_handler = true;

    for (size_t i = 0; i < matches.size(); ++i) {
      if (IsGoodMatchAppsFileHandler(matches[i].file_handler(), entries)) {
        best_index = i;
        is_generic_handler = false;
        break;
      }
    }

    GURL icon_url(base::StrCat({chrome::kChromeUIAppIconURL, app_id, "/32"}));

    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(app_id, file_tasks::TASK_TYPE_WEB_APP,
                       matches[best_index].file_handler().action.spec()),
        registrar.GetAppShortName(app_id),
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH, icon_url,
        /* is_default=*/false, is_generic_handler,
        matches[best_index].matched_file_extension()));
  }
}

void ExecuteWebTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    FileTaskFinishedCallback done) {
  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);
  web_app::AppRegistrar& registrar = provider->registrar();

  if (!registrar.IsInstalled(task.app_id)) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        base::StrCat({"Web app ", task.app_id, " is not installed."}));
    return;
  }

  if (!provider->os_integration_manager().IsFileHandlingAPIAvailable(
          task.app_id)) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Web app file handling disabled");
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

  apps::mojom::FilePathsPtr launch_files = apps::mojom::FilePaths::New();
  for (const auto& file_system_url : file_system_urls)
    launch_files->file_paths.push_back(file_system_url.path());

  // App Service doesn't exist in Incognito mode but apps can be
  // launched (ie. default handler to open a download from its
  // notification) from Incognito mode. Use the base profile in these
  // cases (see crbug.com/1111695).
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    profile = profile->GetOriginalProfile();
  }
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithFiles(
      task.app_id, launch_container,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerTab,
                          WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          /* preferred_containner=*/false),
      apps::mojom::LaunchSource::kFromFileManager, std::move(launch_files));

  // In a multiprofile session, web apps always open on the current desktop,
  // regardless of which profile owns the files being opened, so use
  // TASK_RESULT_OPENED.
  std::move(done).Run(extensions::api::file_manager_private::TASK_RESULT_OPENED,
                      "");
}

}  // namespace file_tasks
}  // namespace file_manager
