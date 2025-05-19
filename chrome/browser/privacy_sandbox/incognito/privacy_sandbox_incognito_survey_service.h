// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"

namespace privacy_sandbox {

// A service responsible for managing Privacy Sandbox surveys in Incognito.
class PrivacySandboxIncognitoSurveyService : public KeyedService {
 public:
  // Records the survey's status when attempting to surface the ACT survey.
  //
  // LINT.IfChange(PrivacySandboxActSurveyStatus)
  enum class ActSurveyStatus {
    kSurveyShown = 0,          // Survey was successfully shown.
    kFeatureDisabled = 1,      // ACT Survey feature disabled.
    kHatsServiceFailed = 2,    // Could not initialize HaTS service.
    kSurveyLaunchFailed = 3,   // Survey failed to launch.
    kNonIncognitoProfile = 4,  // Not inside Incognito.
    kMaxValue = kNonIncognitoProfile,
  };
  // LINT.ThenChange(/tools/metrics/histograms/enums.xml:PrivacySandboxActSurveyStatus)

  using RandIntCallback = base::RepeatingCallback<int(int, int)>;

  explicit PrivacySandboxIncognitoSurveyService(HatsService* hats_service,
                                                bool is_incognito);
  ~PrivacySandboxIncognitoSurveyService() override;

  // Called to surface the ACT survey if the conditions are met.
  void MaybeShowActSurvey(content::WebContents* web_contents);

  // Set a custom RandInt callback for testing.
  void SetRandIntCallbackForTesting(const RandIntCallback&& rand_int_callback);

 private:
  // Determines if the ACT survey is enabled.
  bool IsActSurveyEnabled();

  // Calculates the delay of the ACT survey based on feature params. The delay
  // is the time between triggering the survey and launching it. Note that
  // survey loading time is not included in the delay.
  base::TimeDelta CalculateActSurveyDelay();

  // Construct product specific string data for the ACT survey.
  std::map<std::string, std::string> GetActSurveyPsd(int delay_ms);

  // Emits the given ACT survey status to UMA.
  void RecordActSurveyStatus(ActSurveyStatus status);

  void OnActSurveyShown();
  void OnActSurveyFailure();

  RandIntCallback rand_int_callback_;
  const bool is_incognito_;
  raw_ptr<HatsService> hats_service_;
  base::WeakPtrFactory<PrivacySandboxIncognitoSurveyService> weak_ptr_factory_{
      this};

  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxIncognitoSurveyServiceTest,
                           IsActSurveyEnabled_DisabledByDefault);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxIncognitoSurveyServiceTest,
                           RecordActSurveyStatus_EmitsHistogram);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxIncognitoSurveyServiceTest,
                           GetActSurveyPsd_ReturnsProperPsd);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxIncognitoSurveyServiceActSurveyDelayTest,
      CalculateActSurveyDelay_ProperlyCalculatesDelay);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxIncognitoSurveyServiceActSurveyDelayRandomizationTest,
      CalculateActSurveyDelay_ProperlyCalculatesRandomizedDelay);
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_SURVEY_SERVICE_H_
