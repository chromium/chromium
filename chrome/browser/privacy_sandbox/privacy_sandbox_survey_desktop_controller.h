// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Profile;

namespace privacy_sandbox {

// This controller is responsible for managing privacy sandbox surveys for
// desktop.
class PrivacySandboxSurveyDesktopController : public KeyedService {
 public:
  explicit PrivacySandboxSurveyDesktopController(Profile* profile);
  ~PrivacySandboxSurveyDesktopController() override;

  // Called to surface the sentiment survey if the conditions are met.
  void MaybeShowSentimentSurvey();

  // Called to denote that we've visited a new tab page.
  void OnNewTabPageSeen();

 private:
  void OnSentimentSurveyShown();
  void OnSentimentSurveyFailure();

  // Tracks if a NTP has been seen within the current session.
  bool has_seen_ntp_ = false;
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<PrivacySandboxSurveyDesktopController> weak_ptr_factory_{
      this};

  friend class PrivacySandboxSurveyDesktopControllerLaunchSurveyTest;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SURVEY_DESKTOP_CONTROLLER_H_
