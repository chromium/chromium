// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_camera_app_ui_delegate.h"

#include <vector>

#include "ash/public/cpp/tablet_mode.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
// TODO(b/174811949): Hide behind ChromeOS build flag.
#include "chrome/browser/ash/web_applications/chrome_camera_app_ui_constants.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

// static
void ChromeCameraAppUIDelegate::CameraAppDialog::ShowIntent(
    const std::string& queries,
    gfx::NativeWindow parent) {
  std::string url = chromeos::kChromeUICameraAppMainURL + queries;
  CameraAppDialog* dialog = new CameraAppDialog(url);
  dialog->ShowSystemDialog(parent);
}

ChromeCameraAppUIDelegate::CameraAppDialog::CameraAppDialog(
    const std::string& url)
    : chromeos::SystemWebDialogDelegate(GURL(url),
                                        /*title=*/std::u16string()) {}

ChromeCameraAppUIDelegate::CameraAppDialog::~CameraAppDialog() {}

ui::ModalType ChromeCameraAppUIDelegate::CameraAppDialog::GetDialogModalType()
    const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ChromeCameraAppUIDelegate::CameraAppDialog::CanMaximizeDialog() const {
  return !ash::TabletMode::Get()->InTabletMode();
}

void ChromeCameraAppUIDelegate::CameraAppDialog::GetDialogSize(
    gfx::Size* size) const {
  size->SetSize(kChromeCameraAppDefaultWidth, kChromeCameraAppDefaultHeight);
}

void ChromeCameraAppUIDelegate::CameraAppDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /* extension */ nullptr);
}

bool ChromeCameraAppUIDelegate::CameraAppDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

ChromeCameraAppUIDelegate::ChromeCameraAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeCameraAppUIDelegate::SetLaunchDirectory() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  content::WebContents* web_contents = web_ui_->GetWebContents();

  // Since launch paths does not accept empty vector, we put a placeholder file
  // path in it.
  base::FilePath empty_file_path("/dev/null");

  auto my_files_folder_path =
      file_manager::util::GetMyFilesFolderForProfile(profile);

  web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
      web_contents, web_contents->GetURL(), my_files_folder_path,
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
  base::FilePath path = GetFilePathByName(name);
  if (path.empty()) {
    return;
  }

  auto&& file_paths = std::vector<base::FilePath>({path});
  apps::mojom::FilePathsPtr launch_files =
      apps::mojom::FilePaths::New(file_paths);

  apps::AppServiceProxyFactory::GetForProfile(Profile::FromWebUI(web_ui_))
      ->LaunchAppWithFiles(
          web_app::kMediaAppId,
          apps::mojom::LaunchContainer::kLaunchContainerWindow,
          apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerTab,
                              WindowOpenDisposition::NEW_FOREGROUND_TAB,
                              /* preferred_container=*/false),
          apps::mojom::LaunchSource::kFromOtherApp, std::move(launch_files));
}

void ChromeCameraAppUIDelegate::OpenFeedbackDialog(
    const std::string& placeholder) {
  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  Profile* profile = Profile::FromWebUI(web_ui_);
  chrome::ShowFeedbackPage(GURL(chromeos::kChromeUICameraAppURL), profile,
                           chrome::kFeedbackSourceCameraApp,
                           std::string() /* description_template */,
                           placeholder /* description_placeholder_text */,
                           "chromeos-camera-app" /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

std::string ChromeCameraAppUIDelegate::GetFilePathInArcByName(
    const std::string& name) {
  base::FilePath path = GetFilePathByName(name);
  if (path.empty()) {
    return std::string();
  }

  GURL arc_url_out;
  bool requires_sharing = false;
  if (!file_manager::util::ConvertPathToArcUrl(path, &arc_url_out,
                                               &requires_sharing) ||
      !arc_url_out.is_valid()) {
    return std::string();
  }
  if (requires_sharing) {
    LOG(ERROR) << "File path should be in MyFiles and not require any sharing";
    NOTREACHED();
    return std::string();
  }
  return arc_url_out.spec();
}

base::FilePath ChromeCameraAppUIDelegate::GetFilePathByName(
    const std::string& name) {
  // Check to avoid directory traversal attack.
  base::FilePath name_component(name);
  if (name_component.ReferencesParent())
    return base::FilePath();

  Profile* profile = Profile::FromWebUI(web_ui_);

  return file_manager::util::GetMyFilesFolderForProfile(profile)
      .Append("Camera")
      .Append(name_component);
}

void ChromeCameraAppUIDelegate::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsToggleAction::NoOp());
}
