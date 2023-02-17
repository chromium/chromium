// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_HATS_TRIGGER_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_PRIVACY_HUB_HATS_TRIGGER_H_

#include "base/memory/scoped_refptr.h"

class Profile;

namespace ash {

class HatsNotificationController;

// Implementation for the Privacy Hub Hats surveys.
// This is a simple abstraction on top of the standard mechanisms to show a
// survey within the PrivacyHub specific limits.
class PrivacyHubHatsTrigger {
 public:
  PrivacyHubHatsTrigger();
  PrivacyHubHatsTrigger(const PrivacyHubHatsTrigger&) = delete;
  PrivacyHubHatsTrigger& operator=(const PrivacyHubHatsTrigger&) = delete;
  ~PrivacyHubHatsTrigger();

  // Show the survey to the current primary user if they are selected. If they
  // aren't or any of the surveys preconditions aren't met this does nothing.
  void ShowSurveyIfSelected();

 private:
  friend class PrivacyHubHatsTriggerTest;

  const HatsNotificationController* GetHatsNotificationControllerForTesting()
      const;
  void SetNoProfileForTesting(bool no_profile);
  Profile* GetProfile() const;

  scoped_refptr<HatsNotificationController> hats_controller_;
  bool no_profile_for_testing_ = false;
};

}  // namespace ash

#endif
