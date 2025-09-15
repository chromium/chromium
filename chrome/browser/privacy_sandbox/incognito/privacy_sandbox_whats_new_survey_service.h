// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_WHATS_NEW_SURVEY_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_WHATS_NEW_SURVEY_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)
#error This file should only be included on Win, Mac or Linux
#endif

namespace privacy_sandbox {

// A service responsible for managing and potentially displaying a survey
// to users after they have interacted with the "What's New" page, specifically
// concerning Privacy Sandbox features. This service is profile-scoped.
class PrivacySandboxWhatsNewSurveyService : public KeyedService {
 public:
  // Enum to track the outcome of the survey request.
  //
  // LINT.IfChange(PrivacySandboxWhatsNewSurveyStatus)
  enum class WhatsNewSurveyStatus {
    kSurveyShown = 0,  // The survey was successfully shown.
    kFeatureDisabled =
        1,  // The survey was not shown because the feature is disabled.
    kHatsServiceFailed = 2,   // The HaTS service was unavailable or failed.
    kSurveyLaunchFailed = 3,  // The survey launch failed.
    kSurveyLaunched =
        4,  // The survey was launched (can overlap with other statuses)
    kMaxValue = kSurveyLaunched,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxWhatsNewSurveyStatus)

  explicit PrivacySandboxWhatsNewSurveyService(Profile* profile);

  ~PrivacySandboxWhatsNewSurveyService() override;

  // Checks if the conditions are met to show the "What's New" survey and
  // requests the HatsService to display it if appropriate.
  // Conditions include feature flags, whether the user has seen the page, etc.
  // |web_contents| is the context in which the survey may be shown.
  void MaybeShowSurvey(content::WebContents* web_contents);

 private:
  // Checks if the "What's New" survey feature is enabled
  bool IsSurveyEnabled();

  // Records the final status of the attempt to show the survey to UMA
  // histograms.
  void RecordSurveyStatus(WhatsNewSurveyStatus status);

  // Callback function executed when the HaTS survey is actually shown to the
  // user.
  void OnSurveyShown();

  // Callback function executed if the HaTS survey fails to show for any reason
  // after being requested.
  void OnSurveyFailure();

  raw_ptr<Profile> profile_;
  // Factory for creating weak pointers to this service.
  base::WeakPtrFactory<PrivacySandboxWhatsNewSurveyService> weak_ptr_factory_{
      this};

  // Provide access to protected fields and methods to tests.
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxWhatsNewSurveyServiceTest,
                           IsWhatsNewSurveyEnabled_DisabledByDefault);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxWhatsNewSurveyServiceTest,
                           RecordWhatsNewSurveyStatus_EmitsHistogram);
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_WHATS_NEW_SURVEY_SERVICE_H_
