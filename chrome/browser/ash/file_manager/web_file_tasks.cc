// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/web_file_tasks.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
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

void ExecuteWebTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    FileTaskFinishedCallback done) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetDeprecated(profile);
  web_app::WebAppRegistrar& registrar = provider->registrar();

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

  // No event flags means the launch container will be based on the app
  // settings.
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithFiles(
      task.app_id, ui::EF_NONE, apps::mojom::LaunchSource::kFromFileManager,
      std::move(launch_files));

  // In a multiprofile session, web apps always open on the current desktop,
  // regardless of which profile owns the files being opened, so use
  // TASK_RESULT_OPENED.
  std::move(done).Run(extensions::api::file_manager_private::TASK_RESULT_OPENED,
                      "");
}

}  // namespace file_tasks
}  // namespace file_manager
