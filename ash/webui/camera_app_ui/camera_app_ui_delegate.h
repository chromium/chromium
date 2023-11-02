// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_

#include <string>

#include "base/callback.h"

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
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
