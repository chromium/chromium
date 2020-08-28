// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include "chromeos/components/camera_app_ui/camera_app_ui_delegate.h"
#include "content/public/browser/web_ui.h"

/**
 * Implementation of the CameraAppUIDelegate interface. Provides the camera app
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeCameraAppUIDelegate : public CameraAppUIDelegate {
 public:
  explicit ChromeCameraAppUIDelegate(content::WebUI* web_ui);

  ChromeCameraAppUIDelegate(const ChromeCameraAppUIDelegate&) = delete;
  ChromeCameraAppUIDelegate& operator=(const ChromeCameraAppUIDelegate&) =
      delete;

  // CameraAppUIDelegate
  void SetLaunchDirectory() override;
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  void OpenFileInGallery(const std::string& name) override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
