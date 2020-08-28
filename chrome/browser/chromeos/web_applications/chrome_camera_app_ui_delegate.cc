// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_camera_app_ui_delegate.h"

#include "base/files/file_path.h"
#include "base/system/sys_info.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_ui_data_source.h"

ChromeCameraAppUIDelegate::ChromeCameraAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeCameraAppUIDelegate::SetLaunchDirectory() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  content::WebContents* web_contents = web_ui_->GetWebContents();

  // Since launch paths does not accept empty vector, we put a placeholder file
  // path in it.
  base::FilePath empty_file_path("/dev/null");

  auto downloads_folder_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);

  web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
      web_contents, web_contents->GetURL(), downloads_folder_path,
      std::vector<base::FilePath>{empty_file_path});
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
}

void ChromeCameraAppUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  // Add strings that can be pulled in.
  source->AddString("board_name", base::SysInfo::GetLsbReleaseBoard());
}

bool ChromeCameraAppUIDelegate::IsMetricsAndCrashReportingEnabled() {
  // It is exposed for recording Google Analytics metrics.
  // TODO(crbug.com/1113567): Remove the method once the metrics is migrated to
  // UMA.
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

void ChromeCameraAppUIDelegate::OpenFileInGallery(const std::string& name) {
  // Check to avoid directory traversal attack.
  base::FilePath name_component(name);
  if (name_component.ReferencesParent())
    return;

  Profile* profile = Profile::FromWebUI(web_ui_);

  base::FilePath path =
      file_manager::util::GetDownloadsFolderForProfile(profile).Append(
          name_component);
  auto&& file_paths = std::vector<base::FilePath>({path});
  apps::mojom::FilePathsPtr launch_files =
      apps::mojom::FilePaths::New(file_paths);

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->LaunchAppWithFiles(
      chromeos::default_web_apps::kMediaAppId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerTab,
                          WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          /* preferred_container=*/false),
      apps::mojom::LaunchSource::kFromOtherApp, std::move(launch_files));
}
