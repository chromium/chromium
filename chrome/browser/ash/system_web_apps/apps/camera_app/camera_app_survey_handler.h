// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_SURVEY_HANDLER_H_

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"

// Used to show a Happiness Tracking Survey when the camera app sends a trigger
// event.
class CameraAppSurveyHandler {
 public:
  CameraAppSurveyHandler();

  CameraAppSurveyHandler(const CameraAppSurveyHandler&) = delete;
  CameraAppSurveyHandler& operator=(const CameraAppSurveyHandler&) = delete;

  ~CameraAppSurveyHandler();

  static CameraAppSurveyHandler* GetInstance();

  void MaybeTriggerSurvey();

  void OnHardwareInfoFetched(base::SysInfo::HardwareInfo info);

 private:
  friend struct base::DefaultSingletonTraits<CameraAppSurveyHandler>;

  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;

  bool has_triggered_ = false;

  base::WeakPtrFactory<CameraAppSurveyHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CAMERA_APP_SURVEY_HANDLER_H_
