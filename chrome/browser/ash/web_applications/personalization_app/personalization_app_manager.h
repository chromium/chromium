// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

class HatsNotificationController;

namespace personalization_app {

enum class HatsSurveyType {
  kAvatar,
  kScreensaver,
  kWallpaper,
};

// Manager for the Chrome OS Personalization App. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the personalization app is not opened.
//
// Handles triggering HaTS surveys based on user interaction with
// personalization features.
class PersonalizationAppManager : public KeyedService {
 public:
  explicit PersonalizationAppManager(content::BrowserContext* context);

  PersonalizationAppManager(const PersonalizationAppManager& other) = delete;
  PersonalizationAppManager& operator=(const PersonalizationAppManager& other) =
      delete;

  ~PersonalizationAppManager() override;

  // Starts |hats_timer_| to show a Personalization survey if this user is
  // eligible for the survey. Will not start timer if a user has already seen a
  // Personalization survey during this session.
  void MaybeStartHatsTimer(HatsSurveyType hats_survey_type);

 private:
  // Callback to |hats_timer_|. Will show the given survey type.
  void OnHatsTimerDone(HatsSurveyType hats_survey_type);

  raw_ptr<content::BrowserContext> context_;

  base::OneShotTimer hats_timer_;

  scoped_refptr<HatsNotificationController> hats_notification_controller_;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_
