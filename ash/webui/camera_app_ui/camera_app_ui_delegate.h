// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"

namespace content {
class WebContents;
class WebUIDataSource;
}  // namespace content

namespace ash {

// A delegate which exposes browser functionality from //chrome to the camera
// app ui page handler.
class CameraAppUIDelegate {
 public:
  enum class FileMonitorResult {
    // The file is deleted.
    DELETED = 0,

    // The request is canceled since there is another monitor request.
    CANCELED = 1,

    // Fails to monitor the file due to errors.
    ERROR = 2,
  };

  enum class StorageMonitorStatus {
    // Storage has enough space to operate CCA functions.
    NORMAL = 0,

    // Storage is getting low, display warning to users.
    LOW = 1,

    // Storage is almost full. Should stop ongoing recording and don't allow new
    // recording.
    CRITICALLY_LOW = 2,

    // Monitoring got canceled since there is another monitor request.
    CANCELED = 3,

    // Monitoring get errors.
    ERROR = 4,
  };

  virtual ~CameraAppUIDelegate() = default;

  // Sets Downloads folder as launch directory by File Handling API so that we
  // can get the handle on the app side.
  virtual void SetLaunchDirectory() = 0;

  // Takes a WebUIDataSource, and adds load time data into it.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;

  // TODO(crbug.com/1113567): Remove this method once we migrate to use UMA to
  // collect metrics. Checks if the logging consent option is enabled.
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
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
