// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_HATS_TRIGGER_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_HATS_TRIGGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"

class Profile;

namespace ash {
namespace settings {
class PrivacyHubHandlerHatsTest;
}

class HatsNotificationController;

// Implementation for the Privacy Hub Hats surveys.
// This is a simple abstraction on top of the standard mechanisms to show a
// survey within the PrivacyHub specific limits.
class PrivacyHubHatsTrigger {
 public:
  static PrivacyHubHatsTrigger& Get();

  PrivacyHubHatsTrigger(const PrivacyHubHatsTrigger&) = delete;
  PrivacyHubHatsTrigger& operator=(const PrivacyHubHatsTrigger&) = delete;

  // Start the delay to show the survey to the user if they are selected. If the
  // user has already seen a survey or this is called while the delay hasn't
  // elapsed yet nothing will happen.
  void ShowSurveyAfterDelayElapsed();

 private:
  friend class base::NoDestructor<PrivacyHubHatsTrigger>;
  friend class PrivacyHubHatsTriggerTest;
  friend class settings::PrivacyHubHandlerHatsTest;

  PrivacyHubHatsTrigger();
  ~PrivacyHubHatsTrigger();

  const HatsNotificationController* GetHatsNotificationControllerForTesting()
      const;
  base::OneShotTimer& GetTimerForTesting();
  void SetNoProfileForTesting(bool no_profile);
  Profile* GetProfile() const;

  // Show the survey to the current primary user if they are selected. If they
  // aren't or any of the surveys preconditions aren't met this does nothing.
  void ShowSurveyIfSelected();

  base::OneShotTimer show_notification_timer_;
  scoped_refptr<HatsNotificationController> hats_controller_;
  bool no_profile_for_testing_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_HATS_TRIGGER_H_
