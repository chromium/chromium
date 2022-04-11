// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_delegate.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
// TODO(b/174811949): Hide behind ChromeOS build flag.
#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_constants.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chromeos/constants/devicetype.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace {

std::string DeviceTypeToString(chromeos::DeviceType device_type) {
  switch (device_type) {
    case chromeos::DeviceType::kChromebase:
      return "chromebase";
    case chromeos::DeviceType::kChromebit:
      return "chromebit";
    case chromeos::DeviceType::kChromebook:
      return "chromebook";
    case chromeos::DeviceType::kChromebox:
      return "chromebox";
    case chromeos::DeviceType::kUnknown:
    default:
      return "unknown";
  }
}

}  // namespace

// static
void ChromeCameraAppUIDelegate::CameraAppDialog::ShowIntent(
    const std::string& queries,
    gfx::NativeWindow parent) {
  std::string url = ash::kChromeUICameraAppMainURL + queries;
  CameraAppDialog* dialog = new CameraAppDialog(url);
  dialog->ShowSystemDialog(parent);
}

ChromeCameraAppUIDelegate::CameraAppDialog::CameraAppDialog(
    const std::string& url)
    : chromeos::SystemWebDialogDelegate(GURL(url),
                                        /*title=*/std::u16string()) {}

ChromeCameraAppUIDelegate::CameraAppDialog::~CameraAppDialog() = default;

ui::ModalType ChromeCameraAppUIDelegate::CameraAppDialog::GetDialogModalType()
    const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ChromeCameraAppUIDelegate::CameraAppDialog::CanMaximizeDialog() const {
  return !ash::TabletMode::Get()->InTabletMode();
}

ui::WebDialogDelegate::FrameKind
ChromeCameraAppUIDelegate::CameraAppDialog::GetWebDialogFrameKind() const {
  // For customizing the title bar.
  return ui::WebDialogDelegate::FrameKind::kNonClient;
}

void ChromeCameraAppUIDelegate::CameraAppDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  auto grey_900 = cros_styles::ResolveColor(
      cros_styles::ColorName::kGoogleGrey900, /*is_dark_mode=*/false,
      /*use_debug_colors=*/false);
  params->init_properties_container.SetProperty(
      chromeos::kTrackDefaultFrameColors, false);
  params->init_properties_container.SetProperty(chromeos::kFrameActiveColorKey,
                                                grey_900);
  params->init_properties_container.SetProperty(
      chromeos::kFrameInactiveColorKey, grey_900);
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

ChromeCameraAppUIDelegate::FileMonitor::FileMonitor() = default;

ChromeCameraAppUIDelegate::FileMonitor::~FileMonitor() = default;

void ChromeCameraAppUIDelegate::FileMonitor::Monitor(
    const base::FilePath& file_path,
    base::OnceCallback<void(FileMonitorResult)> callback) {
  // Cancel the previous monitor callback if it hasn't been notified.
  if (!callback_.is_null()) {
    std::move(callback_).Run(FileMonitorResult::CANCELED);
  }

  // There is chance that the file is deleted during the task is scheduled and
  // executed. Therefore, check here before watching it.
  if (!base::PathExists(file_path)) {
    std::move(callback).Run(FileMonitorResult::DELETED);
    return;
  }

  callback_ = std::move(callback);
  file_watcher_ = std::make_unique<base::FilePathWatcher>();
  if (!file_watcher_->Watch(
          file_path, base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(
              &ChromeCameraAppUIDelegate::FileMonitor::OnFileDeletion,
              base::Unretained(this)))) {
    std::move(callback_).Run(FileMonitorResult::ERROR);
  }
}

void ChromeCameraAppUIDelegate::FileMonitor::OnFileDeletion(
    const base::FilePath& path,
    bool error) {
  if (callback_.is_null()) {
    return;
  }

  if (error) {
    std::move(callback_).Run(FileMonitorResult::ERROR);
    return;
  }
  std::move(callback_).Run(FileMonitorResult::DELETED);
}

ChromeCameraAppUIDelegate::ChromeCameraAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeCameraAppUIDelegate::InitFileMonitorOnFileThread,
                     base::Unretained(this)));
}

ChromeCameraAppUIDelegate::~ChromeCameraAppUIDelegate() {
  // Destroy |file_monitor_| on |file_task_runner_|.
  // TODO(wtlee): Ensure there is no lifetime issue before actually deleting it.
  file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_monitor_));
}

void ChromeCameraAppUIDelegate::SetLaunchDirectory() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  content::WebContents* web_contents = web_ui_->GetWebContents();

  auto my_files_folder_path =
      file_manager::util::GetMyFilesFolderForProfile(profile);

  auto* provider = web_app::WebAppProvider::GetForSystemWebApps(profile);
  absl::optional<web_app::AppId> app_id =
      provider->system_web_app_manager().GetAppIdForSystemApp(
          web_app::SystemAppType::CAMERA);

  // The launch directory is passed here rather than
  // `SystemWebAppDelegate::LaunchAndNavigateSystemWebApp()` to handle the case
  // of the app being opened to handle an Android intent, i.e. when it's shown
  // as a dialog via `CameraAppDialog`.
  web_app::WebAppLaunchParams launch_params;
  launch_params.started_new_navigation = true;
  launch_params.app_id = *app_id;
  launch_params.target_url = GURL(ash::kChromeUICameraAppMainURL);
  launch_params.dir = my_files_folder_path;
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
  web_app::WebAppTabHelper::FromWebContents(web_contents)
      ->EnsureLaunchQueue()
      .Enqueue(std::move(launch_params));
}

void ChromeCameraAppUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  // Add strings that can be pulled in.
  source->AddString("board_name", base::SysInfo::GetLsbReleaseBoard());
  source->AddString("device_type",
                    DeviceTypeToString(chromeos::GetDeviceType()));
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

  web_app::SystemAppLaunchParams params;
  params.launch_paths = {path};
  params.launch_source = apps::mojom::LaunchSource::kFromOtherApp;
  web_app::LaunchSystemWebAppAsync(Profile::FromWebUI(web_ui_),
                                   web_app::SystemAppType::MEDIA, params);
}

void ChromeCameraAppUIDelegate::OpenFeedbackDialog(
    const std::string& placeholder) {
  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  Profile* profile = Profile::FromWebUI(web_ui_);
  chrome::ShowFeedbackPage(GURL(ash::kChromeUICameraAppURL), profile,
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

void ChromeCameraAppUIDelegate::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsToggleAction::NoOp());
}

void ChromeCameraAppUIDelegate::MonitorFileDeletion(
    const std::string& name,
    base::OnceCallback<void(FileMonitorResult)> callback) {
  auto file_path = GetFilePathByName(name);
  if (file_path.empty()) {
    LOG(ERROR) << "Unexpected file name: " << name;
    std::move(callback).Run(FileMonitorResult::ERROR);
    return;
  }

  // We should return the response on current thread (mojo thread).
  auto callback_on_current_thread = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(callback), FROM_HERE);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeCameraAppUIDelegate::MonitorFileDeletionOnFileThread,
          base::Unretained(this), file_monitor_.get(), std::move(file_path),
          std::move(callback_on_current_thread)));
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

void ChromeCameraAppUIDelegate::InitFileMonitorOnFileThread() {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  file_monitor_ = std::make_unique<FileMonitor>();
}

void ChromeCameraAppUIDelegate::MonitorFileDeletionOnFileThread(
    FileMonitor* file_monitor,
    const base::FilePath& file_path,
    base::OnceCallback<void(FileMonitorResult)> callback) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  file_monitor->Monitor(file_path, std::move(callback));
}
