// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace privacy_sandbox {
class TrackingProtectionOnboarding;
}

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
  EligibilityService(Profile* profile, ExperimentManager* experiment_manager);
  EligibilityService(const EligibilityService&) = delete;
  EligibilityService& operator=(const EligibilityService&) = delete;
  ~EligibilityService() override;

  static EligibilityService* Get(Profile* profile);

  void Shutdown() override;

 private:
  // So EligibilityServiceFactory::BuildServiceInstanceFor can call the
  // constructor.
  friend class EligibilityServiceFactory;
  friend class EligibilityServiceBrowserTest;

  // MarkProfileEligibility should be called for all profiles to set their
  // eligibility, whether currently loaded or created later.
  void MarkProfileEligibility(bool is_client_eligible);
  void BroadcastProfileEligibility();
  bool IsProfileEligible();

  raw_ptr<Profile> profile_;
  // onboarding_service_ may be null for OTR and system profiles.
  raw_ptr<privacy_sandbox::TrackingProtectionOnboarding> onboarding_service_;
  // `ExperimentManager` is a singleton and lives forever.
  raw_ptr<ExperimentManager> experiment_manager_;
  bool is_profile_eligible_ = false;

  base::WeakPtrFactory<EligibilityService> weak_factory_{this};
};

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_ELIGIBILITY_SERVICE_H_
