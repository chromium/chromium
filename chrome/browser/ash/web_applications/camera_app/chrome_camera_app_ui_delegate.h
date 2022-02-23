// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include <memory>

#include "ash/webui/camera_app_ui/camera_app_ui_delegate.h"
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_ui.h"

class GURL;

namespace content {
struct MediaStreamRequest;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace blink {
namespace mojom {
enum class MediaStreamType;
}  // namespace mojom
}  // namespace blink

namespace ui {
enum ModalType;
}  // namespace ui

/**
 * Implementation of the CameraAppUIDelegate interface. Provides the camera app
 * code in ash/ with functions that only exist in chrome/.
 */
class ChromeCameraAppUIDelegate : public ash::CameraAppUIDelegate {
 public:
  class CameraAppDialog : public chromeos::SystemWebDialogDelegate {
   public:
    CameraAppDialog(const CameraAppDialog&) = delete;
    CameraAppDialog& operator=(const CameraAppDialog&) = delete;

    static void ShowIntent(const std::string& queries,
                           gfx::NativeWindow parent);

    // SystemWebDialogDelegate
    ui::ModalType GetDialogModalType() const override;
    bool CanMaximizeDialog() const override;
    ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;
    void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

    // ui::WebDialogDelegate
    void GetDialogSize(gfx::Size* size) const override;
    void RequestMediaAccessPermission(
        content::WebContents* web_contents,
        const content::MediaStreamRequest& request,
        content::MediaResponseCallback callback) override;
    bool CheckMediaAccessPermission(
        content::RenderFrameHost* render_frame_host,
        const GURL& security_origin,
        blink::mojom::MediaStreamType type) override;

   private:
    explicit CameraAppDialog(const std::string& url);
    ~CameraAppDialog() override;
  };

  class FileMonitor {
   public:
    FileMonitor();
    FileMonitor(const FileMonitor&) = delete;
    FileMonitor& operator=(const FileMonitor&) = delete;
    ~FileMonitor();

    void Monitor(const base::FilePath& file_path,
                 base::OnceCallback<void(FileMonitorResult)> callback);

   private:
    void OnFileDeletion(const base::FilePath& path, bool error);

    // Things which might be touched by the callback of |file_watcher_| should
    // be destroyed later than the destruction of |file_watcher_|.
    base::OnceCallback<void(FileMonitorResult)> callback_;
    std::unique_ptr<base::FilePathWatcher> file_watcher_;
  };

  explicit ChromeCameraAppUIDelegate(content::WebUI* web_ui);

  ChromeCameraAppUIDelegate(const ChromeCameraAppUIDelegate&) = delete;
  ChromeCameraAppUIDelegate& operator=(const ChromeCameraAppUIDelegate&) =
      delete;
  ~ChromeCameraAppUIDelegate() override;

  // ash::CameraAppUIDelegate
  void SetLaunchDirectory() override;
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  void OpenFileInGallery(const std::string& name) override;
  void OpenFeedbackDialog(const std::string& placeholder) override;
  std::string GetFilePathInArcByName(const std::string& name) override;
  void OpenDevToolsWindow(content::WebContents* web_contents) override;
  void MonitorFileDeletion(
      const std::string& name,
      base::OnceCallback<void(FileMonitorResult)> callback) override;

 private:
  base::FilePath GetFilePathByName(const std::string& name);
  void InitFileMonitorOnFileThread();
  void MonitorFileDeletionOnFileThread(
      FileMonitor* file_monitor,
      const base::FilePath& file_path,
      base::OnceCallback<void(FileMonitorResult)> callback);

  content::WebUI* web_ui_;  // Owns |this|.

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  // It should only be created, used and destroyed on |file_task_runner_|.
  std::unique_ptr<FileMonitor> file_monitor_;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_
