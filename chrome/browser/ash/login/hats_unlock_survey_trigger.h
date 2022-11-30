// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_HATS_UNLOCK_SURVEY_TRIGGER_H_
#define CHROME_BROWSER_ASH_LOGIN_HATS_UNLOCK_SURVEY_TRIGGER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/login/login_auth_recorder.h"

class Profile;

namespace ash {

// Used to show a Happiness Tracking Survey after a successful unlock to gather
// information about the unlocking experience. Separate configurations are used
// depending on whether the auth method used was Smart Lock.
// This is tied to LoginScreenClientImpl lifetime.
class HatsUnlockSurveyTrigger {
 public:
  using AuthMethod = LoginAuthRecorder::AuthMethod;

  // Thin wrapper around HatsNotificationController to facilitate testing.
  class Impl {
   public:
    Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    virtual ~Impl();

    virtual bool ShouldShowSurveyToProfile(Profile* profile,
                                           const HatsConfig& hats_config);
    virtual void ShowSurvey(
        Profile* profile,
        const HatsConfig& hats_config,
        const base::flat_map<std::string, std::string>& product_specific_data);

   private:
    scoped_refptr<HatsNotificationController> hats_notification_controller_;
  };

  // Uses default implementation.
  HatsUnlockSurveyTrigger();

  // For testing. Allows injecting a fake implementation.
  explicit HatsUnlockSurveyTrigger(std::unique_ptr<Impl> impl);

  HatsUnlockSurveyTrigger(const HatsUnlockSurveyTrigger&) = delete;
  HatsUnlockSurveyTrigger& operator=(const HatsUnlockSurveyTrigger&) = delete;
  ~HatsUnlockSurveyTrigger();

  // Checks to see if a survey should be shown to the profile associated with
  // |account_id|, and if so, displays the survey with some probability
  // determined by the Finch config file.  This method should be called after
  // the user is authenticated but before the session is unlocked. If the
  // profile isn't loaded, then the survey isn't shown.
  void ShowSurveyIfSelected(const AccountId& account_id, AuthMethod method);

  void SetProfileForTesting(Profile* profile);

 private:
  Profile* GetProfile(const AccountId& account_id);

  std::unique_ptr<Impl> impl_;
  Profile* profile_for_testing_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_HATS_UNLOCK_SURVEY_TRIGGER_H_
