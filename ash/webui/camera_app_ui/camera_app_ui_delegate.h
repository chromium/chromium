// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/camera_app_ui/ocr.mojom-forward.h"
#include "ash/webui/camera_app_ui/pdf_builder.mojom-forward.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class BrowserContext;
class WebContents;
class WebUIDataSource;
}  // namespace content

namespace media_device_salt {
class MediaDeviceSaltService;
}  // namespace media_device_salt

namespace ash {

class ProgressivePdf {
 public:
  virtual ~ProgressivePdf() = default;

  // Adds a new page to the PDF at `page_index`. If a page at `page_index`
  // already exists, it will be replaced.
  virtual void NewPage(const std::vector<uint8_t>& jpg,
                       uint32_t page_index) = 0;

  // Deletes the page of the PDF at `page_index`.
  virtual void DeletePage(uint32_t page_index) = 0;

  // Returns the PDF produced by the preceding operations.
  virtual void Save(base::OnceCallback<void(const std::vector<uint8_t>&)>) = 0;
};
// A delegate which exposes browser functionality from //chrome to the camera
// app ui page handler.
class CameraAppUIDelegate {
 public:
  enum class FileMonitorResult {
    // The file is deleted.
    kDeleted = 0,

    // The request is canceled since there is another monitor request.
    kCanceled = 1,

    // Fails to monitor the file due to errors.
    kError = 2,
  };

  enum class StorageMonitorStatus {
    // Storage has enough space to operate CCA functions.
    kNormal = 0,

    // Storage is getting low, display warning to users.
    kLow = 1,

    // Storage is almost full. Should stop ongoing recording and don't allow new
    // recording.
    kCriticallyLow = 2,

    // Monitoring got canceled since there is another monitor request.
    kCanceled = 3,

    // Monitoring get errors.
    kError = 4,
  };

  struct WifiConfig {
    WifiConfig();
    WifiConfig(const WifiConfig&);
    WifiConfig& operator=(const WifiConfig&);
    ~WifiConfig();

    std::string ssid;
    std::string security;
    std::optional<std::string> password;
    std::optional<std::string> eap_method;
    std::optional<std::string> eap_phase2_method;
    std::optional<std::string> eap_identity;
    std::optional<std::string> eap_anonymous_identity;
  };

  virtual ~CameraAppUIDelegate() = default;

  // Sets Downloads folder as launch directory by File Handling API so that we
  // can get the handle on the app side.
  virtual void SetLaunchDirectory() = 0;

  // Takes a WebUIDataSource, and adds load time data into it.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;

  // Checks if the logging consent option is enabled.
  virtual bool IsMetricsAndCrashReportingEnabled() = 0;

  // Opens the file in Downloads folder by its |name| in gallery.
  virtual void OpenFileInGallery(const std::string& name) = 0;

  // Opens the native chrome feedback dialog scoped to chrome://camera-app and
  // show |placeholder| in the description field.
  virtual void OpenFeedbackDialog(const std::string& placeholder) = 0;

  // Gets the file path in ARC file system by given file |name|.
  virtual std::string GetFilePathInArcByName(const std::string& name) = 0;

  // Opens the dev tools window.
  virtual void OpenDevToolsWindow(content::WebContents* web_contents) = 0;

  // Monitors deletion of the file by its |name|. The |callback| might be
  // triggered when the file is deleted, or when the monitor is canceled, or
  // when error occurs.
  virtual void MonitorFileDeletion(
      const std::string& name,
      base::OnceCallback<void(FileMonitorResult)> callback) = 0;

  // Maybe triggers HaTS survey for the camera app if all the conditions match.
  virtual void MaybeTriggerSurvey() = 0;

  // Start monitor storage status, |monitor_callback| will be called at initial
  // time and every time the status is changed.
  virtual void StartStorageMonitor(
      base::RepeatingCallback<void(StorageMonitorStatus)> monitor_callback) = 0;

  // Stop ongoing storage monitoring, if there is one, otherwise no-ops.
  virtual void StopStorageMonitor() = 0;

  // Open "Storage management" page in system's Settings app.
  virtual void OpenStorageManagement() = 0;

  // Gets the file path by given file |name|.
  virtual base::FilePath GetFilePathByName(const std::string& name) = 0;

  // Returns a service that provides persistent salts for generating media
  // device IDs. Can be null if the embedder does not support persistent salts.
  virtual media_device_salt::MediaDeviceSaltService* GetMediaDeviceSaltService(
      content::BrowserContext* context) = 0;

  // Opens a Wi-Fi connection dialog based on the given information.
  virtual void OpenWifiDialog(WifiConfig wifi_config) = 0;

  // Gets the system language of the current profile.
  virtual std::string GetSystemLanguage() = 0;

  // Returns the first page of a PDF as a JPEG.
  virtual void RenderPdfAsJpeg(
      const std::vector<uint8_t>& pdf,
      base::OnceCallback<void(const std::vector<uint8_t>&)> callback) = 0;

  // Performs OCR on the image and returns the OCR result.
  virtual void PerformOcr(
      base::span<const uint8_t> jpeg_data,
      base::OnceCallback<void(camera_app::mojom::OcrResultPtr)> callback) = 0;

  // Creates a PDF and provides operations to add and delete pages, and save the
  // searchified PDF.
  virtual void CreatePdfBuilder(
      mojo::PendingReceiver<camera_app::mojom::PdfBuilder>) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
