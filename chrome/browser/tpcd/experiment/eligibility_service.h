// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace privacy_sandbox {
class PrivacySandboxSettings;
class TrackingProtectionOnboarding;
}  // namespace privacy_sandbox

namespace tpcd::experiment {

class ExperimentManager;

enum class ProfileEligibilityMismatch {
  kEligibleProfileInExperiment = 0,
  kIneligibleProfileNotInExperiment = 1,
  kIneligibleProfileInExperiment = 2,
  kEligibleProfileNotInExperiment = 3,
  kMaxValue = kEligibleProfileNotInExperiment,
};

const char ProfileEligibilityMismatchHistogramName[] =
    "Privacy.3pcd.ProfileEligibilityMismatch";

class EligibilityService : public KeyedService {
 public:
  EligibilityService(
      Profile* profile,
      privacy_sandbox::TrackingProtectionOnboarding*
          tracking_protection_onboarding,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      ExperimentManager* experiment_manager);
  EligibilityService(const EligibilityService&) = delete;
  EligibilityService& operator=(const EligibilityService&) = delete;
  ~EligibilityService() override;

  static EligibilityService* Get(Profile* profile);

  // KeyedService:
  void Shutdown() override;

 private:
  // So EligibilityServiceFactory::BuildServiceInstanceFor can call the
  // constructor.
  friend class EligibilityServiceFactory;
  friend class EligibilityServiceBrowserTestBase;

  // MarkProfileEligibility should be called for all profiles to set their
  // eligibility, whether currently loaded or created later.
  void MarkProfileEligibility(bool is_client_eligible);
  void BroadcastProfileEligibility();
  privacy_sandbox::TpcdExperimentEligibility ProfileEligibility();
  void UpdateCookieDeprecationLabel();
  void MaybeNotifyManagerTrackingProtectionOnboarded(
      privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus
          onboarding_status);
  void MaybeNotifyManagerTrackingProtectionSilentOnboarded(
      privacy_sandbox::TrackingProtectionOnboarding::SilentOnboardingStatus
          onboarding_status);

  raw_ptr<Profile> profile_;
  // `onboarding_service_` may be null for OTR and system profiles.
  raw_ptr<privacy_sandbox::TrackingProtectionOnboarding> onboarding_service_;
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  // `experiment_manager_` is a singleton and lives forever.
  raw_ptr<ExperimentManager> experiment_manager_;

  // Set in the constructor, it will always have a value past that point. An
  // optional is used since the user preferences are sometimes reset before
  // setting the `profile_eligibility_`.
  std::optional<privacy_sandbox::TpcdExperimentEligibility>
      profile_eligibility_;

  base::WeakPtrFactory<EligibilityService> weak_factory_{this};
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
