// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/components/camera_app_ui/camera_app_ui_delegate.h"
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
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeCameraAppUIDelegate : public CameraAppUIDelegate {
 public:
  class CameraAppDialog : public chromeos::SystemWebDialogDelegate {
   public:
    static void ShowIntent(const std::string& queries,
                           gfx::NativeWindow parent);

    // SystemWebDialogDelegate
    ui::ModalType GetDialogModalType() const override;
    bool CanMaximizeDialog() const override;

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

    DISALLOW_COPY_AND_ASSIGN(CameraAppDialog);
  };

  explicit ChromeCameraAppUIDelegate(content::WebUI* web_ui);

  ChromeCameraAppUIDelegate(const ChromeCameraAppUIDelegate&) = delete;
  ChromeCameraAppUIDelegate& operator=(const ChromeCameraAppUIDelegate&) =
      delete;
  ~ChromeCameraAppUIDelegate() override;

  // CameraAppUIDelegate
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
  void OnFileDeletion(const base::FilePath& path, bool error);

  content::WebUI* web_ui_;  // Owns |this|.

  std::unique_ptr<base::FilePathWatcher> file_watcher_;
  base::OnceCallback<void(FileMonitorResult)> cur_file_monitor_callback_;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
