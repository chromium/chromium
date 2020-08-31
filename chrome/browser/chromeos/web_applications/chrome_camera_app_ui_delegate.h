// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/components/camera_app_ui/camera_app_ui_delegate.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_ui.h"

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

  // CameraAppUIDelegate
  void SetLaunchDirectory() override;
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  void OpenFileInGallery(const std::string& name) override;
  void OpenFeedbackDialog(const std::string& placeholder) override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
