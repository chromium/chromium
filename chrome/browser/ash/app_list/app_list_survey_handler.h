// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SURVEY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace app_list {

// Used to show a Happiness Tracking Survey when the launcher is opened.
class AppListSurveyHandler : public ProfileObserver {
 public:
  explicit AppListSurveyHandler(Profile* profile);

  AppListSurveyHandler(const AppListSurveyHandler&) = delete;
  AppListSurveyHandler& operator=(const AppListSurveyHandler&) = delete;

  ~AppListSurveyHandler() override;

  void MaybeTriggerSurvey();

  ash::HatsNotificationController* GetHatsNotificationControllerForTesting()
      const;

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;

  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_SURVEY_HANDLER_H_
